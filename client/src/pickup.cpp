/*=================================================================================================
   Copyright (c) 2016 Joel de Guzman

   Licensed under a Creative Commons Attribution-ShareAlike 4.0 International.
   http://creativecommons.org/licenses/by-sa/4.0/
=================================================================================================*/
#include <elf/pickup.hpp>
#include <photon/view.hpp>
#include <cmath>

#include <iostream>

namespace elf
{
   using photon::full_extent;
   using photon::canvas;
   using photon::clamp_max;
   using photon::widget;
   using photon::clamp;

   float const scale_len = 0.8;

   namespace
   {
      canvas::state prepare(rect& bounds, float slant, canvas& canvas_)
      {
         auto state = canvas_.new_state();

         float l = bounds.left;
         float t = bounds.top;
         float w = bounds.width();
         float h = bounds.height();

         canvas_.translate({ l+(w/2), t+(h/2) });
         bounds = bounds.move(-(l+(w/2)), -(t+(h/2)));
         canvas_.rotate(slant);
         return state;
      }

      bool hit_test_pickup(rect bounds, float slant, point mp, canvas& canvas_)
      {
         auto state = prepare(bounds, slant, canvas_);

         canvas_.begin_path();
         canvas_.round_rect(bounds, bounds.width()/2);
         return canvas_.hit_test(canvas_.transform_point(mp));
      }

      void draw_pickup(rect bounds, float slant, bool hilite, context const& ctx)
      {
         auto  canvas_ = ctx.canvas();
         auto& theme = ctx.theme();
         auto  state = prepare(bounds, slant, canvas_);

         auto outline_color = theme.frame_color;
         auto glow_color = theme.indicator_color;
         auto fill_color = theme.controls_color;

         // Fill
         canvas_.fill_style(fill_color);
         canvas_.fill_round_rect(bounds, bounds.width()/2);

         if (hilite)
         {
            outline_color = outline_color.opacity(1).level(0.8);
            glow_color = glow_color.opacity(1).level(1.5);
         }

         // Glow
         auto  alpha = glow_color.alpha;
         auto  radius = bounds.width()/2;

         canvas_.line_width(4);
         canvas_.stroke_style(glow_color.opacity(alpha * 0.2));
         canvas_.stroke_round_rect(bounds, radius);

         canvas_.line_width(3);
         canvas_.stroke_style(glow_color.opacity(alpha * 0.4));
         canvas_.stroke_round_rect(bounds, radius);

         canvas_.line_width(2);
         canvas_.stroke_style(glow_color.opacity(alpha * 0.7));
         canvas_.stroke_round_rect(bounds, radius);

         // Outline
         canvas_.line_width(1);
         canvas_.stroke_style(outline_color);
         canvas_.stroke_round_rect(bounds, radius);
      }
   }

   rect pickup::limits(basic_context const& ctx) const
   {
      return { 400, 100, full_extent, full_extent };
   }

   void pickup::draw(context const& ctx)
   {
      rect  r1, r2;
      pickup_bounds(ctx, r1, r2);

      auto  mp = ctx.cursor_pos();
      auto  canvas_ = ctx.canvas();

      if (_type == single)
      {
         bool hilite = hit_test_pickup(r1, _slant, mp, canvas_);
         draw_pickup(r1, _slant, hilite, ctx);
      }
      else
      {
         bool hilite =
            hit_test_pickup(r1, _slant, mp, canvas_) ||
            hit_test_pickup(r2, _slant, mp, canvas_)
            ;
         draw_pickup(r1, _slant, hilite, ctx);
         draw_pickup(r2, _slant, hilite, ctx);
      }
   }

   widget* pickup::hit_test(context const& ctx, point p)
   {
      return hit(ctx, p) ? this : nullptr;
   }

   bool pickup::cursor(context const& ctx, point p, cursor_tracking status)
   {
      ctx.view.refresh(ctx.bounds);
      return true;
   }

   bool pickup::reposition(context const& ctx, point mp)
   {
      rect r1, r2;
      pickup_bounds(ctx, r1, r2);
      rect pu_bounds = (_type == single) ? r1 : rect{ r1.left, r1.top, r2.right, r2.bottom };

      if (_tracking == start)
      {
         hit_item what = hit(ctx, mp);
         if (what == hit_pickup)
         {
            // start tracking move
            _tracking = tracking_move;
            _offset = point{ mp.x-pu_bounds.left, mp.y-pu_bounds.top };
         }
         else if (what == hit_rotator)
         {
            // start tracking rotate
            _tracking = tracking_rotate;
            _offset = point{ mp.x-_rotator_pos.x, mp.y-_rotator_pos.y };
            //if (btn.num_clicks == 2)
            //{
            //   _slant = 0;
            //   ctx.view.refresh(ctx.bounds);
            //   return true;
            //}
         }
      }

      // continue tracking move
      if (_tracking == tracking_move)
      {
         float w = (ctx.bounds.width() * scale_len) + pu_bounds.width();
         float offs = (ctx.bounds.width() - w) / 2;
         mp.x -= _offset.x + ctx.bounds.left + offs;

         double align = mp.x / (w - pu_bounds.width());
         clamp(align, 0.0, 1.0);
         _pos = align;
         ctx.view.refresh(ctx.bounds);
         return true;
      }

      // continue tracking rotate
      if (_tracking == tracking_rotate)
      {
         mp = mp.move(-_offset.x, -_offset.y);
         auto center = center_point(pu_bounds);
         float angle = -std::atan2(mp.x-center.x, mp.y-center.y);

         clamp(angle, -0.4, 0.4);
         _slant = angle;
         ctx.view.refresh(ctx.bounds);
         return true;
      }

      return false;
   }

   pickup::hit_item pickup::hit(context const& ctx, point p) const
   {
      rect  r1, r2;
      pickup_bounds(ctx, r1, r2);
      rect  r = r1;
      if (_type == double_)
         r.right = r2.right;

      auto  canvas_ = ctx.canvas();

      if (_type == single)
      {
         if (hit_test_pickup(r1, _slant, p, canvas_))
            return hit_pickup;
      }
      else
      {
         if (hit_test_pickup(r1, _slant, ctx.cursor_pos(), canvas_) ||
            hit_test_pickup(r2, _slant, ctx.cursor_pos(), canvas_))
            return hit_pickup;
      }

//      if (hit_test_rotator(r, _slant, p, thm))
//         return hit_rotator;

      return hit_none;
   }

   void pickup::pickup_bounds(context const& ctx, rect& r1, rect& r2) const
   {
      auto        bounds = ctx.bounds;
      float       w = bounds.width() * scale_len;
      float       h = w * 0.19;

      clamp_max(h, ctx.bounds.height() * 0.8); // 0.8 to accomodate the rotator

      float pu_w = h * 0.25;
      rect  pu_bounds  = { 0, 0, pu_w, h };
      if (_type == double_)
         pu_bounds.right += pu_w + 4;

      rect  active_bounds = { 0, 0, w + pu_bounds.width(), h };
      active_bounds = center(active_bounds, bounds);
      pu_bounds = align(pu_bounds, active_bounds, _pos, 0.5);

      if (_type == single)
      {
         r1 = pu_bounds;
      }
      else
      {
         r2 = r1 = pu_bounds;
         r1.right = r1.left + pu_w;
         r2.left = r2.right - pu_w;
      }
   }

   void pickup::begin_tracking(context const& ctx, info& track_info)
   {
      _tracking = start;
      reposition(ctx, track_info.current);
   }

   void pickup::keep_tracking(context const& ctx, info& track_info)
   {
      if (_tracking == tracking_move || _tracking == tracking_rotate)
         reposition(ctx, track_info.current);
   }

   void pickup::end_tracking(context const& ctx, info& track_info)
   {
      _tracking = none;
   }
}
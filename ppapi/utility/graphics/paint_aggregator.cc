// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/utility/graphics/paint_aggregator.h"

#include <stdint.h>

#include <algorithm>

#include "ppapi/cpp/logging.h"

// ----------------------------------------------------------------------------
// ALGORITHM NOTES
//
// We attempt to maintain a scroll rect in the presence of invalidations that
// are contained within the scroll rect.  If an invalidation crosses a scroll
// rect, then we just treat the scroll rect as an invalidation rect.
//
// For invalidations performed prior to scrolling and contained within the
// scroll rect, we offset the invalidation rects to account for the fact that
// the consumer will perform scrolling before painting.
//
// We only support scrolling along one axis at a time.  A diagonal scroll will
// therefore be treated as an invalidation.
// ----------------------------------------------------------------------------

namespace pp {

PaintAggregator::PaintUpdate::PaintUpdate() : has_scroll(false) {}

PaintAggregator::PaintUpdate::~PaintUpdate() {}

PaintAggregator::InternalPaintUpdate::InternalPaintUpdate() {}

PaintAggregator::InternalPaintUpdate::~InternalPaintUpdate() {}

Rect PaintAggregator::InternalPaintUpdate::GetScrollDamage() const {
  // Should only be scrolling in one direction at a time.
  PP_DCHECK(!(scroll_delta.x() && scroll_delta.y()));

  Rect damaged_rect;

  // Compute the region we will expose by scrolling, and paint that into a
  // shared memory section.
  if (scroll_delta.x()) {
    int32_t dx = scroll_delta.x();
    damaged_rect.set_y(scroll_rect.y());
    damaged_rect.set_height(scroll_rect.height());
    if (dx > 0) {
      damaged_rect.set_x(scroll_rect.x());
      damaged_rect.set_width(dx);
    } else {
      damaged_rect.set_x(scroll_rect.right() + dx);
      damaged_rect.set_width(-dx);
    }
  } else {
    int32_t dy = scroll_delta.y();
    damaged_rect.set_x(scroll_rect.x());
    damaged_rect.set_width(scroll_rect.width());
    if (dy > 0) {
      damaged_rect.set_y(scroll_rect.y());
      damaged_rect.set_height(dy);
    } else {
      damaged_rect.set_y(scroll_rect.bottom() + dy);
      damaged_rect.set_height(-dy);
    }
  }

  // In case the scroll offset exceeds the width/height of the scroll rect
  return scroll_rect.Intersect(damaged_rect);
}

Rect PaintAggregator::InternalPaintUpdate::GetPaintBounds() const {
  Rect bounds;
  for (size_t i = 0; i < paint_rects.size(); ++i)
    bounds = bounds.Union(paint_rects[i]);
  return bounds;
}

PaintAggregator::PaintAggregator()
    : max_redundant_paint_to_scroll_area_(0.8f),
      max_paint_rects_(10) {
}

bool PaintAggregator::HasPendingUpdate() const {
  return !update_.scroll_rect.IsEmpty() || !update_.paint_rects.empty();
}

void PaintAggregator::ClearPendingUpdate() {
  update_ = InternalPaintUpdate();
}

PaintAggregator::PaintUpdate PaintAggregator::GetPendingUpdate() const {
  // Convert the internal paint update to the external one, which includes a
  // bit more precomputed info for the caller.
  PaintUpdate ret;
  ret.scroll_delta = update_.scroll_delta;
  ret.scroll_rect = update_.scroll_rect;
  ret.has_scroll = ret.scroll_delta.x() != 0 || ret.scroll_delta.y() != 0;

  ret.paint_rects.reserve(update_.paint_rects.size() + 1);
  for (size_t i = 0; i < update_.paint_rects.size(); i++)
    ret.paint_rects.push_back(update_.paint_rects[i]);

  ret.paint_bounds = update_.GetPaintBounds();

  // Also include the scroll damage (if any) in the paint rects.
  if (ret.has_scroll) {
    PP_Rect scroll_damage = update_.GetScrollDamage();
    ret.paint_rects.push_back(scroll_damage);
    ret.paint_bounds = ret.paint_bounds.Union(scroll_damage);
  }

  return ret;
}

void PaintAggregator::InvalidateRect(const Rect& rect) {
  // Combine overlapping paints using smallest bounding box.
  for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
    const Rect& existing_rect = update_.paint_rects[i];
    if (existing_rect.Contains(rect))  // Optimize out redundancy.
      return;
    if (rect.Intersects(existing_rect) || rect.SharesEdgeWith(existing_rect)) {
      // Re-invalidate in case the union intersects other paint rects.
      Rect combined_rect = existing_rect.Union(rect);
      update_.paint_rects.erase(update_.paint_rects.begin() + i);
      InvalidateRect(combined_rect);
      return;
    }
  }

  // Add a non-overlapping paint.
  update_.paint_rects.push_back(rect);

  // If the new paint overlaps with a scroll, then it forces an invalidation of
  // the scroll.  If the new paint is contained by a scroll, then trim off the
  // scroll damage to avoid redundant painting.
  if (!update_.scroll_rect.IsEmpty()) {
    if (ShouldInvalidateScrollRect(rect)) {
      InvalidateScrollRect();
    } else if (update_.scroll_rect.Contains(rect)) {
      update_.paint_rects.back() = rect.Subtract(update_.GetScrollDamage());
      if (update_.paint_rects.back().IsEmpty())
        update_.paint_rects.pop_back();
    }
  }

  if (update_.paint_rects.size() > max_paint_rects_)
    CombinePaintRects();
}

void PaintAggregator::ScrollRect(const Rect& clip_rect, const Point& amount) {
  // We only support scrolling along one axis at a time.
  if (amount.x() != 0 && amount.y() != 0) {
    InvalidateRect(clip_rect);
    return;
  }

  // We can only scroll one rect at a time.
  if (!update_.scroll_rect.IsEmpty() && update_.scroll_rect != clip_rect) {
    InvalidateRect(clip_rect);
    return;
  }

  // Again, we only support scrolling along one axis at a time.  Make sure this
  // update doesn't scroll on a different axis than any existing one.
  if ((amount.x() && update_.scroll_delta.y()) ||
      (amount.y() && update_.scroll_delta.x())) {
    InvalidateRect(clip_rect);
    return;
  }

  // The scroll rect is new or isn't changing (though the scroll amount may
  // be changing).
  update_.scroll_rect = clip_rect;
  update_.scroll_delta += amount;

  // We might have just wiped out a pre-existing scroll.
  if (update_.scroll_delta == Point()) {
    update_.scroll_rect = Rect();
    return;
  }

  // Adjust any contained paint rects and check for any overlapping paints.
  for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
    if (update_.scroll_rect.Contains(update_.paint_rects[i])) {
      update_.paint_rects[i] = ScrollPaintRect(update_.paint_rects[i], amount);
      // The rect may have been scrolled out of view.
      if (update_.paint_rects[i].IsEmpty()) {
        update_.paint_rects.erase(update_.paint_rects.begin() + i);
        i--;
      }
    } else if (update_.scroll_rect.Intersects(update_.paint_rects[i])) {
      InvalidateScrollRect();
      return;
    }
  }

  // If the new scroll overlaps too much with contained paint rects, then force
  // an invalidation of the scroll.
  if (ShouldInvalidateScrollRect(Rect()))
    InvalidateScrollRect();
}

Rect PaintAggregator::ScrollPaintRect(const Rect& paint_rect,
                                      const Point& amount) const {
  Rect result = paint_rect;

  result.Offset(amount);
  result = update_.scroll_rect.Intersect(result);

  // Subtract out the scroll damage rect to avoid redundant painting.
  return result.Subtract(update_.GetScrollDamage());
}

bool PaintAggregator::ShouldInvalidateScrollRect(const Rect& rect) const {
  if (!rect.IsEmpty()) {
    if (!update_.scroll_rect.Intersects(rect))
      return false;

    if (!update_.scroll_rect.Contains(rect))
      return true;
  }

  // Check if the combined area of all contained paint rects plus this new
  // rect comes too close to the area of the scroll_rect.  If so, then we
  // might as well invalidate the scroll rect.

  int paint_area = rect.size().GetArea();
  for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
    const Rect& existing_rect = update_.paint_rects[i];
    if (update_.scroll_rect.Contains(existing_rect))
      paint_area += existing_rect.size().GetArea();
  }
  int scroll_area = update_.scroll_rect.size().GetArea();
  return static_cast<float>(paint_area) / static_cast<float>(scroll_area) >
         max_redundant_paint_to_scroll_area_;
}

void PaintAggregator::InvalidateScrollRect() {
  Rect scroll_rect = update_.scroll_rect;
  update_.scroll_rect = Rect();
  update_.scroll_delta = Point();
  InvalidateRect(scroll_rect);
}

void PaintAggregator::CombinePaintRects() {
  // Combine paint rects down to at most two rects: one inside the scroll_rect
  // and one outside the scroll_rect.  If there is no scroll_rect, then just
  // use the smallest bounding box for all paint rects.
  //
  // NOTE: This is a fairly simple algorithm.  We could get fancier by only
  // combining two rects to get us under the max_paint_rects limit, but if we
  // reach this method then it means we're hitting a rare case, so there's no
  // need to over-optimize it.
  //
  if (update_.scroll_rect.IsEmpty()) {
    Rect bounds = update_.GetPaintBounds();
    update_.paint_rects.clear();
    update_.paint_rects.push_back(bounds);
  } else {
    Rect inner, outer;
    for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
      const Rect& existing_rect = update_.paint_rects[i];
      if (update_.scroll_rect.Contains(existing_rect)) {
        inner = inner.Union(existing_rect);
      } else {
        outer = outer.Union(existing_rect);
      }
    }
    update_.paint_rects.clear();
    update_.paint_rects.push_back(inner);
    update_.paint_rects.push_back(outer);
  }
}

}  // namespace pp

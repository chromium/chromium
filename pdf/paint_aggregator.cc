// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_aggregator.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

namespace {

bool IsNegative(int32_t num) {
  return num < 0;
}

}  // namespace

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

PaintAggregator::PaintUpdate::PaintUpdate() = default;

PaintAggregator::PaintUpdate::PaintUpdate(const PaintUpdate& that) = default;

PaintAggregator::PaintUpdate::~PaintUpdate() = default;

PaintAggregator::InternalPaintUpdate::InternalPaintUpdate()
    : synthesized_scroll_damage_rect_(false) {}

PaintAggregator::InternalPaintUpdate::~InternalPaintUpdate() = default;

gfx::Rect PaintAggregator::InternalPaintUpdate::GetScrollDamage() const {
  // Should only be scrolling in one direction at a time.
  DCHECK(!(scroll_delta.x() && scroll_delta.y()));

  gfx::Rect damaged_rect;

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
  return gfx::IntersectRects(scroll_rect, damaged_rect);
}

PaintAggregator::PaintAggregator() = default;

bool PaintAggregator::HasPendingUpdate() const {
  return !update_.scroll_rect.IsEmpty() || !update_.paint_rects.empty();
}

void PaintAggregator::ClearPendingUpdate() {
  update_ = InternalPaintUpdate();
}

PaintAggregator::PaintUpdate PaintAggregator::GetPendingUpdate() {
  // Convert the internal paint update to the external one, which includes a
  // bit more precomputed info for the caller.
  PaintUpdate ret;
  ret.scroll_delta = update_.scroll_delta;
  ret.scroll_rect = update_.scroll_rect;
  ret.has_scroll = ret.scroll_delta.x() != 0 || ret.scroll_delta.y() != 0;

  // Include the scroll damage (if any) in the paint rects.
  // Code invalidates damaged rect here, it pick it up from the list of paint
  // rects in the next block.
  if (ret.has_scroll && !update_.synthesized_scroll_damage_rect_) {
    update_.synthesized_scroll_damage_rect_ = true;
    gfx::Rect scroll_damage = update_.GetScrollDamage();
    InvalidateRectInternal(scroll_damage, false);
  }

  ret.paint_rects.reserve(update_.paint_rects.size() + 1);
  ret.paint_rects.insert(ret.paint_rects.end(), update_.paint_rects.begin(),
                         update_.paint_rects.end());

  return ret;
}

void PaintAggregator::SetIntermediateResults(
    const std::vector<PaintReadyRect>& ready,
    const std::vector<gfx::Rect>& pending) {
  update_.ready_rects.insert(update_.ready_rects.end(), ready.begin(),
                             ready.end());
  update_.paint_rects = pending;
}

std::vector<PaintReadyRect> PaintAggregator::GetReadyRects() const {
  return update_.ready_rects;
}

void PaintAggregator::InvalidateRect(const gfx::Rect& rect) {
  InvalidateRectInternal(rect, true);
}

void PaintAggregator::ScrollRect(const gfx::Rect& clip_rect,
                                 const gfx::Vector2d& amount) {
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

  // If we scroll in a reverse direction to the direction we originally scrolled
  // and there were invalidations that happened in-between we may end up
  // incorrectly clipping the invalidated rects (see crbug.com/488390). This bug
  // doesn't exist in the original implementation
  // (ppapi/utility/graphics/paint_aggregator.cc) which uses a different method
  // of handling invalidations that occur after a scroll. The problem is that
  // when we scroll the invalidated region, we clip it to the scroll rect. This
  // can cause us to lose information about what the invalidated region was if
  // it gets scrolled back into view. We either need to not do this clipping or
  // disallow combining scrolls that occur in different directions with
  // invalidations that happen in-between. This code really needs some tests...
  if (!update_.paint_rects.empty()) {
    if (IsNegative(amount.x()) != IsNegative(update_.scroll_delta.x()) ||
        IsNegative(amount.y()) != IsNegative(update_.scroll_delta.y())) {
      InvalidateRect(clip_rect);
      return;
    }
  }

  // The scroll rect is new or isn't changing (though the scroll amount may
  // be changing).
  update_.scroll_rect = clip_rect;
  update_.scroll_delta += amount;

  // We might have just wiped out a pre-existing scroll.
  if (update_.scroll_delta == gfx::Vector2d()) {
    update_.scroll_rect = gfx::Rect();
    return;
  }

  // Adjust any paint rects that intersect the scroll. For the portion of the
  // paint that is inside the scroll area, move it by the scroll amount and
  // replace the existing paint with it. For the portion (if any) that is
  // outside the scroll, just invalidate it.
  std::vector<gfx::Rect> leftover_rects;
  for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
    if (!update_.scroll_rect.Intersects(update_.paint_rects[i]))
      continue;

    gfx::Rect intersection =
        gfx::IntersectRects(update_.paint_rects[i], update_.scroll_rect);
    gfx::Rect rect = update_.paint_rects[i];
    while (!rect.IsEmpty()) {
      gfx::Rect leftover = gfx::SubtractRects(rect, intersection);
      if (leftover.IsEmpty())
        break;
      // Don't want to call InvalidateRectInternal now since it'll modify
      // update_.paint_rects, so keep track of this and do it below.
      leftover_rects.push_back(leftover);
      rect.Subtract(leftover);
    }

    update_.paint_rects[i] = ScrollPaintRect(intersection, amount);

    // The rect may have been scrolled out of view.
    if (update_.paint_rects[i].IsEmpty()) {
      update_.paint_rects.erase(update_.paint_rects.begin() + i);
      i--;
    }
  }

  for (const auto& leftover_rect : leftover_rects)
    InvalidateRectInternal(leftover_rect, false);

  for (auto& update_rect : update_.ready_rects) {
    if (update_.scroll_rect.Contains(update_rect.rect()))
      update_rect.set_rect(ScrollPaintRect(update_rect.rect(), amount));
  }

  if (update_.synthesized_scroll_damage_rect_) {
    InvalidateRect(update_.GetScrollDamage());
  }
}

gfx::Rect PaintAggregator::ScrollPaintRect(const gfx::Rect& paint_rect,
                                           const gfx::Vector2d& amount) const {
  gfx::Rect result = paint_rect + amount;
  result.Intersect(update_.scroll_rect);
  return result;
}

void PaintAggregator::InvalidateScrollRect() {
  gfx::Rect scroll_rect = update_.scroll_rect;
  update_.scroll_rect = gfx::Rect();
  update_.scroll_delta = gfx::Vector2d();
  InvalidateRect(scroll_rect);
}

void PaintAggregator::InvalidateRectInternal(const gfx::Rect& rect_old,
                                             bool check_scroll) {
  gfx::Rect rect = rect_old;
  // Check if any rects that are ready to be painted overlap.
  for (size_t i = 0; i < update_.ready_rects.size(); ++i) {
    const gfx::Rect& existing_rect = update_.ready_rects[i].rect();
    if (rect.Intersects(existing_rect)) {
      // Re-invalidate in case the union intersects other paint rects.
      rect.Union(existing_rect);
      update_.ready_rects.erase(update_.ready_rects.begin() + i);
      break;
    }
  }

  bool add_paint = true;

  // Combine overlapping paints using smallest bounding box.
  for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
    const gfx::Rect& existing_rect = update_.paint_rects[i];
    if (existing_rect.Contains(rect))  // Optimize out redundancy.
      add_paint = false;
    if (rect.Intersects(existing_rect) || rect.SharesEdgeWith(existing_rect)) {
      // Re-invalidate in case the union intersects other paint rects.
      gfx::Rect combined_rect = gfx::UnionRects(rect, existing_rect);
      update_.paint_rects.erase(update_.paint_rects.begin() + i);
      InvalidateRectInternal(combined_rect, check_scroll);
      add_paint = false;
    }
  }

  if (add_paint) {
    // Add a non-overlapping paint.
    update_.paint_rects.push_back(rect);
  }

  // If the new paint overlaps with a scroll, then also invalidate the rect in
  // its new position.
  if (check_scroll && !update_.scroll_rect.IsEmpty() &&
      update_.scroll_rect.Intersects(rect)) {
    InvalidateRectInternal(ScrollPaintRect(rect, update_.scroll_delta), false);
  }
}

}  // namespace chrome_pdf

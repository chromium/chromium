// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_FLOAT_CLIP_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_FLOAT_CLIP_RECT_H_

#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT FloatClipRect {
  USING_FAST_MALLOC(FloatClipRect);

 public:
  FloatClipRect()
      : rect_(FloatRect(LayoutRect::InfiniteIntRect())),
        has_radius_(false),
        is_tight_(true),
        is_infinite_(true) {}

  explicit FloatClipRect(const FloatRect& rect) { SetRect(rect); }

  explicit FloatClipRect(const FloatRoundedRect& rect)
      : rect_(rect.Rect()),
        has_radius_(rect.IsRounded()),
        is_tight_(!rect.IsRounded()),
        is_infinite_(false) {}

  const FloatRect& Rect() const { return rect_; }

  FloatRect& Rect() { return rect_; }

  void SetRect(const FloatRect& rect) {
    rect_ = rect;
    has_radius_ = false;
    is_tight_ = true;
    is_infinite_ = false;
  }

  void Intersect(const FloatClipRect& other) {
    if (other.is_infinite_)
      return;

    if (is_infinite_) {
      is_infinite_ = other.is_infinite_;
      rect_ = other.rect_;
    } else {
      rect_.Intersect(other.Rect());
    }
    if (other.HasRadius())
      SetHasRadius();
    else if (!other.IsTight())
      ClearIsTight();
  }

  // See FloatRect::InclusiveIntersect for description of the return value.
  // TL;DR, this returns true if rects actually intersect.
  bool InclusiveIntersect(const FloatClipRect& other) {
    if (other.is_infinite_)
      return true;

    bool retval = true;
    if (is_infinite_) {
      is_infinite_ = other.is_infinite_;
      rect_ = other.rect_;
    } else {
      retval = rect_.InclusiveIntersect(other.Rect());
    }
    if (other.HasRadius())
      SetHasRadius();
    else if (!other.IsTight())
      ClearIsTight();
    return retval;
  }

  bool HasRadius() const { return has_radius_; }
  void SetHasRadius() {
    has_radius_ = true;
    is_tight_ = false;
    is_infinite_ = false;
  }

  // The rect is tight means that the rect covers only clipped result and
  // nothing else.
  bool IsTight() const {
    DCHECK(!is_tight_ || !has_radius_);
    return is_tight_;
  }
  void ClearIsTight() { is_tight_ = false; }

  void MoveBy(const FloatPoint& offset) {
    if (is_infinite_)
      return;
    rect_.MoveBy(offset);
  }

  // Assumes that the transform always makes the clip rect not tight. The caller
  // should use MoveBy() to keep tightness if the transform is known to be
  // identity or a 2d translation.
  void Map(const TransformationMatrix& matrix) {
    is_tight_ = false;
    if (is_infinite_)
      return;
    rect_ = matrix.MapRect(rect_);
  }

  bool IsInfinite() const { return is_infinite_; }

 private:
  FloatRect rect_;
  bool has_radius_ : 1;
  bool is_tight_ : 1;
  bool is_infinite_ : 1;
};

inline bool operator==(const FloatClipRect& a, const FloatClipRect& b) {
  if (a.IsTight() != b.IsTight())
    return false;
  if (a.IsInfinite() && b.IsInfinite())
    return true;
  return !a.IsInfinite() && !b.IsInfinite() && a.HasRadius() == b.HasRadius() &&
         a.Rect() == b.Rect();
}

inline bool operator!=(const FloatClipRect& a, const FloatClipRect& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_FLOAT_CLIP_RECT_H_

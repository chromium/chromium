// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CULL_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CULL_RECT_H_

#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <limits>

namespace blink {

class AffineTransform;
class FloatRect;
class LayoutRect;
class LayoutUnit;
class TransformPaintPropertyNode;

class PLATFORM_EXPORT CullRect {
  DISALLOW_NEW();

 public:
  CullRect() = default;
  explicit CullRect(const IntRect& rect) : rect_(rect) {}

  static CullRect Infinite() { return CullRect(LayoutRect::InfiniteIntRect()); }

  bool IsInfinite() const { return rect_ == LayoutRect::InfiniteIntRect(); }

  bool Intersects(const IntRect&) const;
  bool IntersectsTransformed(const AffineTransform&, const FloatRect&) const;
  bool IntersectsHorizontalRange(LayoutUnit lo, LayoutUnit hi) const;
  bool IntersectsVerticalRange(LayoutUnit lo, LayoutUnit hi) const;

  void MoveBy(const IntPoint& offset);
  void Move(const IntSize& offset);
  void Move(const FloatSize& offset);

  // Applies one transform to the cull rect. Before this function is called,
  // the cull rect is in the space of the parent the transform node.
  // For CompositeAfterPaint, when the transform is a scroll translation, the
  // cull rect is converted in the following steps:
  // 1. it's clipped by the container rect,
  // 2. transformed by inverse of the scroll translation,
  // 3. expanded by thousands of pixels for composited scrolling.
  void ApplyTransform(const TransformPaintPropertyNode& transform) {
    ApplyTransformInternal(transform);
  }

  // For CompositeAfterPaint only. Applies transforms from |source| (not
  // included) to |destination| (included). For each scroll translation, the
  // cull rect is converted as described in ApplyTransform(). If |old_cull_rect|
  // is provided, and the cull rect converted by the last scroll translation
  // doesn't cover the whole scrolling contents, and the new cull rect doesn't
  // change enough (by hundreds of pixels) from |old_cull_rect|, the cull rect
  // will be set to |old_cull_rect| to avoid repaint on each composited scroll.
  void ApplyTransforms(const TransformPaintPropertyNode& source,
                       const TransformPaintPropertyNode& destination,
                       const base::Optional<CullRect>& old_cull_rect);

  const IntRect& Rect() const { return rect_; }

  String ToString() const { return rect_.ToString(); }

 private:
  friend class CullRectTest;

  enum ApplyTransformResult {
    // The cull rect is transformed into the target transform space (by mapping
    // the cull rect with the inverse of the transform) without expansion.
    // In SlimmingPaintV1, the functions always return this value.
    kNotExpanded,
    // The cull rect is converted by a scroll translation (in the steps
    // described in ApplyTransform(), and the result covers the whole scrolling
    // contents.
    kExpandedForWholeScrollingContents,
    // The cull rect is converted by a scroll translation, and the result
    // doesn't cover the whole scrolling contents.
    kExpandedForPartialScrollingContents,
  };
  ApplyTransformResult ApplyTransformInternal(
      const TransformPaintPropertyNode&);

  bool ChangedEnough(const CullRect& old_cull_rect) const;

  IntRect rect_;
};

inline bool operator==(const CullRect& a, const CullRect& b) {
  return a.Rect() == b.Rect();
}
inline bool operator!=(const CullRect& a, const CullRect& b) {
  return !(a == b);
}

inline std::ostream& operator<<(std::ostream& os, const CullRect& cull_rect) {
  return os << cull_rect.Rect();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CULL_RECT_H_

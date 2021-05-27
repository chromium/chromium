// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CULL_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CULL_RECT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
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
class PropertyTreeState;
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
  void ApplyTransform(const TransformPaintPropertyNode&);

  // For CullRectUpdate only. Similar to the above but also applies clips and
  // expands for all directly composited transforms (including scrolling and
  // non-scrolling ones). |root| is used to calculate the expansion distance in
  // the local space, to make the expansion distance approximately the same in
  // the root space.
  // Returns whether the cull rect has been expanded.
  bool ApplyPaintProperties(const PropertyTreeState& root,
                            const PropertyTreeState& source,
                            const PropertyTreeState& destination,
                            const absl::optional<CullRect>& old_cull_rect);

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
  ApplyTransformResult ApplyScrollTranslation(
      const TransformPaintPropertyNode& root_transform,
      const TransformPaintPropertyNode& scroll_translation);

  // Returns false if the rect is clipped to be invisible. Otherwise returns
  // true, even if the cull rect is empty due to a special 3d transform in case
  // later 3d transforms make the cull rect visible again.
  bool ApplyPaintPropertiesWithoutExpansion(
      const PropertyTreeState& source,
      const PropertyTreeState& destination);

  bool ChangedEnough(const CullRect& old_cull_rect,
                     const absl::optional<IntRect>& expansion_bounds) const;

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

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

bool CullRect::Intersects(const IntRect& rect) const {
  return IsInfinite() || rect.Intersects(rect_);
}

bool CullRect::Intersects(const LayoutRect& rect) const {
  return IsInfinite() || rect_.Intersects(EnclosingIntRect(rect));
}

bool CullRect::Intersects(const LayoutRect& rect,
                          const LayoutPoint& offset) const {
  return IsInfinite() || rect_.Intersects(EnclosingIntRect(LayoutRect(
                             rect.Location() + offset, rect.Size())));
}

bool CullRect::IntersectsTransformed(const AffineTransform& transform,
                                     const FloatRect& rect) const {
  return IsInfinite() || transform.MapRect(rect).Intersects(rect_);
}

bool CullRect::IntersectsHorizontalRange(LayoutUnit lo, LayoutUnit hi) const {
  return !(lo >= rect_.MaxX() || hi <= rect_.X());
}

bool CullRect::IntersectsVerticalRange(LayoutUnit lo, LayoutUnit hi) const {
  return !(lo >= rect_.MaxY() || hi <= rect_.Y());
}

void CullRect::MoveBy(const IntPoint& offset) {
  if (!IsInfinite())
    rect_.MoveBy(offset);
}

void CullRect::Move(const IntSize& offset) {
  if (!IsInfinite())
    rect_.Move(offset);
}

static void MapRect(const TransformPaintPropertyNode& transform,
                    IntRect& rect) {
  if (transform.IsIdentityOr2DTranslation()) {
    FloatRect float_rect(rect);
    float_rect.Move(-transform.Translation2D());
    rect = EnclosingIntRect(float_rect);
  } else {
    rect = transform.MatrixWithOriginApplied().Inverse().MapRect(rect);
  }
}

CullRect::ApplyTransformResult CullRect::ApplyTransformInternal(
    const TransformPaintPropertyNode& transform,
    bool clip_to_scroll_container) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (const auto* scroll = transform.ScrollNode()) {
      if (clip_to_scroll_container) {
        rect_.Intersect(scroll->ContainerRect());
        if (rect_.IsEmpty())
          return kNotExpanded;
      }

      MapRect(transform, rect_);

      // Don't expand for non-composited scrolling.
      if (!transform.HasDirectCompositingReasons())
        return kNotExpanded;

      // We create scroll node for the root scroller even it's not scrollable.
      // Don't expand in the case.
      if (scroll->ContainerRect().Width() >= scroll->ContentsSize().Width() &&
          scroll->ContainerRect().Height() >= scroll->ContentsSize().Height())
        return kNotExpanded;

      // Expand the cull rect for scrolling contents for composited scrolling.
      static const int kPixelDistanceToExpand = 4000;
      rect_.Inflate(kPixelDistanceToExpand);
      // Don't clip the cull rect by contents size to let ChangedEnough() work
      // even if the new cull rect exceeds the bounds of contents rect.
      return rect_.Contains(IntRect(IntPoint(), scroll->ContentsSize()))
                 ? kExpandedForWholeScrollingContents
                 : kExpandedForPartialScrollingContents;
    }
  }

  if (!IsInfinite())
    MapRect(transform, rect_);
  return kNotExpanded;
}

void CullRect::ApplyTransforms(const TransformPaintPropertyNode& source,
                               const TransformPaintPropertyNode& destination,
                               const base::Optional<CullRect>& old_cull_rect,
                               bool clip_to_scroll_container) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  Vector<const TransformPaintPropertyNode*> scroll_translations;
  for (const auto* t = &destination; t != &source; t = t->Parent()) {
    if (!t) {
      // |source| is not an ancestor of |destination|. Simply map.
      if (!IsInfinite())
        GeometryMapper::SourceToDestinationRect(source, destination, rect_);
      return;
    }
    if (t->ScrollNode())
      scroll_translations.push_back(t);
  }

  const auto* last_transform = &source;
  ApplyTransformResult last_scroll_translation_result = kNotExpanded;
  for (auto it = scroll_translations.rbegin(); it != scroll_translations.rend();
       ++it) {
    const auto* scroll_translation = *it;
    if (!IsInfinite()) {
      DCHECK(scroll_translation->Parent());
      GeometryMapper::SourceToDestinationRect(
          *last_transform, *scroll_translation->Parent(), rect_);
    }
    last_scroll_translation_result =
        ApplyTransformInternal(*scroll_translation, clip_to_scroll_container);
    last_transform = scroll_translation;
  }

  if (!IsInfinite()) {
    GeometryMapper::SourceToDestinationRect(*last_transform, destination,
                                            rect_);
  }

  if (last_scroll_translation_result == kExpandedForPartialScrollingContents &&
      old_cull_rect && !ChangedEnough(*old_cull_rect))
    rect_ = old_cull_rect->Rect();
}

bool CullRect::ChangedEnough(const CullRect& old_cull_rect) const {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  const auto& new_rect = Rect();
  const auto& old_rect = old_cull_rect.Rect();
  if (old_rect == new_rect || (old_rect.IsEmpty() && new_rect.IsEmpty()))
    return false;

  if (old_rect.IsEmpty())
    return true;

  auto expanded_old_rect = old_rect;
  static const int kChangedEnoughMinimumDistance = 512;
  expanded_old_rect.Inflate(kChangedEnoughMinimumDistance);
  return !expanded_old_rect.Contains(new_rect);
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

static constexpr int kReasonablePixelLimit =
    std::numeric_limits<int>::max() / 2;

// Returns the number of pixels to expand the cull rect for composited scroll
// and transform.
static int LocalPixelDistanceToExpand(
    const TransformPaintPropertyNode& root_transform,
    const TransformPaintPropertyNode& local_transform) {
  // Number of pixels to expand in root coordinates for cull rect under
  // composited scroll translation or other composited transform.
  static constexpr int kPixelDistanceToExpand = 4000;

  FloatRect rect(0, 0, 1, 1);
  GeometryMapper::SourceToDestinationRect(root_transform, local_transform,
                                          rect);
  // Now rect.Size() is the size of a screen pixel in local coordinates.
  float scale = std::max(rect.Width(), rect.Height());
  // A very big scale may be caused by non-invertable near non-invertable
  // transforms. Fallback to scale 1. The limit is heuristic.
  if (scale > kReasonablePixelLimit / kPixelDistanceToExpand)
    return kPixelDistanceToExpand;
  return scale * kPixelDistanceToExpand;
}

bool CullRect::Intersects(const IntRect& rect) const {
  if (rect.IsEmpty())
    return false;
  return IsInfinite() || rect.Intersects(rect_);
}

bool CullRect::IntersectsTransformed(const AffineTransform& transform,
                                     const FloatRect& rect) const {
  if (rect.IsEmpty())
    return false;
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

void CullRect::Move(const FloatSize& offset) {
  if (IsInfinite())
    return;

  FloatRect float_rect(rect_);
  float_rect.Move(offset);
  rect_ = EnclosingIntRect(float_rect);
}

void CullRect::ApplyTransform(const TransformPaintPropertyNode& transform) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      transform.ScrollNode()) {
    // TODO(wangxianzhu): Remove this code path for CullRectUpdate.
    ApplyScrollTranslation(transform, transform);
  } else {
    ApplyTransformWithoutExpansion(transform);
  }
}

void CullRect::ApplyTransformWithoutExpansion(
    const TransformPaintPropertyNode& transform) {
  if (IsInfinite())
    return;

  DCHECK(transform.Parent());
  GeometryMapper::SourceToDestinationRect(*transform.Parent(), transform,
                                          rect_);
}

CullRect::ApplyTransformResult CullRect::ApplyScrollTranslation(
    const TransformPaintPropertyNode& root_transform,
    const TransformPaintPropertyNode& scroll_translation) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
         RuntimeEnabledFeatures::CullRectUpdateEnabled());

  const auto* scroll = scroll_translation.ScrollNode();
  DCHECK(scroll);

  rect_.Intersect(scroll->ContainerRect());
  if (rect_.IsEmpty())
    return kNotExpanded;

  ApplyTransformWithoutExpansion(scroll_translation);

  // Don't expand for non-composited scrolling.
  if (!scroll_translation.HasDirectCompositingReasons())
    return kNotExpanded;

  // We create scroll node for the root scroller even it's not scrollable.
  // Don't expand in the case.
  if (scroll->ContainerRect().Width() >= scroll->ContentsSize().Width() &&
      scroll->ContainerRect().Height() >= scroll->ContentsSize().Height())
    return kNotExpanded;

  // Expand the cull rect for scrolling contents for composited scrolling.
  rect_.Inflate(LocalPixelDistanceToExpand(root_transform, scroll_translation));
  IntRect contents_rect(IntPoint(), scroll->ContentsSize());
  rect_.Intersect(contents_rect);
  return rect_ == contents_rect ? kExpandedForWholeScrollingContents
                                : kExpandedForPartialScrollingContents;
}

void CullRect::ApplyTransforms(const TransformPaintPropertyNode& source,
                               const TransformPaintPropertyNode& destination,
                               const base::Optional<CullRect>& old_cull_rect) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  Vector<const TransformPaintPropertyNode*> scroll_translations;
  for (const auto* t = &destination; t != &source; t = t->UnaliasedParent()) {
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
        ApplyScrollTranslation(source, *scroll_translation);
    last_transform = scroll_translation;
  }

  if (!IsInfinite()) {
    GeometryMapper::SourceToDestinationRect(*last_transform, destination,
                                            rect_);
  }

  if (last_scroll_translation_result == kExpandedForPartialScrollingContents &&
      old_cull_rect &&
      !ChangedEnough(*old_cull_rect,
                     &last_transform->ScrollNode()->ContentsSize())) {
    rect_ = old_cull_rect->Rect();
  }
}

void CullRect::ApplyPaintPropertiesWithoutExpansion(
    const PropertyTreeState& source,
    const PropertyTreeState& destination) {
  FloatClipRect clip_rect =
      GeometryMapper::LocalToAncestorClipRect(destination, source);
  if (!clip_rect.IsInfinite())
    rect_.Intersect(EnclosingIntRect(clip_rect.Rect()));
  if (!IsInfinite()) {
    GeometryMapper::SourceToDestinationRect(source.Transform(),
                                            destination.Transform(), rect_);
  }
}

void CullRect::ApplyPaintProperties(
    const PropertyTreeState& root,
    const PropertyTreeState& source,
    const PropertyTreeState& destination,
    const base::Optional<CullRect>& old_cull_rect) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
         RuntimeEnabledFeatures::CullRectUpdateEnabled());

  Vector<const TransformPaintPropertyNode*, 4> scroll_translations;
  Vector<const ClipPaintPropertyNode*, 4> clips;
  bool abnormal_hierarchy = false;

  for (const auto* t = &destination.Transform(); t != &source.Transform();
       t = t->UnaliasedParent()) {
    DCHECK(t);
    if (t == &root.Transform()) {
      abnormal_hierarchy = true;
      break;
    }
    if (t->ScrollNode())
      scroll_translations.push_back(t);
  }

  if (!abnormal_hierarchy) {
    for (const auto* c = &destination.Clip(); c != &source.Clip();
         c = c->UnaliasedParent()) {
      DCHECK(c);
      if (c == &root.Clip()) {
        abnormal_hierarchy = true;
        break;
      }
      clips.push_back(c);
    }
  }

  if (abnormal_hierarchy) {
    // Either the transform or the clip of |source| is not an ancestor of
    // |destination|. Map infinite rect from the root.
    *this = Infinite();
    ApplyPaintProperties(root, root, destination, old_cull_rect);
    return;
  }

  // These are either the source transform/clip or the last scroll
  // translation's transform/clip.
  const auto* last_transform = &source.Transform();
  const auto* last_clip = &source.Clip();
  auto last_scroll_translation_result = kNotExpanded;

  // For now effects (especially pixel-moving filters) are not considered in
  // this class. The client has to use infinite cull rect in the case.
  // TODO(wangxianzhu): support clip rect expansion for pixel-moving filters.
  const auto& effect_root = EffectPaintPropertyNode::Root();
  auto scroll_translation_it = scroll_translations.rbegin();
  for (const auto* clip : base::Reversed(clips)) {
    if (scroll_translation_it == scroll_translations.rend())
      break;

    const auto* scroll_translation = *scroll_translation_it++;
    if (&clip->LocalTransformSpace() != scroll_translation->Parent())
      continue;

    ApplyPaintPropertiesWithoutExpansion(
        PropertyTreeState(*last_transform, *last_clip, effect_root),
        PropertyTreeState(*scroll_translation->UnaliasedParent(), *clip,
                          effect_root));
    last_scroll_translation_result =
        ApplyScrollTranslation(root.Transform(), *scroll_translation);

    last_transform = scroll_translation;
    last_clip = clip;
  }

  ApplyPaintPropertiesWithoutExpansion(
      PropertyTreeState(*last_transform, *last_clip, effect_root), destination);

  // Since the cull rect mapping above can produce extremely large numbers in
  // cases of perspective, try our best to "normalize" the result by ensuring
  // that none of the rect dimensions exceed some large, but reasonable, limit.
  // Note that by clamping X and Y, we are effectively moving the rect right /
  // down. However, this will at most make us paint more content, which is
  // better than erroneously deciding that the rect produced here is far
  // offscreen.
  if (rect_.X() < -kReasonablePixelLimit)
    rect_.SetX(-kReasonablePixelLimit);
  if (rect_.Y() < -kReasonablePixelLimit)
    rect_.SetY(-kReasonablePixelLimit);
  if (rect_.MaxX() > kReasonablePixelLimit)
    rect_.ShiftMaxXEdgeTo(kReasonablePixelLimit);
  if (rect_.MaxY() > kReasonablePixelLimit)
    rect_.ShiftMaxYEdgeTo(kReasonablePixelLimit);

  const IntSize* expansion_bounds = nullptr;
  bool expanded = false;
  if (last_scroll_translation_result == kExpandedForPartialScrollingContents &&
      last_clip == &destination.Clip()) {
    DCHECK(last_transform->ScrollNode());
    expansion_bounds = &last_transform->ScrollNode()->ContentsSize();
    expanded = true;
  } else if (!IsInfinite() && last_transform != &destination.Transform() &&
             destination.Transform().RequiresCullRectExpansion()) {
    // Direct compositing reasons such as will-change transform can cause the
    // content to move arbitrarily, so there is no exact cull rect. Instead of
    // using an infinite rect, we use a heuristic of expanding by
    // |pixel_distance_to_expand|. To avoid extreme expansion in the presence
    // of nested composited transforms, the heuristic is skipped for rects that
    // are already very large.
    int pixel_distance_to_expand =
        LocalPixelDistanceToExpand(root.Transform(), destination.Transform());
    if (rect_.Width() < pixel_distance_to_expand) {
      rect_.InflateX(pixel_distance_to_expand);
      expanded = true;
    }
    if (rect_.Height() < pixel_distance_to_expand) {
      rect_.InflateY(pixel_distance_to_expand);
      expanded = true;
    }
  }

  if (expanded && old_cull_rect &&
      !ChangedEnough(*old_cull_rect, expansion_bounds))
    rect_ = old_cull_rect->Rect();
}

bool CullRect::ChangedEnough(const CullRect& old_cull_rect,
                             const IntSize* expansion_bounds) const {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
         RuntimeEnabledFeatures::CullRectUpdateEnabled());

  const auto& new_rect = Rect();
  const auto& old_rect = old_cull_rect.Rect();
  if (old_rect.Contains(new_rect))
    return false;
  if (old_rect.IsEmpty() && new_rect.IsEmpty())
    return false;

  if (old_rect.IsEmpty())
    return true;

  static constexpr int kChangedEnoughMinimumDistance = 512;
  auto expanded_old_rect = old_rect;
  expanded_old_rect.Inflate(kChangedEnoughMinimumDistance);
  if (!expanded_old_rect.Contains(new_rect))
    return true;

  // The following edge checking logic applies only when the bounds (which were
  // used to clip the cull rect) are known.
  if (!expansion_bounds)
    return false;

  // The cull rect must have been clipped by *expansion_bounds.
  DCHECK(IntRect(IntPoint(), *expansion_bounds).Contains(rect_));

  // Even if the new cull rect doesn't include enough new area to satisfy
  // the condition above, update anyway if it touches the edge of the scrolling
  // contents that is not touched by the existing cull rect.  Because it's
  // impossible to expose more area in the direction, update cannot be deferred
  // until the exposed new area satisfies the condition above.
  // For example,
  //   scroller contents dimensions: 100x1000
  //   old cull rect: 0,100 100x8000
  // A new rect of 0,0 100x8000 will not be |kChangedEnoughMinimumDistance|
  // pixels away from the current rect. Without additional logic for this case,
  // we will continue using the old cull rect.
  if (rect_.X() == 0 && old_cull_rect.Rect().X() != 0)
    return true;
  if (rect_.Y() == 0 && old_cull_rect.Rect().Y() != 0)
    return true;
  if (rect_.MaxX() == expansion_bounds->Width() &&
      old_cull_rect.Rect().MaxX() != expansion_bounds->Width())
    return true;
  if (rect_.MaxY() == expansion_bounds->Height() &&
      old_cull_rect.Rect().MaxY() != expansion_bounds->Height())
    return true;

  return false;
}

}  // namespace blink

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

constexpr int kReasonablePixelLimit = kIntMaxForLayoutUnit;
constexpr int kChangedEnoughMinimumDistance = 512;

// Number of pixels to expand in root coordinates for cull rect under
// composited scroll translation or other composited transform.
constexpr int kPixelDistanceToExpand = 4000;

// Returns the number of pixels to expand the cull rect for composited scroll
// and transform.
int LocalPixelDistanceToExpand(
    const TransformPaintPropertyNode& root_transform,
    const TransformPaintPropertyNode& local_transform) {
  float scale = GeometryMapper::SourceToDestinationApproximateMinimumScale(
      root_transform, local_transform);
  // A very big scale may be caused by non-invertable near non-invertable
  // transforms. Fallback to scale 1. The limit is heuristic.
  if (scale > kReasonablePixelLimit / kPixelDistanceToExpand)
    return kPixelDistanceToExpand;
  return scale * kPixelDistanceToExpand;
}

}  // anonymous namespace

bool CullRect::Intersects(const gfx::Rect& rect) const {
  if (rect.IsEmpty())
    return false;
  return IsInfinite() || rect.Intersects(rect_);
}

bool CullRect::IntersectsTransformed(const AffineTransform& transform,
                                     const gfx::RectF& rect) const {
  if (rect.IsEmpty())
    return false;
  return IsInfinite() || transform.MapRect(rect).Intersects(gfx::RectF(rect_));
}

bool CullRect::IntersectsHorizontalRange(LayoutUnit lo, LayoutUnit hi) const {
  return !(lo >= rect_.right() || hi <= rect_.x());
}

bool CullRect::IntersectsVerticalRange(LayoutUnit lo, LayoutUnit hi) const {
  return !(lo >= rect_.bottom() || hi <= rect_.y());
}

void CullRect::Move(const gfx::Vector2d& offset) {
  if (!IsInfinite())
    rect_.Offset(offset);
}

void CullRect::ApplyTransform(const TransformPaintPropertyNode& transform) {
  if (IsInfinite())
    return;

  DCHECK(transform.Parent());
  GeometryMapper::SourceToDestinationRect(*transform.Parent(), transform,
                                          rect_);
}

bool CullRect::ApplyScrollTranslation(
    const TransformPaintPropertyNode& root_transform,
    const TransformPaintPropertyNode& scroll_translation,
    bool disable_expansion) {
  const auto* scroll = scroll_translation.ScrollNode();
  DCHECK(scroll);

  rect_.Intersect(scroll->ContainerRect());
  if (rect_.IsEmpty())
    return false;

  ApplyTransform(scroll_translation);

  if (disable_expansion) {
    return false;
  }
  // kNotPreferred is used for selects/inputs which don't benefit from
  // composited scrolling.
  if (scroll->GetCompositedScrollingPreference() ==
      CompositedScrollingPreference::kNotPreferred) {
    return false;
  }
  // We create scroll node for the root scroller even it's not scrollable.
  // Don't expand in the case.
  gfx::Rect contents_rect = scroll->ContentsRect();
  if (scroll->ContainerRect().width() >= contents_rect.width() &&
      scroll->ContainerRect().height() >= contents_rect.height())
    return false;

  // Expand the cull rect for scrolling contents for composited scrolling.
  rect_.Outset(LocalPixelDistanceToExpand(root_transform, scroll_translation));
  rect_.Intersect(contents_rect);
  return true;
}

bool CullRect::ApplyPaintPropertiesWithoutExpansion(
    const PropertyTreeState& source,
    const PropertyTreeState& destination) {
  FloatClipRect clip_rect =
      GeometryMapper::LocalToAncestorClipRect(destination, source);
  if (clip_rect.Rect().IsEmpty()) {
    rect_ = gfx::Rect();
    return false;
  }
  if (!clip_rect.IsInfinite()) {
    rect_.Intersect(gfx::ToEnclosingRect(clip_rect.Rect()));
    if (rect_.IsEmpty())
      return false;
  }
  if (!IsInfinite()) {
    GeometryMapper::SourceToDestinationRect(source.Transform(),
                                            destination.Transform(), rect_);
  }
  // Return true even if the transformed rect is empty (e.g. by rotateX(90deg))
  // because later transforms may make the content visible again.
  return true;
}

bool CullRect::ApplyPaintProperties(
    const PropertyTreeState& root,
    const PropertyTreeState& source,
    const PropertyTreeState& destination,
    const absl::optional<CullRect>& old_cull_rect,
    bool disable_expansion) {
  // The caller should check this before calling this function.
  DCHECK_NE(source, destination);

  // Only a clip can make an infinite cull rect finite.
  if (IsInfinite() && &destination.Clip() == &source.Clip())
    return false;

  Vector<const TransformPaintPropertyNode*, 4> scroll_translations;
  Vector<const ClipPaintPropertyNode*, 4> clips;
  bool abnormal_hierarchy = false;

  for (const auto* t = &destination.Transform(); t != &source.Transform();
       t = t->UnaliasedParent()) {
    // TODO(wangxianzhu): This should be DCHECK(t), but for now we need to
    // work around crbug.com/1262837 etc. Also see the TODO in
    // FragmentData::LocalBorderBoxProperties().
    if (!t)
      return false;
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
    return root != destination &&
           ApplyPaintProperties(root, root, destination, old_cull_rect,
                                disable_expansion);
  }

  // These are either the source transform/clip or the last scroll
  // translation's transform/clip.
  const auto* last_transform = &source.Transform();
  const auto* last_clip = &source.Clip();
  bool expanded = false;

  // For now effects (especially pixel-moving filters) are not considered in
  // this class. The client has to use infinite cull rect in the case.
  // TODO(wangxianzhu): support clip rect expansion for pixel-moving filters.
  const auto& effect_root = EffectPaintPropertyNode::Root();
  auto clip_it = clips.rbegin();
  for (const auto* scroll_translation : base::Reversed(scroll_translations)) {
    if (clip_it == clips.rend())
      break;

    // Skip clips until we find one in the same space as |scroll_translation|.
    while (clip_it != clips.rend() &&
           &(*clip_it)->LocalTransformSpace() != scroll_translation->Parent()) {
      clip_it++;
    }

    // Find the last clip in the same space as |scroll_translation|.
    const ClipPaintPropertyNode* updated_last_clip = nullptr;
    while (clip_it != clips.rend() &&
           &(*clip_it)->LocalTransformSpace() == scroll_translation->Parent()) {
      updated_last_clip = *clip_it;
      clip_it++;
    }

    // Process all clips in the same space as |scroll_translation|.
    if (updated_last_clip) {
      if (!ApplyPaintPropertiesWithoutExpansion(
              PropertyTreeState(*last_transform, *last_clip, effect_root),
              PropertyTreeState(*scroll_translation->UnaliasedParent(),
                                *updated_last_clip, effect_root))) {
        return false;
      }
      last_clip = updated_last_clip;
    }

    // We only keep the expanded status of the last scroll translation.
    expanded = ApplyScrollTranslation(root.Transform(), *scroll_translation,
                                      disable_expansion);
    last_transform = scroll_translation;
  }

  if (!ApplyPaintPropertiesWithoutExpansion(
          PropertyTreeState(*last_transform, *last_clip, effect_root),
          destination))
    return false;

  if (IsInfinite())
    return false;

  // Since the cull rect mapping above can produce extremely large numbers in
  // cases of perspective, try our best to "normalize" the result by ensuring
  // that none of the rect dimensions exceed some large, but reasonable, limit.
  // Note that by clamping X and Y, we are effectively moving the rect right /
  // down. However, this will at most make us paint more content, which is
  // better than erroneously deciding that the rect produced here is far
  // offscreen.
  if (rect_.x() < -kReasonablePixelLimit)
    rect_.set_x(-kReasonablePixelLimit);
  if (rect_.y() < -kReasonablePixelLimit)
    rect_.set_y(-kReasonablePixelLimit);
  if (rect_.right() > kReasonablePixelLimit)
    rect_.set_width(kReasonablePixelLimit - rect_.x());
  if (rect_.bottom() > kReasonablePixelLimit)
    rect_.set_height(kReasonablePixelLimit - rect_.y());

  absl::optional<gfx::Rect> expansion_bounds;
  if (expanded) {
    DCHECK(last_transform->ScrollNode());
    expansion_bounds = last_transform->ScrollNode()->ContentsRect();
    if (last_transform != &destination.Transform() ||
        last_clip != &destination.Clip()) {
      // Map expansion_bounds in the same way as we did for rect_ in the last
      // ApplyPaintPropertiesWithoutExpansion().
      FloatClipRect clip_rect = GeometryMapper::LocalToAncestorClipRect(
          destination,
          PropertyTreeState(*last_transform, *last_clip, effect_root));
      if (!clip_rect.IsInfinite())
        expansion_bounds->Intersect(gfx::ToEnclosingRect(clip_rect.Rect()));
      GeometryMapper::SourceToDestinationRect(
          *last_transform, destination.Transform(), *expansion_bounds);
    }
  }

  if (!disable_expansion && last_transform != &destination.Transform() &&
      destination.Transform().RequiresCullRectExpansion()) {
    // Direct compositing reasons such as will-change transform can cause the
    // content to move arbitrarily, so there is no exact cull rect. Instead of
    // using an infinite rect, we use a heuristic of expanding by
    // |pixel_distance_to_expand|. To avoid extreme expansion in the presence
    // of nested composited transforms, the heuristic is skipped for rects that
    // are already very large.
    int pixel_distance_to_expand =
        LocalPixelDistanceToExpand(root.Transform(), destination.Transform());
    if (rect_.width() < pixel_distance_to_expand) {
      rect_.Outset(gfx::Outsets::VH(0, pixel_distance_to_expand));
      if (expansion_bounds)
        expansion_bounds->Outset(gfx::Outsets::VH(0, pixel_distance_to_expand));
      expanded = true;
    }
    if (rect_.height() < pixel_distance_to_expand) {
      rect_.Outset(gfx::Outsets::VH(pixel_distance_to_expand, 0));
      if (expansion_bounds)
        expansion_bounds->Outset(gfx::Outsets::VH(pixel_distance_to_expand, 0));
      expanded = true;
    }
  }

  if (expanded && old_cull_rect &&
      !ChangedEnough(*old_cull_rect, expansion_bounds))
    rect_ = old_cull_rect->Rect();

  return expanded;
}

bool CullRect::ChangedEnough(
    const CullRect& old_cull_rect,
    const absl::optional<gfx::Rect>& expansion_bounds) const {
  const auto& new_rect = Rect();
  const auto& old_rect = old_cull_rect.Rect();
  if (old_rect.Contains(new_rect))
    return false;
  if (old_rect.IsEmpty() && new_rect.IsEmpty())
    return false;

  if (old_rect.IsEmpty())
    return true;

  auto expanded_old_rect = old_rect;
  expanded_old_rect.Outset(kChangedEnoughMinimumDistance);
  if (!expanded_old_rect.Contains(new_rect))
    return true;

  // The following edge checking logic applies only when the bounds (which were
  // used to clip the cull rect) are known.
  if (!expansion_bounds)
    return false;

  // The cull rect must have been clipped by *expansion_bounds.
  DCHECK(expansion_bounds->Contains(rect_));

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
  if (rect_.x() == expansion_bounds->x() &&
      old_cull_rect.Rect().x() != expansion_bounds->x())
    return true;
  if (rect_.y() == expansion_bounds->y() &&
      old_cull_rect.Rect().y() != expansion_bounds->y())
    return true;
  if (rect_.right() == expansion_bounds->right() &&
      old_cull_rect.Rect().right() != expansion_bounds->right())
    return true;
  if (rect_.bottom() == expansion_bounds->bottom() &&
      old_cull_rect.Rect().bottom() != expansion_bounds->bottom())
    return true;

  return false;
}

bool CullRect::HasScrolledEnough(
    const gfx::Vector2dF& delta,
    const TransformPaintPropertyNode& scroll_translation) {
  if (!scroll_translation.ScrollNode()) {
    return !delta.IsZero();
  }
  if (std::abs(delta.x()) < kChangedEnoughMinimumDistance &&
      std::abs(delta.y()) < kChangedEnoughMinimumDistance) {
    return false;
  }

  // Return false if the scroll won't expose more contents in the scrolled
  // direction.
  gfx::Rect contents_rect = scroll_translation.ScrollNode()->ContentsRect();
  if (Rect().Contains(contents_rect))
    return false;
  return (delta.x() < 0 && Rect().x() != contents_rect.x()) ||
         (delta.x() > 0 && Rect().right() != contents_rect.right()) ||
         (delta.y() < 0 && Rect().y() != contents_rect.y()) ||
         (delta.y() > 0 && Rect().bottom() != contents_rect.bottom());
}

}  // namespace blink

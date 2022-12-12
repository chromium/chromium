// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// Expands a visual rect under a fixed-position transform so that the result
// covers all area that could overlap with anything under the scroller during
// scrolling.
void ExpandFixedVisualRectInScroller(
    const TransformPaintPropertyNode& scroll_translation,
    gfx::RectF& rect) {
  DCHECK(scroll_translation.ScrollNode());

  // First move the rect back to the min scroll offset, by accounting for the
  // current scroll offset.
  rect.Offset(scroll_translation.Get2dTranslation());

  // Calculate the max scroll offset and expand by that amount. The max scroll
  // offset is the contents size minus one viewport's worth of space (i.e. the
  // container rect size).
  gfx::SizeF expansion(scroll_translation.ScrollNode()->ContentsRect().size() -
                       scroll_translation.ScrollNode()->ContainerRect().size());
  rect.set_size(rect.size() + expansion);
}

}  // namespace

gfx::Transform GeometryMapper::SourceToDestinationProjection(
    const TransformPaintPropertyNode& source,
    const TransformPaintPropertyNode& destination) {
  ExtraProjectionResult extra_result;
  bool success = false;
  return SourceToDestinationProjectionInternal(source, destination,
                                               extra_result, success);
}

// Returns flatten(destination_to_screen)^-1 * flatten(source_to_screen)
//
// In case that source and destination are coplanar in tree hierarchy [1],
// computes destination_to_plane_root ^ -1 * source_to_plane_root.
// It can be proved that [2] the result will be the same (except numerical
// errors) when the plane root has invertible screen projection, and this
// offers fallback definition when plane root is singular. For example:
// <div style="transform:rotateY(90deg); overflow:scroll;">
//   <div id="A" style="opacity:0.5;">
//     <div id="B" style="position:absolute;"></div>
//   </div>
// </div>
// Both A and B have non-invertible screen projection, nevertheless it is
// useful to define projection between A and B. Say, the transform may be
// animated in compositor thus become visible.
// As SPv1 treats 3D transforms as compositing trigger, that implies mappings
// within the same compositing layer can only contain 2D transforms, thus
// intra-composited-layer queries are guaranteed to be handled correctly.
//
// [1] As defined by that all local transforms between source and some common
//     ancestor 'plane root' and all local transforms between the destination
//     and the plane root being flat.
// [2] destination_to_screen = plane_root_to_screen * destination_to_plane_root
//     source_to_screen = plane_root_to_screen * source_to_plane_root
//     output = flatten(destination_to_screen)^-1 * flatten(source_to_screen)
//     = flatten(plane_root_to_screen * destination_to_plane_root)^-1 *
//       flatten(plane_root_to_screen * source_to_plane_root)
//     Because both destination_to_plane_root and source_to_plane_root are
//     already flat,
//     = flatten(plane_root_to_screen * flatten(destination_to_plane_root))^-1 *
//       flatten(plane_root_to_screen * flatten(source_to_plane_root))
//     By flatten lemma [3] flatten(A * flatten(B)) = flatten(A) * flatten(B),
//     = flatten(destination_to_plane_root)^-1 *
//       flatten(plane_root_to_screen)^-1 *
//       flatten(plane_root_to_screen) * flatten(source_to_plane_root)
//     If flatten(plane_root_to_screen) is invertible, they cancel out:
//     = flatten(destination_to_plane_root)^-1 * flatten(source_to_plane_root)
//     = destination_to_plane_root^-1 * source_to_plane_root
// [3] Flatten lemma: https://goo.gl/DNKyOc
gfx::Transform GeometryMapper::SourceToDestinationProjectionInternal(
    const TransformPaintPropertyNode& source,
    const TransformPaintPropertyNode& destination,
    ExtraProjectionResult& extra_result,
    bool& success) {
  success = true;

  if (&source == &destination)
    return gfx::Transform();

  if (source.Parent() && &destination == &source.Parent()->Unalias()) {
    extra_result.has_fixed = source.RequiresCompositingForFixedPosition();
    extra_result.has_sticky = source.RequiresCompositingForStickyPosition();
    if (source.IsIdentityOr2dTranslation() && source.Origin().IsOrigin()) {
      // The result will be translate(origin)*matrix*translate(-origin) which
      // equals to matrix if the origin is zero or if the matrix is just
      // identity or 2d translation.
      extra_result.has_animation = source.HasActiveTransformAnimation();
      return source.Matrix();
    }
  }

  if (destination.IsIdentityOr2dTranslation() && destination.Parent() &&
      &source == &destination.Parent()->Unalias() &&
      !destination.HasActiveTransformAnimation()) {
    return gfx::Transform::MakeTranslation(-destination.Get2dTranslation());
  }

  const auto& source_cache = source.GetTransformCache();
  const auto& destination_cache = destination.GetTransformCache();

  extra_result.has_fixed |= source_cache.has_fixed();
  extra_result.has_sticky |= source_cache.has_sticky();

  // Case 1a (fast path of case 1b): check if source and destination are under
  // the same 2d translation root.
  if (source_cache.root_of_2d_translation() ==
      destination_cache.root_of_2d_translation()) {
    // We always use full matrix for animating transforms.
    return gfx::Transform::MakeTranslation(
        source_cache.to_2d_translation_root() -
        destination_cache.to_2d_translation_root());
  }

  // Case 1b: Check if source and destination are known to be coplanar.
  // Even if destination may have invertible screen projection,
  // this formula is likely to be numerically more stable.
  if (source_cache.plane_root() == destination_cache.plane_root()) {
    extra_result.has_animation =
        source_cache.has_animation_to_plane_root() ||
        destination_cache.has_animation_to_plane_root();
    if (&source == destination_cache.plane_root())
      return destination_cache.from_plane_root();
    if (&destination == source_cache.plane_root())
      return source_cache.to_plane_root();

    gfx::Transform matrix;
    destination_cache.ApplyFromPlaneRoot(matrix);
    source_cache.ApplyToPlaneRoot(matrix);
    return matrix;
  }

  // Case 2: Check if we can fallback to the canonical definition of
  // flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  // If flatten(destination_to_screen)^-1 is invalid, we are out of luck.
  // Screen transform data are updated lazily because they are rarely used.
  source.UpdateScreenTransform();
  destination.UpdateScreenTransform();
  extra_result.has_animation = source_cache.has_animation_to_screen() ||
                               destination_cache.has_animation_to_screen();
  if (!destination_cache.projection_from_screen_is_valid()) {
    success = false;
    return gfx::Transform();
  }

  // Case 3: Compute:
  // flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  const auto& root = TransformPaintPropertyNode::Root();
  if (&source == &root)
    return destination_cache.projection_from_screen();
  gfx::Transform matrix;
  destination_cache.ApplyProjectionFromScreen(matrix);
  source_cache.ApplyToScreen(matrix);
  matrix.Flatten();
  return matrix;
}

float GeometryMapper::SourceToDestinationApproximateMinimumScale(
    const TransformPaintPropertyNode& source,
    const TransformPaintPropertyNode& destination) {
  if (&source == &destination)
    return 1.f;

  const auto& source_cache = source.GetTransformCache();
  const auto& destination_cache = destination.GetTransformCache();
  if (source_cache.root_of_2d_translation() ==
      destination_cache.root_of_2d_translation()) {
    return 1.f;
  }

  gfx::RectF rect(0, 0, 1, 1);
  SourceToDestinationRect(source, destination, rect);
  return std::min(rect.width(), rect.height());
}

bool GeometryMapper::LocalToAncestorVisualRect(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& mapping_rect,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior) {
  return LocalToAncestorVisualRectInternal<ForCompositingOverlap::kNo>(
      local_state, ancestor_state, mapping_rect, clip_behavior,
      inclusive_behavior);
}

template <GeometryMapper::ForCompositingOverlap for_compositing_overlap>
bool GeometryMapper::LocalToAncestorVisualRectInternal(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& rect_to_map,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior) {
  // Many effects (e.g. filters, clip-paths) can make a clip rect not tight.
  if (&local_state.Effect() != &ancestor_state.Effect())
    rect_to_map.ClearIsTight();

  // The transform tree and the clip tree contain all information needed for
  // visual rect mapping. Pixel-moving filters should have corresponding
  // pixel-moving filter clip expanders in the clip tree.
  if (&local_state.Transform() == &ancestor_state.Transform() &&
      &local_state.Clip() == &ancestor_state.Clip()) {
    return true;
  }

  if (&local_state.Effect() != &ancestor_state.Effect() &&
      &local_state.Clip() != &ancestor_state.Clip()) {
    return SlowLocalToAncestorVisualRectWithEffects<for_compositing_overlap>(
        local_state, ancestor_state, rect_to_map, clip_behavior,
        inclusive_behavior);
  }

  ExtraProjectionResult extra_result;
  bool success = false;
  gfx::Transform projection = SourceToDestinationProjectionInternal(
      local_state.Transform(), ancestor_state.Transform(), extra_result,
      success);
  if (!success) {
    // A failure implies either source-to-plane or destination-to-plane being
    // singular. A notable example of singular source-to-plane from valid CSS:
    // <div id="plane" style="transform:rotateY(180deg)">
    //   <div style="overflow:overflow">
    //     <div id="ancestor" style="opacity:0.5;">
    //       <div id="local" style="position:absolute; transform:scaleX(0);">
    //       </div>
    //     </div>
    //   </div>
    // </div>
    // Either way, the element won't be renderable thus returning empty rect.
    rect_to_map = FloatClipRect(gfx::RectF());
    return false;
  }

  if (for_compositing_overlap == ForCompositingOverlap::kYes &&
      (extra_result.has_animation || extra_result.has_sticky)) {
    // Assume during the animation or the sticky translation can map
    // |rect_to_map| to anywhere during animation or composited scroll.
    // Ancestor clips will still apply.
    // TODO(crbug.com/1026653): Use animation bounds instead of infinite rect.
    // TODO(crbug.com/1117658): Use sticky bounds instead of infinite rect.
    rect_to_map = InfiniteLooseFloatClipRect();
  } else {
    rect_to_map.Map(projection);
  }

  FloatClipRect clip_rect =
      LocalToAncestorClipRectInternal<for_compositing_overlap>(
          local_state.Clip(), ancestor_state.Clip(), ancestor_state.Transform(),
          clip_behavior, inclusive_behavior);
  // This is where we propagate the roundedness and tightness of |clip_rect|
  // to |rect_to_map|.
  if (inclusive_behavior == kInclusiveIntersect)
    return rect_to_map.InclusiveIntersect(clip_rect);
  rect_to_map.Intersect(clip_rect);
  return !rect_to_map.Rect().IsEmpty();
}

template <GeometryMapper::ForCompositingOverlap for_compositing_overlap>
bool GeometryMapper::SlowLocalToAncestorVisualRectWithEffects(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& rect_to_map,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior) {
  PropertyTreeState last_state = local_state;
  last_state.SetEffect(ancestor_state.Effect());
  const auto* ancestor_filter_clip =
      ancestor_state.Clip().NearestPixelMovingFilterClip();
  const auto* filter_clip = local_state.Clip().NearestPixelMovingFilterClip();
  while (filter_clip != ancestor_filter_clip) {
    if (!filter_clip) {
      // Abnormal clip hierarchy.
      rect_to_map = InfiniteLooseFloatClipRect();
      return true;
    }

    PropertyTreeState new_state(filter_clip->LocalTransformSpace().Unalias(),
                                *filter_clip, last_state.Effect());
    const auto* filter = filter_clip->PixelMovingFilter();
    DCHECK(filter);
    DCHECK_EQ(&filter->LocalTransformSpace().Unalias(), &new_state.Transform());
    if (for_compositing_overlap == ForCompositingOverlap::kYes &&
        filter->HasActiveFilterAnimation()) {
      // Assume during the animation the filter can map |rect_to_map| to
      // anywhere. Ancestor clips will still apply.
      // TODO(crbug.com/1026653): Use animation bounds instead of infinite
      // rect.
      rect_to_map = InfiniteLooseFloatClipRect();
    } else {
      bool intersects =
          LocalToAncestorVisualRectInternal<for_compositing_overlap>(
              last_state, new_state, rect_to_map, clip_behavior,
              inclusive_behavior);
      if (!intersects) {
        rect_to_map = FloatClipRect(gfx::RectF());
        return false;
      }
      if (!rect_to_map.IsInfinite())
        rect_to_map.Rect() = filter->MapRect(rect_to_map.Rect());
    }

    last_state = new_state;
    const auto* next_clip = filter_clip->UnaliasedParent();
    DCHECK(next_clip);
    last_state.SetClip(*next_clip);
    filter_clip = next_clip->NearestPixelMovingFilterClip();
  }

  return LocalToAncestorVisualRectInternal<for_compositing_overlap>(
      last_state, ancestor_state, rect_to_map, clip_behavior,
      inclusive_behavior);
}

FloatClipRect GeometryMapper::LocalToAncestorClipRect(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    OverlayScrollbarClipBehavior clip_behavior) {
  const auto& local_clip = local_state.Clip();
  const auto& ancestor_clip = ancestor_state.Clip();
  if (&local_clip == &ancestor_clip)
    return FloatClipRect();

  auto result = LocalToAncestorClipRectInternal<ForCompositingOverlap::kNo>(
      local_clip, ancestor_clip, ancestor_state.Transform(), clip_behavior,
      kNonInclusiveIntersect);

  // Many effects (e.g. filters, clip-paths) can make a clip rect not tight.
  if (&local_state.Effect() != &ancestor_state.Effect())
    result.ClearIsTight();

  return result;
}

static FloatClipRect GetClipRect(const ClipPaintPropertyNode& clip_node,
                                 OverlayScrollbarClipBehavior clip_behavior) {
  // TODO(crbug.com/1248598): Do we need to use PaintClipRect when mapping for
  // painting/compositing?
  FloatClipRect clip_rect =
      UNLIKELY(clip_behavior == kExcludeOverlayScrollbarSizeForHitTesting)
          ? clip_node.LayoutClipRectExcludingOverlayScrollbars()
          : clip_node.LayoutClipRect();
  if (clip_node.ClipPath())
    clip_rect.ClearIsTight();
  return clip_rect;
}

template <GeometryMapper::ForCompositingOverlap for_compositing_overlap>
FloatClipRect GeometryMapper::LocalToAncestorClipRectInternal(
    const ClipPaintPropertyNode& descendant_clip,
    const ClipPaintPropertyNode& ancestor_clip,
    const TransformPaintPropertyNode& ancestor_transform,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior) {
  if (&descendant_clip == &ancestor_clip)
    return FloatClipRect();

  if (descendant_clip.UnaliasedParent() == &ancestor_clip &&
      &descendant_clip.LocalTransformSpace() == &ancestor_transform) {
    return GetClipRect(descendant_clip, clip_behavior);
  }

  FloatClipRect clip;
  const auto* clip_node = &descendant_clip;
  Vector<const ClipPaintPropertyNode*> intermediate_nodes;

  GeometryMapperClipCache::ClipAndTransform clip_and_transform(
      &ancestor_clip, &ancestor_transform, clip_behavior);
  // Iterate over the path from localState.clip to ancestor_state.clip. Stop if
  // we've found a memoized (precomputed) clip for any particular node.
  while (clip_node && clip_node != &ancestor_clip) {
    const GeometryMapperClipCache::ClipCacheEntry* cached_clip = nullptr;
    // Inclusive intersected clips are not cached at present.
    if (inclusive_behavior != kInclusiveIntersect)
      cached_clip = clip_node->GetClipCache().GetCachedClip(clip_and_transform);

    if (for_compositing_overlap == ForCompositingOverlap::kYes && cached_clip &&
        (cached_clip->has_transform_animation ||
         cached_clip->has_sticky_transform)) {
      // Don't use cached clip if it's transformed by any animating transform
      // or sticky translation.
      cached_clip = nullptr;
    }

    if (cached_clip) {
      clip = cached_clip->clip_rect;
      break;
    }

    intermediate_nodes.push_back(clip_node);
    clip_node = clip_node->UnaliasedParent();
  }
  if (!clip_node) {
    // Don't clip if the clip tree has abnormal hierarchy.
    return InfiniteLooseFloatClipRect();
  }

  // Iterate down from the top intermediate node found in the previous loop,
  // computing and memoizing clip rects as we go.
  for (auto* const node : base::Reversed(intermediate_nodes)) {
    ExtraProjectionResult extra_result;
    bool success = false;
    gfx::Transform projection = SourceToDestinationProjectionInternal(
        node->LocalTransformSpace().Unalias(), ancestor_transform, extra_result,
        success);
    if (!success)
      return FloatClipRect(gfx::RectF());

    if (for_compositing_overlap == ForCompositingOverlap::kYes &&
        (extra_result.has_animation || extra_result.has_sticky))
      continue;

    // This is where we generate the roundedness and tightness of clip rect
    // from clip and transform properties, and propagate them to |clip|.
    FloatClipRect mapped_rect(GetClipRect(*node, clip_behavior));
    mapped_rect.Map(projection);
    if (inclusive_behavior == kInclusiveIntersect) {
      clip.InclusiveIntersect(mapped_rect);
    } else {
      clip.Intersect(mapped_rect);
      // Inclusive intersected clips are not cached at present.
      node->GetClipCache().SetCachedClip(
          GeometryMapperClipCache::ClipCacheEntry{clip_and_transform, clip,
                                                  extra_result.has_animation,
                                                  extra_result.has_sticky});
    }
  }
  // Clips that are inclusive intersected or expanded for animation are not
  // cached at present.
  DCHECK(inclusive_behavior == kInclusiveIntersect ||
         for_compositing_overlap == ForCompositingOverlap::kYes ||
         descendant_clip.GetClipCache()
                 .GetCachedClip(clip_and_transform)
                 ->clip_rect == clip);
  return clip;
}

bool GeometryMapper::MightOverlapForCompositing(
    const gfx::RectF& rect1,
    const PropertyTreeState& state1,
    const gfx::RectF& rect2,
    const PropertyTreeState& state2) {
  if (&state1.Transform() == &state2.Transform())
    return MightOverlapForCompositingInternal(rect1, state1, rect2, state2);

  const auto* scroll_translation1 =
      &state1.Transform().NearestScrollTranslationNode();
  const auto* scroll_translation2 =
      &state2.Transform().NearestScrollTranslationNode();
  if (LIKELY(scroll_translation1 == scroll_translation2))
    return MightOverlapForCompositingInternal(rect1, state1, rect2, state2);

  auto new_rect1 = rect1;
  auto new_state1 = state1;
  auto new_rect2 = rect2;
  auto new_state2 = state2;

  // The first two blocks below handle simple cases of overlap testing between
  // 1. a visual rect (can be rect1 or rect2) directly under a fixed-position
  //    transform, and
  // 2. the other visual rect directly under the scroll translation associated
  //    with the fixed-position transform.
  // Complex cases where #2 is under multiple level scrollers will be handled
  // in the third block which also handles generic cases of overlap testing
  // across scrollers. More complex (but rare) cases where #1 is indirectly
  // under a fixed-position transform will be treated like a generic case.
  const auto* fixed_scroll_translation1 =
      state1.Transform().ScrollTranslationForFixed();
  const auto* fixed_scroll_translation2 =
      state2.Transform().ScrollTranslationForFixed();
  if (fixed_scroll_translation1 == scroll_translation2 &&
      &state1.Clip() == scroll_translation2->ScrollNode()->OverflowClipNode()) {
    ExpandFixedVisualRectInScroller(*fixed_scroll_translation1, new_rect1);
  } else if (scroll_translation1 == fixed_scroll_translation2 &&
             &state2.Clip() ==
                 scroll_translation1->ScrollNode()->OverflowClipNode()) {
    ExpandFixedVisualRectInScroller(*fixed_scroll_translation2, new_rect2);
  } else {
    const auto& transform_lca =
        state1.Transform().LowestCommonAncestor(state2.Transform()).Unalias();
    const auto& scroll_translation_lca =
        transform_lca.NearestScrollTranslationNode();
    bool between_fixed_and_non_fixed = false;

    // If we will test overlap across scroll translations, adjust each property
    // tree state to be the parent of the highest scroll translation under
    // |transform_lca| along the ancestor path, and the visual rect to be the
    // scroll container rect, assuming the visual rect under the scroll
    // translation can be anywhere in the scroll container rect, thus we can
    // avoid re-testing overlap on change of scroll offset.
    auto adjust_rect_and_state =
        [&scroll_translation_lca, &between_fixed_and_non_fixed](
            const TransformPaintPropertyNode* scroll_translation,
            const TransformPaintPropertyNode* other_fixed_scroll_translation,
            gfx::RectF& rect, PropertyTreeState& state) {
          if (scroll_translation == &scroll_translation_lca)
            return;

          auto* parent = scroll_translation->UnaliasedParent();
          DCHECK(parent);
          for (auto* next = &parent->NearestScrollTranslationNode();
               next != &scroll_translation_lca;
               next = &parent->NearestScrollTranslationNode()) {
            if (next == other_fixed_scroll_translation) {
              between_fixed_and_non_fixed = true;
              break;
            }
            scroll_translation = next;
            parent = scroll_translation->UnaliasedParent();
            DCHECK(parent);
          }
          rect = gfx::RectF(scroll_translation->ScrollNode()->ContainerRect());
          state.SetTransform(*parent);
          if (auto* clip = scroll_translation->ScrollNode()->OverflowClipNode())
            state.SetClip(*clip->UnaliasedParent());
          else
            state.SetClip(ClipPaintPropertyNode::Root());
        };

    adjust_rect_and_state(scroll_translation1, fixed_scroll_translation2,
                          new_rect1, new_state1);
    if (between_fixed_and_non_fixed) {
      ExpandFixedVisualRectInScroller(*fixed_scroll_translation2, new_rect2);
    } else {
      adjust_rect_and_state(scroll_translation2, fixed_scroll_translation1,
                            new_rect2, new_state2);
      if (between_fixed_and_non_fixed)
        ExpandFixedVisualRectInScroller(*fixed_scroll_translation1, new_rect1);
    }
  }

  return MightOverlapForCompositingInternal(new_rect1, new_state1, new_rect2,
                                            new_state2);
}

bool GeometryMapper::MightOverlapForCompositingInternal(
    const gfx::RectF& rect1,
    const PropertyTreeState& state1,
    const gfx::RectF& rect2,
    const PropertyTreeState& state2) {
  PropertyTreeState common_ancestor(
      state1.Transform().LowestCommonAncestor(state2.Transform()).Unalias(),
      state1.Clip().LowestCommonAncestor(state2.Clip()).Unalias(),
      EffectPaintPropertyNode::Root());
  auto v1 = VisualRectForCompositingOverlap(rect1, state1, common_ancestor);
  auto v2 = VisualRectForCompositingOverlap(rect2, state2, common_ancestor);
  return v1.Intersects(v2);
}

const ClipPaintPropertyNode* GeometryMapper::HighestOutputClipBetween(
    const EffectPaintPropertyNode& ancestor,
    const EffectPaintPropertyNode& descendant) {
  const ClipPaintPropertyNode* result = nullptr;
  for (const auto* effect = &descendant; effect != &ancestor;
       effect = effect->UnaliasedParent()) {
    if (const auto* output_clip = effect->OutputClip())
      result = &output_clip->Unalias();
  }
  return result;
}

gfx::RectF GeometryMapper::VisualRectForCompositingOverlap(
    const gfx::RectF& local_rect,
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state) {
  FloatClipRect visual_rect(local_rect);
  GeometryMapper::LocalToAncestorVisualRectInternal<
      ForCompositingOverlap::kYes>(local_state, ancestor_state, visual_rect,
                                   kIgnoreOverlayScrollbarSize,
                                   kNonInclusiveIntersect);
  return visual_rect.Rect();
}

bool GeometryMapper::LocalToAncestorVisualRectInternalForTesting(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& mapping_rect) {
  return GeometryMapper::LocalToAncestorVisualRectInternal<
      ForCompositingOverlap::kNo>(local_state, ancestor_state, mapping_rect,
                                  kIgnoreOverlayScrollbarSize,
                                  kNonInclusiveIntersect);
}

bool GeometryMapper::
    LocalToAncestorVisualRectInternalForCompositingOverlapForTesting(
        const PropertyTreeState& local_state,
        const PropertyTreeState& ancestor_state,
        FloatClipRect& mapping_rect) {
  return GeometryMapper::LocalToAncestorVisualRectInternal<
      ForCompositingOverlap::kYes>(local_state, ancestor_state, mapping_rect,
                                   kIgnoreOverlayScrollbarSize,
                                   kNonInclusiveIntersect);
}

void GeometryMapper::ClearCache() {
  GeometryMapperTransformCache::ClearCache();
  GeometryMapperClipCache::ClearCache();
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

GeometryMapper::Translation2DOrMatrix
GeometryMapper::SourceToDestinationProjection(
    const TransformPaintPropertyNode& source,
    const TransformPaintPropertyNode& destination) {
  bool has_animation = false;
  bool success = false;
  return SourceToDestinationProjectionInternal(source, destination,
                                               has_animation, success);
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
GeometryMapper::Translation2DOrMatrix
GeometryMapper::SourceToDestinationProjectionInternal(
    const TransformPaintPropertyNode& source,
    const TransformPaintPropertyNode& destination,
    bool& has_animation,
    bool& success) {
  has_animation = false;
  success = true;

  if (&source == &destination)
    return Translation2DOrMatrix();

  if (source.Parent() && &destination == &source.Parent()->Unalias()) {
    if (source.IsIdentityOr2DTranslation()) {
      // We always use full matrix for animating transforms.
      DCHECK(!source.HasActiveTransformAnimation());
      return Translation2DOrMatrix(source.Translation2D());
    }
    // The result will be translate(origin)*matrix*translate(-origin) which
    // equals to matrix if the origin is zero or if the matrix is just
    // identity or 2d translation.
    if (source.Origin().IsZero()) {
      has_animation = source.HasActiveTransformAnimation();
      return Translation2DOrMatrix(source.Matrix());
    }
  }

  if (destination.IsIdentityOr2DTranslation() && destination.Parent() &&
      &source == &destination.Parent()->Unalias()) {
    // We always use full matrix for animating transforms.
    DCHECK(!destination.HasActiveTransformAnimation());
    return Translation2DOrMatrix(-destination.Translation2D());
  }

  const auto& source_cache = source.GetTransformCache();
  const auto& destination_cache = destination.GetTransformCache();

  // Case 1a (fast path of case 1b): check if source and destination are under
  // the same 2d translation root.
  if (source_cache.root_of_2d_translation() ==
      destination_cache.root_of_2d_translation()) {
    // We always use full matrix for animating transforms.
    return Translation2DOrMatrix(source_cache.to_2d_translation_root() -
                                 destination_cache.to_2d_translation_root());
  }

  // Case 1b: Check if source and destination are known to be coplanar.
  // Even if destination may have invertible screen projection,
  // this formula is likely to be numerically more stable.
  if (source_cache.plane_root() == destination_cache.plane_root()) {
    has_animation = source_cache.has_animation_to_plane_root() ||
                    destination_cache.has_animation_to_plane_root();
    if (&source == destination_cache.plane_root()) {
      return Translation2DOrMatrix(destination_cache.from_plane_root());
    }
    if (&destination == source_cache.plane_root()) {
      return Translation2DOrMatrix(source_cache.to_plane_root());
    }
    TransformationMatrix matrix;
    destination_cache.ApplyFromPlaneRoot(matrix);
    source_cache.ApplyToPlaneRoot(matrix);
    return Translation2DOrMatrix(matrix);
  }

  // Case 2: Check if we can fallback to the canonical definition of
  // flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  // If flatten(destination_to_screen)^-1 is invalid, we are out of luck.
  // Screen transform data are updated lazily because they are rarely used.
  source.UpdateScreenTransform();
  destination.UpdateScreenTransform();
  has_animation = source_cache.has_animation_to_screen() ||
                  destination_cache.has_animation_to_screen();
  if (!destination_cache.projection_from_screen_is_valid()) {
    success = false;
    return Translation2DOrMatrix();
  }

  // Case 3: Compute:
  // flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  const auto& root = TransformPaintPropertyNode::Root();
  if (&source == &root)
    return Translation2DOrMatrix(destination_cache.projection_from_screen());
  TransformationMatrix matrix;
  destination_cache.ApplyProjectionFromScreen(matrix);
  source_cache.ApplyToScreen(matrix);
  matrix.FlattenTo2d();
  return Translation2DOrMatrix(matrix);
}

bool GeometryMapper::LocalToAncestorVisualRect(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& mapping_rect,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior,
    ExpandVisualRectForAnimationOrNot expand_for_animation) {
  bool success = false;
  bool result = LocalToAncestorVisualRectInternal(
      local_state, ancestor_state, mapping_rect, clip_behavior,
      inclusive_behavior, expand_for_animation, success);
  DCHECK(success);
  return result;
}

bool GeometryMapper::LocalToAncestorVisualRectInternal(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& rect_to_map,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior,
    ExpandVisualRectForAnimationOrNot expand_for_animation,
    bool& success) {
  if (local_state == ancestor_state) {
    success = true;
    return true;
  }

  if (&local_state.Effect() != &ancestor_state.Effect()) {
    return SlowLocalToAncestorVisualRectWithEffects(
        local_state, ancestor_state, rect_to_map, clip_behavior,
        inclusive_behavior, expand_for_animation, success);
  }

  bool has_animation = false;
  const auto& translation_2d_or_matrix = SourceToDestinationProjectionInternal(
      local_state.Transform(), ancestor_state.Transform(), has_animation,
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
    success = true;
    rect_to_map = FloatClipRect(FloatRect());
    return false;
  }

  if (has_animation && expand_for_animation == kExpandVisualRectForAnimation) {
    // Assume during the animation the transform can map |rect_to_map| to
    // anywhere. Ancestor clips will still apply.
    // TODO(crbug.com/1026653): Use animation bounds instead of infinite rect.
    rect_to_map = InfiniteLooseFloatClipRect();
  } else {
    translation_2d_or_matrix.MapFloatClipRect(rect_to_map);
  }

  FloatClipRect clip_rect = LocalToAncestorClipRectInternal(
      local_state.Clip(), ancestor_state.Clip(), ancestor_state.Transform(),
      clip_behavior, inclusive_behavior, expand_for_animation, success);
  if (success) {
    // This is where we propagate the roundedness and tightness of |clip_rect|
    // to |rect_to_map|.
    if (inclusive_behavior == kInclusiveIntersect)
      return rect_to_map.InclusiveIntersect(clip_rect);
    rect_to_map.Intersect(clip_rect);
    return !rect_to_map.Rect().IsEmpty();
  }

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // On SPv1 we may fail when the paint invalidation container creates an
    // overflow clip (in ancestor_state) which is not in localState of an
    // out-of-flow positioned descendant. See crbug.com/513108 and web test
    // compositing/overflow/handle-non-ancestor-clip-parent.html (run with
    // --enable-prefer-compositing-to-lcd-text) for details.
    // Ignore it for SPv1 for now.
    success = true;
    rect_to_map.ClearIsTight();
  }
  return !rect_to_map.Rect().IsEmpty();
}

bool GeometryMapper::SlowLocalToAncestorVisualRectWithEffects(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& mapping_rect,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior,
    ExpandVisualRectForAnimationOrNot expand_for_animation,
    bool& success) {
  PropertyTreeState last_transform_and_clip_state(
      local_state.Transform(), local_state.Clip(),
      EffectPaintPropertyNode::Root());

  const auto& ancestor_effect = ancestor_state.Effect();
  for (const auto* effect = &local_state.Effect();
       effect && effect != &ancestor_effect;
       effect = effect->UnaliasedParent()) {
    if (effect->HasActiveFilterAnimation() &&
        expand_for_animation == kExpandVisualRectForAnimation) {
      // Assume during the animation the filter can map |rect_to_map| to
      // anywhere. Ancestor clips will still apply.
      // TODO(crbug.com/1026653): Use animation bounds instead of infinite rect.
      mapping_rect = InfiniteLooseFloatClipRect();
      last_transform_and_clip_state.SetTransform(
          effect->LocalTransformSpace().Unalias());
      last_transform_and_clip_state.SetClip(effect->OutputClip()->Unalias());
      continue;
    }

    if (!effect->HasFilterThatMovesPixels())
      continue;

    PropertyTreeState transform_and_clip_state(
        effect->LocalTransformSpace().Unalias(),
        effect->OutputClip()->Unalias(), EffectPaintPropertyNode::Root());
    bool intersects = LocalToAncestorVisualRectInternal(
        last_transform_and_clip_state, transform_and_clip_state, mapping_rect,
        clip_behavior, inclusive_behavior, expand_for_animation, success);
    if (!success || !intersects) {
      success = true;
      mapping_rect = FloatClipRect(FloatRect());
      return false;
    }

    mapping_rect = FloatClipRect(effect->MapRect(mapping_rect.Rect()));
    last_transform_and_clip_state = transform_and_clip_state;
  }

  PropertyTreeState final_transform_and_clip_state(
      ancestor_state.Transform(), ancestor_state.Clip(),
      EffectPaintPropertyNode::Root());
  bool intersects = LocalToAncestorVisualRectInternal(
      last_transform_and_clip_state, final_transform_and_clip_state,
      mapping_rect, clip_behavior, inclusive_behavior, expand_for_animation,
      success);

  // Many effects (e.g. filters, clip-paths) can make a clip rect not tight.
  mapping_rect.ClearIsTight();
  return intersects;
}

FloatClipRect GeometryMapper::LocalToAncestorClipRect(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    OverlayScrollbarClipBehavior clip_behavior) {
  const auto& local_clip = local_state.Clip();
  const auto& ancestor_clip = ancestor_state.Clip();
  if (&local_clip == &ancestor_clip)
    return FloatClipRect();

  bool success = false;
  auto result = LocalToAncestorClipRectInternal(
      local_clip, ancestor_clip, ancestor_state.Transform(), clip_behavior,
      kNonInclusiveIntersect, kDontExpandVisualRectForAnimation, success);
  DCHECK(success);

  // Many effects (e.g. filters, clip-paths) can make a clip rect not tight.
  if (&local_state.Effect() != &ancestor_state.Effect())
    result.ClearIsTight();

  return result;
}

static FloatClipRect GetClipRect(const ClipPaintPropertyNode& clip_node,
                                 OverlayScrollbarClipBehavior clip_behavior) {
  FloatClipRect clip_rect(
      UNLIKELY(clip_behavior == kExcludeOverlayScrollbarSizeForHitTesting)
          ? clip_node.UnsnappedClipRectExcludingOverlayScrollbars()
          : FloatClipRect(clip_node.UnsnappedClipRect()));
  if (clip_node.ClipPath())
    clip_rect.ClearIsTight();
  return clip_rect;
}

FloatClipRect GeometryMapper::LocalToAncestorClipRectInternal(
    const ClipPaintPropertyNode& descendant_clip,
    const ClipPaintPropertyNode& ancestor_clip,
    const TransformPaintPropertyNode& ancestor_transform,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior,
    ExpandVisualRectForAnimationOrNot expand_for_animation,
    bool& success) {
  if (&descendant_clip == &ancestor_clip) {
    success = true;
    return FloatClipRect();
  }
  if (descendant_clip.UnaliasedParent() == &ancestor_clip &&
      &descendant_clip.LocalTransformSpace() == &ancestor_transform) {
    success = true;
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

    if (cached_clip && cached_clip->has_transform_animation &&
        expand_for_animation == kExpandVisualRectForAnimation) {
      // Don't use cached clip if it's transformed by any animating transform.
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
    success = false;
    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // On SPv1 we may fail when the paint invalidation container creates an
      // overflow clip (in ancestor_state) which is not in localState of an
      // out-of-flow positioned descendant. See crbug.com/513108 and layout
      // test compositing/overflow/handle-non-ancestor-clip-parent.html (run
      // with --enable-prefer-compositing-to-lcd-text) for details.
      // Ignore it for SPv1 for now.
      success = true;
    }
    return InfiniteLooseFloatClipRect();
  }

  // Iterate down from the top intermediate node found in the previous loop,
  // computing and memoizing clip rects as we go.
  for (auto it = intermediate_nodes.rbegin(); it != intermediate_nodes.rend();
       ++it) {
    bool has_animation = false;
    const auto& translation_2d_or_matrix =
        SourceToDestinationProjectionInternal(
            (*it)->LocalTransformSpace().Unalias(), ancestor_transform,
            has_animation, success);
    if (!success) {
      success = true;
      return FloatClipRect(FloatRect());
    }

    // Don't apply this clip if it's transformed by any animating transform.
    if (has_animation && expand_for_animation == kExpandVisualRectForAnimation)
      continue;

    // This is where we generate the roundedness and tightness of clip rect
    // from clip and transform properties, and propagate them to |clip|.
    FloatClipRect mapped_rect(GetClipRect(**it, clip_behavior));
    translation_2d_or_matrix.MapFloatClipRect(mapped_rect);
    if (inclusive_behavior == kInclusiveIntersect) {
      clip.InclusiveIntersect(mapped_rect);
    } else {
      clip.Intersect(mapped_rect);
      // Inclusive intersected clips are not cached at present.
      (*it)->GetClipCache().SetCachedClip(
          GeometryMapperClipCache::ClipCacheEntry{clip_and_transform, clip,
                                                  has_animation});
    }
  }
  // Clips that are inclusive intersected or expanded for animation are not
  // cached at present.
  DCHECK(inclusive_behavior == kInclusiveIntersect ||
         expand_for_animation == kExpandVisualRectForAnimation ||
         descendant_clip.GetClipCache()
                 .GetCachedClip(clip_and_transform)
                 ->clip_rect == clip);
  success = true;
  return clip;
}

void GeometryMapper::ClearCache() {
  GeometryMapperTransformCache::ClearCache();
  GeometryMapperClipCache::ClearCache();
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

const TransformationMatrix& GeometryMapper::SourceToDestinationProjection(
    const TransformPaintPropertyNode* source,
    const TransformPaintPropertyNode* destination) {
  DCHECK(source && destination);
  bool success = false;
  const auto& result =
      SourceToDestinationProjectionInternal(source, destination, success);
  return result;
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
const TransformationMatrix&
GeometryMapper::SourceToDestinationProjectionInternal(
    const TransformPaintPropertyNode* source,
    const TransformPaintPropertyNode* destination,
    bool& success) {
  DCHECK(source && destination);
  DEFINE_STATIC_LOCAL(TransformationMatrix, identity, ());
  DEFINE_STATIC_LOCAL(TransformationMatrix, temp, ());

  source = source->Unalias();
  destination = destination->Unalias();

  if (source == destination) {
    success = true;
    return identity;
  }

  if (source->Parent() && destination == source->Parent()->Unalias() &&
      // The result will be translate(origin)*matrix*translate(-origin) which
      // equals to matrix if the origin is zero or if the matrix is just
      // identity or 2d translation.
      (source->Origin().IsZero() || source->IsIdentityOr2DTranslation())) {
    success = true;
    return source->Matrix();
  }

  const GeometryMapperTransformCache& source_cache =
      source->GetTransformCache();
  const GeometryMapperTransformCache& destination_cache =
      destination->GetTransformCache();

  // Case 1a (fast path of case 1b): check if source and destination are under
  // the same 2d translation root.
  if (source_cache.root_of_2d_translation() ==
      destination_cache.root_of_2d_translation()) {
    success = true;
    if (source == destination_cache.root_of_2d_translation())
      return destination_cache.from_2d_translation_root();
    if (destination == source_cache.root_of_2d_translation())
      return source_cache.to_2d_translation_root();
    temp = destination_cache.from_2d_translation_root();
    temp.Translate(source_cache.to_2d_translation_root().E(),
                   source_cache.to_2d_translation_root().F());
    return temp;
  }

  // Case 1b: Check if source and destination are known to be coplanar.
  // Even if destination may have invertible screen projection,
  // this formula is likely to be numerically more stable.
  if (source_cache.plane_root() == destination_cache.plane_root()) {
    success = true;
    if (source == destination_cache.plane_root())
      return destination_cache.from_plane_root();
    if (destination == source_cache.plane_root())
      return source_cache.to_plane_root();
    temp = destination_cache.from_plane_root();
    temp.Multiply(source_cache.to_plane_root());
    return temp;
  }

  // Case 2: Check if we can fallback to the canonical definition of
  // flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  // If flatten(destination_to_screen)^-1 is invalid, we are out of luck.
  // Screen transform data are updated lazily because they are rarely used.
  source->UpdateScreenTransform();
  destination->UpdateScreenTransform();
  if (!destination_cache.projection_from_screen_is_valid()) {
    success = false;
    return identity;
  }

  // Case 3: Compute:
  // flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  const auto* root = &TransformPaintPropertyNode::Root();
  success = true;
  if (source == root)
    return destination_cache.projection_from_screen();
  if (destination == root) {
    temp = source_cache.to_screen();
  } else {
    temp = destination_cache.projection_from_screen();
    temp.Multiply(source_cache.to_screen());
  }
  temp.FlattenTo2d();
  return temp;
}

bool GeometryMapper::LocalToAncestorVisualRect(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& mapping_rect,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior) {
  bool success = false;
  bool result = LocalToAncestorVisualRectInternal(local_state, ancestor_state,
                                                  mapping_rect, clip_behavior,
                                                  inclusive_behavior, success);
  DCHECK(success);
  return result;
}

bool GeometryMapper::PointVisibleInAncestorSpace(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    const FloatPoint& local_point) {
  auto* ancestor_clip = ancestor_state.Clip()->Unalias();
  for (const auto* clip = local_state.Clip()->Unalias();
       clip && clip != ancestor_clip; clip = SafeUnalias(clip->Parent())) {
    FloatPoint mapped_point =
        SourceToDestinationProjection(local_state.Transform(),
                                      clip->LocalTransformSpace())
            .MapPoint(local_point);

    if (!clip->ClipRect().IntersectsQuad(
            FloatRect(mapped_point, FloatSize(1, 1))))
      return false;

    if (clip->ClipPath() && !clip->ClipPath()->Contains(mapped_point))
      return false;
  }

  return true;
}

bool GeometryMapper::LocalToAncestorVisualRectInternal(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& rect_to_map,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior,
    bool& success) {
  if (local_state == ancestor_state) {
    success = true;
    return true;
  }

  if (SafeUnalias(local_state.Effect()) !=
      SafeUnalias(ancestor_state.Effect())) {
    return SlowLocalToAncestorVisualRectWithEffects(
        local_state, ancestor_state, rect_to_map, clip_behavior,
        inclusive_behavior, success);
  }

  const auto& transform_matrix = SourceToDestinationProjectionInternal(
      local_state.Transform(), ancestor_state.Transform(), success);
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
  rect_to_map.Map(transform_matrix);

  FloatClipRect clip_rect = LocalToAncestorClipRectInternal(
      local_state.Clip(), ancestor_state.Clip(), ancestor_state.Transform(),
      clip_behavior, inclusive_behavior, success);
  if (success) {
    // This is where we propagate the roundedness and tightness of |clip_rect|
    // to |rect_to_map|.
    if (inclusive_behavior == kInclusiveIntersect)
      return rect_to_map.InclusiveIntersect(clip_rect);
    rect_to_map.Intersect(clip_rect);
    return !rect_to_map.Rect().IsEmpty();
  }

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // On SPv1 we may fail when the paint invalidation container creates an
    // overflow clip (in ancestor_state) which is not in localState of an
    // out-of-flow positioned descendant. See crbug.com/513108 and layout test
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
    bool& success) {
  PropertyTreeState last_transform_and_clip_state(local_state.Transform(),
                                                  local_state.Clip(), nullptr);

  auto* ancestor_effect = ancestor_state.Effect()->Unalias();
  for (const auto* effect = local_state.Effect()->Unalias();
       effect && effect != ancestor_effect;
       effect = SafeUnalias(effect->Parent())) {
    if (!effect->HasFilterThatMovesPixels())
      continue;

    DCHECK(effect->OutputClip());
    PropertyTreeState transform_and_clip_state(effect->LocalTransformSpace(),
                                               effect->OutputClip(), nullptr);
    bool intersects = LocalToAncestorVisualRectInternal(
        last_transform_and_clip_state, transform_and_clip_state, mapping_rect,
        clip_behavior, inclusive_behavior, success);
    if (!success || !intersects) {
      success = true;
      mapping_rect = FloatClipRect(FloatRect());
      return false;
    }

    mapping_rect = FloatClipRect(effect->MapRect(mapping_rect.Rect()));
    last_transform_and_clip_state = transform_and_clip_state;
  }

  PropertyTreeState final_transform_and_clip_state(
      ancestor_state.Transform(), ancestor_state.Clip(), nullptr);
  LocalToAncestorVisualRectInternal(
      last_transform_and_clip_state, final_transform_and_clip_state,
      mapping_rect, clip_behavior, inclusive_behavior, success);

  // Many effects (e.g. filters, clip-paths) can make a clip rect not tight.
  mapping_rect.ClearIsTight();
  return true;
}

FloatClipRect GeometryMapper::LocalToAncestorClipRect(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    OverlayScrollbarClipBehavior clip_behavior) {
  if (local_state.Clip()->Unalias() == ancestor_state.Clip()->Unalias())
    return FloatClipRect();

  bool success = false;
  auto result = LocalToAncestorClipRectInternal(
      local_state.Clip(), ancestor_state.Clip(), ancestor_state.Transform(),
      clip_behavior, kNonInclusiveIntersect, success);
  DCHECK(success);

  // Many effects (e.g. filters, clip-paths) can make a clip rect not tight.
  if (SafeUnalias(local_state.Effect()) != SafeUnalias(ancestor_state.Effect()))
    result.ClearIsTight();

  return result;
}

static FloatClipRect GetClipRect(const ClipPaintPropertyNode* clip_node,
                                 OverlayScrollbarClipBehavior clip_behavior) {
  clip_node = clip_node->Unalias();
  FloatClipRect clip_rect(
      UNLIKELY(clip_behavior == kExcludeOverlayScrollbarSizeForHitTesting)
          ? clip_node->ClipRectExcludingOverlayScrollbars()
          : clip_node->ClipRect());
  if (clip_node->ClipPath())
    clip_rect.ClearIsTight();
  return clip_rect;
}

FloatClipRect GeometryMapper::LocalToAncestorClipRectInternal(
    const ClipPaintPropertyNode* descendant,
    const ClipPaintPropertyNode* ancestor_clip,
    const TransformPaintPropertyNode* ancestor_transform,
    OverlayScrollbarClipBehavior clip_behavior,
    InclusiveIntersectOrNot inclusive_behavior,
    bool& success) {
  descendant = descendant->Unalias();
  ancestor_clip = ancestor_clip->Unalias();
  if (descendant == ancestor_clip) {
    success = true;
    return FloatClipRect();
  }
  ancestor_transform = ancestor_transform->Unalias();
  if (SafeUnalias(descendant->Parent()) == ancestor_clip &&
      descendant->LocalTransformSpace() == ancestor_transform) {
    success = true;
    return GetClipRect(descendant, clip_behavior);
  }

  FloatClipRect clip;
  const ClipPaintPropertyNode* clip_node = descendant;
  Vector<const ClipPaintPropertyNode*> intermediate_nodes;

  GeometryMapperClipCache::ClipAndTransform clip_and_transform(
      ancestor_clip, ancestor_transform, clip_behavior);
  // Iterate over the path from localState.clip to ancestor_state.clip. Stop if
  // we've found a memoized (precomputed) clip for any particular node.
  while (clip_node && clip_node != ancestor_clip) {
    const FloatClipRect* cached_clip = nullptr;
    // Inclusive intersected clips are not cached at present.
    if (inclusive_behavior != kInclusiveIntersect)
      cached_clip = clip_node->GetClipCache().GetCachedClip(clip_and_transform);

    if (cached_clip) {
      clip = *cached_clip;
      break;
    }

    intermediate_nodes.push_back(clip_node);
    clip_node = SafeUnalias(clip_node->Parent());
  }
  if (!clip_node) {
    success = false;
    if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      // On SPv1 we may fail when the paint invalidation container creates an
      // overflow clip (in ancestor_state) which is not in localState of an
      // out-of-flow positioned descendant. See crbug.com/513108 and layout
      // test compositing/overflow/handle-non-ancestor-clip-parent.html (run
      // with --enable-prefer-compositing-to-lcd-text) for details.
      // Ignore it for SPv1 for now.
      success = true;
    }
    FloatClipRect loose_infinite;
    loose_infinite.ClearIsTight();
    return loose_infinite;
  }

  // Iterate down from the top intermediate node found in the previous loop,
  // computing and memoizing clip rects as we go.
  for (auto it = intermediate_nodes.rbegin(); it != intermediate_nodes.rend();
       ++it) {
    const TransformationMatrix& transform_matrix =
        SourceToDestinationProjectionInternal((*it)->LocalTransformSpace(),
                                              ancestor_transform, success);
    if (!success) {
      success = true;
      return FloatClipRect(FloatRect());
    }

    // This is where we generate the roundedness and tightness of clip rect
    // from clip and transform properties, and propagate them to |clip|.
    FloatClipRect mapped_rect(GetClipRect((*it), clip_behavior));
    mapped_rect.Map(transform_matrix);
    if (inclusive_behavior == kInclusiveIntersect) {
      clip.InclusiveIntersect(mapped_rect);
    } else {
      clip.Intersect(mapped_rect);
      // Inclusive intersected clips are not cached at present.
      (*it)->GetClipCache().SetCachedClip(clip_and_transform, clip);
    }
  }
  // Inclusive intersected clips are not cached at present.
  DCHECK(inclusive_behavior == kInclusiveIntersect ||
         *descendant->GetClipCache().GetCachedClip(clip_and_transform) == clip);
  success = true;
  return clip;
}

void GeometryMapper::ClearCache() {
  GeometryMapperTransformCache::ClearCache();
  GeometryMapperClipCache::ClearCache();
}

}  // namespace blink

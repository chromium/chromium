// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/pending_layer.h"

#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

bool PendingLayer::PropertyTreeStateChanged() const {
  auto change = PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  if (change_of_decomposited_transforms_ >= change)
    return true;

  return property_tree_state_.ChangedToRoot(change);
}

PendingLayer::PendingLayer(const PaintChunkSubset& chunks,
                           const PaintChunkIterator& first_chunk,
                           CompositingType compositing_type,
                           bool effectively_invisible)
    : bounds_(first_chunk->bounds),
      rect_known_to_be_opaque_(first_chunk->rect_known_to_be_opaque),
      text_known_to_be_on_opaque_background_(
          first_chunk->text_known_to_be_on_opaque_background),
      effectively_invisible_(effectively_invisible),
      chunks_(&chunks.GetPaintArtifact(), first_chunk.IndexInPaintArtifact()),
      property_tree_state_(
          first_chunk->properties.GetPropertyTreeState().Unalias()),
      compositing_type_(compositing_type) {
  DCHECK(!RequiresOwnLayer() || first_chunk->size() <= 1u);
}

PendingLayer::PendingLayer(const PreCompositedLayerInfo& pre_composited_layer)
    : chunks_(pre_composited_layer.chunks),
      property_tree_state_(
          pre_composited_layer.graphics_layer->GetPropertyTreeState()
              .Unalias()),
      graphics_layer_(pre_composited_layer.graphics_layer),
      compositing_type_(kPreCompositedLayer) {
  DCHECK(graphics_layer_);
  DCHECK(!graphics_layer_->ShouldCreateLayersAfterPaint());
}

FloatRect PendingLayer::MapRectKnownToBeOpaque(
    const PropertyTreeState& new_state) const {
  if (rect_known_to_be_opaque_.IsEmpty())
    return FloatRect();

  FloatClipRect float_clip_rect(rect_known_to_be_opaque_);
  GeometryMapper::LocalToAncestorVisualRect(property_tree_state_, new_state,
                                            float_clip_rect);
  return float_clip_rect.IsTight() ? float_clip_rect.Rect() : FloatRect();
}

std::unique_ptr<JSONObject> PendingLayer::ToJSON() const {
  std::unique_ptr<JSONObject> result = std::make_unique<JSONObject>();
  result->SetArray("bounds", RectAsJSONArray(bounds_));
  result->SetArray("rect_known_to_be_opaque",
                   RectAsJSONArray(rect_known_to_be_opaque_));
  result->SetObject("property_tree_state", property_tree_state_.ToJSON());
  result->SetArray("offset_of_decomposited_transforms",
                   PointAsJSONArray(offset_of_decomposited_transforms_));
  std::unique_ptr<JSONArray> json_chunks = std::make_unique<JSONArray>();
  for (auto it = chunks_.begin(); it != chunks_.end(); ++it) {
    StringBuilder sb;
    sb.Append("index=");
    sb.AppendNumber(it.IndexInPaintArtifact());
    sb.Append(" ");
    sb.Append(it->ToString());
    json_chunks->PushString(sb.ToString());
  }
  result->SetArray("paint_chunks", std::move(json_chunks));
  return result;
}

FloatRect PendingLayer::VisualRectForOverlapTesting(
    const PropertyTreeState& ancestor_state) const {
  FloatClipRect visual_rect(bounds_);
  GeometryMapper::LocalToAncestorVisualRect(
      property_tree_state_, ancestor_state, visual_rect,
      kIgnoreOverlayScrollbarSize, kNonInclusiveIntersect,
      kExpandVisualRectForCompositingOverlap);
  return visual_rect.Rect();
}

void PendingLayer::Upcast(const PropertyTreeState& new_state) {
  DCHECK(!RequiresOwnLayer());
  if (property_tree_state_ == new_state)
    return;

  FloatClipRect float_clip_rect(bounds_);
  GeometryMapper::LocalToAncestorVisualRect(property_tree_state_, new_state,
                                            float_clip_rect);
  bounds_ = float_clip_rect.Rect();

  rect_known_to_be_opaque_ = MapRectKnownToBeOpaque(new_state);
  property_tree_state_ = new_state;
}

const PaintChunk& PendingLayer::FirstPaintChunk() const {
  DCHECK(!RequiresOwnLayer() || chunks_.size() == 1);
  return *chunks_.begin();
}

const DisplayItem& PendingLayer::FirstDisplayItem() const {
#if DCHECK_IS_ON()
  // This method should never be called if the first paint chunk is empty.
  if (RequiresOwnLayer())
    DCHECK_EQ(FirstPaintChunk().size(), 1u);
  else
    DCHECK_GE(FirstPaintChunk().size(), 1u);
#endif
  return *chunks_.begin().DisplayItems().begin();
}

bool PendingLayer::MayDrawContent() const {
  return Chunks().size() > 1 || FirstPaintChunk().size() > 0;
}

// We will only allow merging if the merged-area:home-area+guest-area doesn't
// exceed the ratio |kMergingSparsityTolerance|:1.
static constexpr float kMergeSparsityTolerance = 6;

bool PendingLayer::MergeInternal(const PendingLayer& guest,
                                 const PropertyTreeState& guest_state,
                                 bool dry_run) {
  if (&Chunks().GetPaintArtifact() != &guest.Chunks().GetPaintArtifact())
    return false;
  if (RequiresOwnLayer() || guest.RequiresOwnLayer())
    return false;
  if (&GetPropertyTreeState().Effect() != &guest_state.Effect())
    return false;
  if (EffectivelyInvisible() != guest.EffectivelyInvisible())
    return false;

  const absl::optional<PropertyTreeState>& merged_state =
      GetPropertyTreeState().CanUpcastWith(guest_state);
  if (!merged_state)
    return false;

  FloatClipRect new_home_bounds(Bounds());
  GeometryMapper::LocalToAncestorVisualRect(GetPropertyTreeState(),
                                            *merged_state, new_home_bounds);
  FloatClipRect new_guest_bounds(guest.Bounds());
  GeometryMapper::LocalToAncestorVisualRect(guest_state, *merged_state,
                                            new_guest_bounds);

  FloatRect merged_bounds =
      UnionRect(new_home_bounds.Rect(), new_guest_bounds.Rect());
  // Don't check for sparcity if we may further decomposite the effect, so that
  // the merged layer may be merged to other layers with the decomposited
  // effect, which is often better than not merging even if the merged layer is
  // sparse because we may create less composited effects and render surfaces.
  if (guest_state.Effect().IsRoot() ||
      guest_state.Effect().HasDirectCompositingReasons()) {
    float sum_area = new_home_bounds.Rect().Size().Area() +
                     new_guest_bounds.Rect().Size().Area();
    if (merged_bounds.Size().Area() > kMergeSparsityTolerance * sum_area)
      return false;
  }

  if (!dry_run) {
    chunks_.Merge(guest.Chunks());
    bounds_ = merged_bounds;
    rect_known_to_be_opaque_ =
        MaximumCoveredRect(MapRectKnownToBeOpaque(*merged_state),
                           guest.MapRectKnownToBeOpaque(*merged_state));
    property_tree_state_ = *merged_state;
    text_known_to_be_on_opaque_background_ &=
        (guest.TextKnownToBeOnOpaqueBackground() ||
         RectKnownToBeOpaque().Contains(new_guest_bounds.Rect()));
    change_of_decomposited_transforms_ =
        std::max(ChangeOfDecompositedTransforms(),
                 guest.ChangeOfDecompositedTransforms());
  }
  return true;
}

const TransformPaintPropertyNode*
PendingLayer::ScrollTranslationForScrollHitTestLayer() const {
  // Not checking that the compositing type is
  // PendingLayer::kCompositedScrollHitTestLayer because a scroll hit test
  // chunk without a direct compositing reasons can still be composited
  // (e.g. when it can't be merged into any other layer).
  DCHECK_NE(GetCompositingType(), PendingLayer::kPreCompositedLayer);

  if (Chunks().size() != 1)
    return nullptr;

  const auto& paint_chunk = FirstPaintChunk();
  if (!paint_chunk.hit_test_data)
    return nullptr;
  return paint_chunk.hit_test_data->scroll_translation;
}

// Walk the pending layer list and build up a table of transform nodes that
// can be de-composited (replaced with offset_to_transform_parent). A
// transform node can be de-composited if:
//  1. It is not the root transform node.
//  2. It is a 2d translation only.
//  3. The transform is not used for scrolling - its ScrollNode() is nullptr.
//  4. The transform is not a StickyTranslation node.
//  5. It has no direct compositing reasons, other than k3DTransform. Note
//     that if it has a k3DTransform reason, check #2 above ensures that it
//     isn't really 3D.
//  6. It has FlattensInheritedTransform matching that of its direct parent.
//  7. It has backface visibility matching its direct parent.
//  8. No clips have local_transform_space referring to this transform node.
//  9. No effects have local_transform_space referring to this transform node.
//  10. All child transform nodes are also able to be de-composited.
// This algorithm should be O(t+c+e) where t,c,e are the number of transform,
// clip, and effect nodes in the full tree.
void PendingLayer::DecompositeTransforms(Vector<PendingLayer>& pending_layers) {
  HashMap<const TransformPaintPropertyNode*, bool> can_be_decomposited;
  HashSet<const void*> clips_and_effects_seen;
  for (const PendingLayer& pending_layer : pending_layers) {
    const auto& property_state = pending_layer.GetPropertyTreeState();

    // Lambda to handle marking a transform node false, and walking up all
    // true parents and marking them false as well. This also handles
    // inserting transform_node if it isn't in the map, and keeps track of
    // clips or effects.
    auto mark_not_decompositable =
        [&can_be_decomposited](
            const TransformPaintPropertyNode* transform_node) {
          DCHECK(transform_node);
          while (transform_node && !transform_node->IsRoot()) {
            auto result = can_be_decomposited.insert(transform_node, false);
            if (!result.is_new_entry) {
              if (!result.stored_value->value)
                break;
              result.stored_value->value = false;
            }
            transform_node = &transform_node->Parent()->Unalias();
          }
        };

    // Add the transform and all transform parents to the map.
    for (const auto* node = &property_state.Transform();
         !node->IsRoot() && !can_be_decomposited.Contains(node);
         node = &node->Parent()->Unalias()) {
      if (!node->IsIdentityOr2DTranslation() || node->ScrollNode() ||
          node->GetStickyConstraint() ||
          node->IsAffectedByOuterViewportBoundsDelta() ||
          node->HasDirectCompositingReasonsOtherThan3dTransform() ||
          !node->FlattensInheritedTransformSameAsParent() ||
          !node->BackfaceVisibilitySameAsParent()) {
        mark_not_decompositable(node);
        break;
      }
      can_be_decomposited.insert(node, true);
    }

    // Add clips and effects, and their parents, that we haven't already seen.
    for (const auto* node = &property_state.Clip();
         !node->IsRoot() && !clips_and_effects_seen.Contains(node);
         node = &node->Parent()->Unalias()) {
      clips_and_effects_seen.insert(node);
      mark_not_decompositable(&node->LocalTransformSpace().Unalias());
    }
    for (const auto* node = &property_state.Effect();
         !node->IsRoot() && !clips_and_effects_seen.Contains(node);
         node = &node->Parent()->Unalias()) {
      clips_and_effects_seen.insert(node);
      mark_not_decompositable(&node->LocalTransformSpace().Unalias());
    }

    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // The scroll translation node of a scroll hit test layer may not be
      // referenced by any pending layer's property tree state. Disallow
      // decomposition of it (and its ancestors).
      if (const auto* translation =
              pending_layer.ScrollTranslationForScrollHitTestLayer()) {
        mark_not_decompositable(translation);
      }
    }
  }

  // Now, for any transform nodes that can be de-composited, re-map their
  // transform to point to the correct parent, and set the
  // offset_to_transform_parent.
  for (PendingLayer& pending_layer : pending_layers) {
    const auto* transform = &pending_layer.GetPropertyTreeState().Transform();
    while (!transform->IsRoot() && can_be_decomposited.at(transform)) {
      pending_layer.offset_of_decomposited_transforms_ +=
          transform->Translation2D();
      pending_layer.change_of_decomposited_transforms_ =
          std::max(pending_layer.ChangeOfDecompositedTransforms(),
                   transform->NodeChanged());
      transform = &transform->Parent()->Unalias();
    }
    pending_layer.property_tree_state_.SetTransform(*transform);
    // Move bounds into the new transform space.
    pending_layer.bounds_.MoveBy(
        pending_layer.OffsetOfDecompositedTransforms());
    pending_layer.rect_known_to_be_opaque_.MoveBy(
        pending_layer.OffsetOfDecompositedTransforms());
  }
}

}  // namespace blink

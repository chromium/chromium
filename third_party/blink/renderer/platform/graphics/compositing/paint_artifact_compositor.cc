// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "cc/document_transition/document_transition_request.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/paint/display_item_list.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

// cc property trees make use of a sequence number to identify when tree
// topology changes. For now we naively increment the sequence number each time
// we update the property trees. We should explore optimizing our management of
// the sequence number through the use of a dirty bit or similar. See
// http://crbug.com/692842#c4.
static int g_s_property_tree_sequence_number = 1;

PaintArtifactCompositor::PaintArtifactCompositor(
    base::WeakPtr<CompositorScrollCallbacks> scroll_callbacks)
    : scroll_callbacks_(std::move(scroll_callbacks)),
      tracks_raster_invalidations_(VLOG_IS_ON(3)) {
  root_layer_ = cc::Layer::Create();
}

PaintArtifactCompositor::~PaintArtifactCompositor() {}

void PaintArtifactCompositor::SetTracksRasterInvalidations(bool should_track) {
  tracks_raster_invalidations_ = should_track || VLOG_IS_ON(3);
  for (auto& client : content_layer_clients_)
    client->GetRasterInvalidator().SetTracksRasterInvalidations(should_track);
}

void PaintArtifactCompositor::WillBeRemovedFromFrame() {
  root_layer_->RemoveAllChildren();
}

void PaintArtifactCompositor::SetPrefersLCDText(bool prefers) {
  if (prefers_lcd_text_ == prefers)
    return;
  SetNeedsUpdate();
  prefers_lcd_text_ = prefers;
}

std::unique_ptr<JSONArray> PaintArtifactCompositor::GetPendingLayersAsJSON()
    const {
  std::unique_ptr<JSONArray> result = std::make_unique<JSONArray>();
  for (const PendingLayer& pending_layer : pending_layers_)
    result->PushObject(pending_layer.ToJSON());
  return result;
}

// Get a JSON representation of what layers exist for this PAC.
std::unique_ptr<JSONObject> PaintArtifactCompositor::GetLayersAsJSON(
    LayerTreeFlags flags) const {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      !tracks_raster_invalidations_) {
    flags &= ~(kLayerTreeIncludesInvalidations |
               kLayerTreeIncludesDetailedInvalidations);
  }

  LayersAsJSON layers_as_json(flags);
  for (const auto& layer : root_layer_->children()) {
    const LayerAsJSONClient* json_client = nullptr;
    const TransformPaintPropertyNode* transform = nullptr;
    for (const auto& client : content_layer_clients_) {
      if (&client->Layer() == layer.get()) {
        json_client = client.get();
        transform = &client->State().Transform();
        break;
      }
    }
    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() && !json_client) {
      // The cc::Layer may come from a GraphicsLayer.
      for (const auto& pending_layer : pending_layers_) {
        if (pending_layer.GetCompositingType() ==
            PendingLayer::kPreCompositedLayer) {
          if (&pending_layer.GetGraphicsLayer()->CcLayer() == layer.get()) {
            json_client = pending_layer.GetGraphicsLayer();
            break;
          }
        }
      }
    }
    if (!transform) {
      for (const auto& pending_layer : pending_layers_) {
        if (pending_layer.GetPropertyTreeState().Transform().CcNodeId(
                layer->property_tree_sequence_number()) ==
            layer->transform_tree_index()) {
          transform = &pending_layer.GetPropertyTreeState().Transform();
          break;
        }
      }
    }
    DCHECK(transform);
    layers_as_json.AddLayer(*layer, *transform, json_client);
  }
  return layers_as_json.Finalize();
}

scoped_refptr<cc::Layer> PaintArtifactCompositor::WrappedCcLayerForPendingLayer(
    const PendingLayer& pending_layer) {
  if (pending_layer.GetCompositingType() != PendingLayer::kForeignLayer &&
      pending_layer.GetCompositingType() != PendingLayer::kPreCompositedLayer)
    return nullptr;

  cc::Layer* layer = nullptr;
  gfx::Vector2dF layer_offset;
  if (pending_layer.GetCompositingType() == PendingLayer::kPreCompositedLayer) {
    DCHECK(pending_layer.GetGraphicsLayer());
    DCHECK(!pending_layer.GetGraphicsLayer()->ShouldCreateLayersAfterPaint());
    layer = &pending_layer.GetGraphicsLayer()->CcLayer();
    layer_offset = gfx::Vector2dF(
        pending_layer.GetGraphicsLayer()->GetOffsetFromTransformNode());
  } else {
    DCHECK_EQ(pending_layer.GetCompositingType(), PendingLayer::kForeignLayer);
    // UpdateTouchActionRects() depends on the layer's offset, but when the
    // layer's offset changes, we do not call SetNeedsUpdate() (this is an
    // optimization because the update would only cause an extra commit)
    // This is only OK if the ForeignLayer doesn't have hit test data.
    DCHECK(!pending_layer.FirstPaintChunk().hit_test_data);
    const auto& foreign_layer_display_item =
        To<ForeignLayerDisplayItem>(pending_layer.FirstDisplayItem());
    layer = foreign_layer_display_item.GetLayer();
    layer_offset = gfx::Vector2dF(
        foreign_layer_display_item.VisualRect().OffsetFromOrigin());
  }
  layer->SetOffsetToTransformParent(
      layer_offset + pending_layer.OffsetOfDecompositedTransforms());
  return layer;
}

const TransformPaintPropertyNode&
PaintArtifactCompositor::NearestScrollTranslationForLayer(
    const PendingLayer& pending_layer) {
  if (pending_layer.GetCompositingType() == PendingLayer::kScrollHitTestLayer)
    return pending_layer.ScrollTranslationForScrollHitTestLayer();

  const auto& transform = pending_layer.GetPropertyTreeState().Transform();
  // TODO(pdr): This could be a performance issue because it crawls up the
  // transform tree for each pending layer. If this is on profiles, we should
  // cache a lookup of transform node to scroll translation transform node.
  return transform.NearestScrollTranslationNode();
}

scoped_refptr<cc::Layer>
PaintArtifactCompositor::ScrollHitTestLayerForPendingLayer(
    const PendingLayer& pending_layer) {
  if (pending_layer.GetCompositingType() != PendingLayer::kScrollHitTestLayer)
    return nullptr;

  // We shouldn't decomposite scroll transform nodes.
  DCHECK_EQ(gfx::Vector2dF(), pending_layer.OffsetOfDecompositedTransforms());

  const auto& scroll_node =
      *pending_layer.ScrollTranslationForScrollHitTestLayer().ScrollNode();

  scoped_refptr<cc::Layer> scroll_layer;
  auto scroll_element_id = scroll_node.GetCompositorElementId();
  for (auto& existing_layer : scroll_hit_test_layers_) {
    if (existing_layer && existing_layer->element_id() == scroll_element_id) {
      scroll_layer = std::move(existing_layer);
      break;
    }
  }

  if (scroll_layer) {
    DCHECK_EQ(scroll_layer->element_id(), scroll_node.GetCompositorElementId());
  } else {
    scroll_layer = cc::Layer::Create();
    scroll_layer->SetElementId(scroll_node.GetCompositorElementId());
    scroll_layer->SetHitTestable(true);
  }

  scroll_layer->SetOffsetToTransformParent(
      gfx::Vector2dF(scroll_node.ContainerRect().OffsetFromOrigin()));
  // TODO(pdr): The scroll layer's bounds are currently set to the clipped
  // container bounds but this does not include the border. We may want to
  // change this behavior to make non-composited and composited hit testing
  // match (see: crbug.com/753124). To do this, use
  // |scroll_hit_test->scroll_container_bounds|. Set the layer's bounds equal
  // to the container because the scroll layer does not scroll.
  scroll_layer->SetBounds(scroll_node.ContainerRect().size());

  if (scroll_node.NodeChanged() != PaintPropertyChangeType::kUnchanged) {
    scroll_layer->SetNeedsPushProperties();
    scroll_layer->SetNeedsCommit();
  }

  return scroll_layer;
}

scoped_refptr<cc::ScrollbarLayerBase>
PaintArtifactCompositor::ScrollbarLayerForPendingLayer(
    const PendingLayer& pending_layer) {
  if (pending_layer.GetCompositingType() != PendingLayer::kScrollbarLayer)
    return nullptr;

  const auto& item = pending_layer.FirstDisplayItem();
  DCHECK(item.IsScrollbar());

  const auto& scrollbar_item = To<ScrollbarDisplayItem>(item);
  scoped_refptr<cc::ScrollbarLayerBase> scrollbar_layer;
  for (auto& layer : scrollbar_layers_) {
    if (layer && layer->element_id() == scrollbar_item.ElementId()) {
      scrollbar_layer = std::move(layer);
      break;
    }
  }

  scrollbar_layer = scrollbar_item.CreateOrReuseLayer(scrollbar_layer.get());
  scrollbar_layer->SetOffsetToTransformParent(
      scrollbar_layer->offset_to_transform_parent() +
      gfx::Vector2dF(pending_layer.OffsetOfDecompositedTransforms()));
  return scrollbar_layer;
}

std::unique_ptr<ContentLayerClientImpl>
PaintArtifactCompositor::ClientForPaintChunk(const PaintChunk& paint_chunk) {
  // TODO(chrishtr): for now, just using a linear walk. In the future we can
  // optimize this by using the same techniques used in PaintController for
  // display lists.
  for (auto& client : content_layer_clients_) {
    if (client && client->Matches(paint_chunk))
      return std::move(client);
  }

  auto client = std::make_unique<ContentLayerClientImpl>();
  client->GetRasterInvalidator().SetTracksRasterInvalidations(
      tracks_raster_invalidations_);
  return client;
}

scoped_refptr<cc::Layer>
PaintArtifactCompositor::CompositedLayerForPendingLayer(
    const PendingLayer& pending_layer,
    Vector<std::unique_ptr<ContentLayerClientImpl>>& new_content_layer_clients,
    Vector<scoped_refptr<cc::Layer>>& new_scroll_hit_test_layers,
    Vector<scoped_refptr<cc::ScrollbarLayerBase>>& new_scrollbar_layers) {
  // If the paint chunk is a foreign layer or pre-composited layer, just return
  // its cc::Layer.
  if (auto cc_layer = WrappedCcLayerForPendingLayer(pending_layer))
    return cc_layer;

  // If the paint chunk is a scroll hit test layer, lookup/create the layer.
  if (auto scroll_layer = ScrollHitTestLayerForPendingLayer(pending_layer)) {
    new_scroll_hit_test_layers.push_back(scroll_layer);
    return scroll_layer;
  }

  if (auto scrollbar_layer = ScrollbarLayerForPendingLayer(pending_layer)) {
    new_scrollbar_layers.push_back(scrollbar_layer);
    return scrollbar_layer;
  }

  // The common case: create or reuse a PictureLayer for painted content.
  std::unique_ptr<ContentLayerClientImpl> content_layer_client =
      ClientForPaintChunk(pending_layer.FirstPaintChunk());

  gfx::Vector2dF layer_offset = pending_layer.LayerOffset();
  gfx::Size layer_bounds = pending_layer.LayerBounds();
  auto cc_layer = content_layer_client->UpdateCcPictureLayer(
      pending_layer.Chunks(), layer_offset, layer_bounds,
      pending_layer.GetPropertyTreeState());

  new_content_layer_clients.push_back(std::move(content_layer_client));

  // Set properties that foreign layers would normally control for themselves
  // here to avoid changing foreign layers. This includes things set by
  // GraphicsLayer on the ContentsLayer() or by video clients etc.
  bool contents_opaque = pending_layer.RectKnownToBeOpaque().Contains(
      gfx::RectF(gfx::PointAtOffsetFromOrigin(layer_offset),
                 gfx::SizeF(layer_bounds)));
  cc_layer->SetContentsOpaque(contents_opaque);
  if (!contents_opaque) {
    cc_layer->SetContentsOpaqueForText(
        pending_layer.TextKnownToBeOnOpaqueBackground());
  }

  return cc_layer;
}

namespace {

cc::Layer* ForeignLayer(const PaintChunk& chunk,
                        const PaintArtifact& artifact) {
  if (chunk.size() != 1)
    return nullptr;
  const auto& first_display_item =
      artifact.GetDisplayItemList()[chunk.begin_index];
  auto* foreign_layer = DynamicTo<ForeignLayerDisplayItem>(first_display_item);
  return foreign_layer ? foreign_layer->GetLayer() : nullptr;
}

// True if the paint chunk change affects the result of |Update|, such as the
// compositing decisions in |CollectPendingLayers|. This will return false for
// repaint updates that can be handled by |UpdateRepaintedLayers|, such as
// background color changes.
bool NeedsFullUpdateAfterPaintingChunk(
    const PaintChunk& previous,
    const PaintArtifact& previous_artifact,
    const PaintChunk& repainted,
    const PaintArtifact& repainted_artifact) {
  if (!repainted.Matches(previous))
    return true;

  if (repainted.is_moved_from_cached_subsequence) {
    DCHECK_EQ(previous.bounds, repainted.bounds);
    DCHECK_EQ(previous.rect_known_to_be_opaque,
              repainted.rect_known_to_be_opaque);
    DCHECK_EQ(previous.text_known_to_be_on_opaque_background,
              repainted.text_known_to_be_on_opaque_background);
    DCHECK_EQ(previous.has_text, repainted.has_text);

    // Debugging for https://crbug.com/1237389 and https://crbug.com/1230104.
    // Before returning that a full update is not needed, check that the
    // properties are changed, which would indicate a missing call to
    // SetNeedsUpdate.
    if (previous.properties != repainted.properties) {
      NOTREACHED();
      return true;
    }

    // Not checking ForeignLayer() here because the old ForeignDisplayItem
    // was set to 0 when we moved the cached subsequence. This is also the
    // reason why we check is_moved_from_cached_subsequence before checking
    // ForeignLayer().
    return false;
  }

  // Bounds are used in overlap testing.
  // TODO(pdr): If the bounds shrink, that does affect overlap testing but we
  // could return false to continue using less-than-optimal overlap testing in
  // order to save a full compositing update.
  if (previous.bounds != repainted.bounds)
    return true;

  // Changing foreign layers requires a full update to push the new cc::Layers.
  if (ForeignLayer(previous, previous_artifact) !=
      ForeignLayer(repainted, repainted_artifact)) {
    return true;
  }

  // Opaqueness of individual chunks is used to set the cc::Layer's contents
  // opaque property.
  if (previous.rect_known_to_be_opaque != repainted.rect_known_to_be_opaque)
    return true;
  // Similar to opaqueness, opaqueness for text is used to set the cc::Layer's
  // contents opaque for text property.
  if (previous.text_known_to_be_on_opaque_background !=
      repainted.text_known_to_be_on_opaque_background) {
    return true;
  }
  // |has_text| affects compositing decisions (see:
  // |PendingLayer::MergeInternal|).
  if (previous.has_text != repainted.has_text)
    return true;

  // Debugging for https://crbug.com/1237389 and https://crbug.com/1230104.
  // Before returning that a full update is not needed, check that the
  // properties are changed, which would indicate a missing call to
  // SetNeedsUpdate.
  if (previous.properties != repainted.properties) {
    NOTREACHED();
    return true;
  }

  return false;
}

}  // namespace

void PaintArtifactCompositor::SetNeedsFullUpdateAfterPaintIfNeeded(
    const PaintChunkSubset& previous,
    const PaintChunkSubset& repainted) {
  if (needs_update_)
    return;

  // Adding or removing chunks requires a full update to add/remove cc::layers.
  if (previous.size() != repainted.size()) {
    SetNeedsUpdate();
    return;
  }

  // Loop over both paint chunk subsets in order.
  auto previous_chunk_it = previous.begin();
  auto repainted_chunk_it = repainted.begin();
  for (; previous_chunk_it != previous.end();
       ++previous_chunk_it, ++repainted_chunk_it) {
    const auto& previous_chunk = *previous_chunk_it;
    const auto& repainted_chunk = *repainted_chunk_it;
    if (NeedsFullUpdateAfterPaintingChunk(
            previous_chunk, previous.GetPaintArtifact(), repainted_chunk,
            repainted.GetPaintArtifact())) {
      SetNeedsUpdate();
      return;
    }
  }
}

bool PaintArtifactCompositor::HasComposited(
    CompositorElementId element_id) const {
  // |Update| creates PropertyTrees on the LayerTreeHost to represent the
  // composited page state. Check if it has created a property tree node for
  // the given |element_id|.
  DCHECK(!NeedsUpdate()) << "This should only be called after an update";
  return root_layer_->layer_tree_host()->property_trees()->HasElement(
      element_id);
}

// Returns nullptr if 'ancestor' is not a strict ancestor of 'node'.
// Otherwise, return the child of 'ancestor' that is an ancestor of 'node' or
// 'node' itself.
static const EffectPaintPropertyNode* StrictUnaliasedChildOfAlongPath(
    const EffectPaintPropertyNode& ancestor,
    const EffectPaintPropertyNode& node) {
  const auto* n = &node;
  while (n) {
    const auto* parent = n->UnaliasedParent();
    if (parent == &ancestor)
      return n;
    n = parent;
  }
  return nullptr;
}

bool PaintArtifactCompositor::DecompositeEffect(
    const EffectPaintPropertyNode& parent_effect,
    wtf_size_t first_layer_in_parent_group_index,
    const EffectPaintPropertyNode& effect,
    wtf_size_t layer_index) {
  // The layer must be the last layer in pending_layers_.
  DCHECK_EQ(layer_index, pending_layers_.size() - 1);

  // If the effect associated with the layer is deeper than than the effect
  // we are attempting to decomposite, than implies some previous decision
  // did not allow to decomposite intermediate effects.
  PendingLayer& layer = pending_layers_[layer_index];
  if (&layer.GetPropertyTreeState().Effect() != &effect)
    return false;
  if (layer.RequiresOwnLayer())
    return false;
  if (effect.HasDirectCompositingReasons())
    return false;

  PropertyTreeState group_state(effect.LocalTransformSpace().Unalias(),
                                effect.OutputClip()
                                    ? effect.OutputClip()->Unalias()
                                    : layer.GetPropertyTreeState().Clip(),
                                effect);
  absl::optional<PropertyTreeState> upcast_state =
      group_state.CanUpcastWith(layer.GetPropertyTreeState());
  if (!upcast_state)
    return false;

  upcast_state->SetEffect(parent_effect);

  // Exotic blending layer can be decomposited only if its parent group
  // (which defines the scope of the blending) has zero or one layer before it,
  // and it can be merged into that layer. However, a layer not drawing content
  // at the beginning of the parent group doesn't count, as the blending mode
  // doesn't apply to it.
  if (effect.BlendMode() != SkBlendMode::kSrcOver) {
    auto num_previous_siblings =
        layer_index - first_layer_in_parent_group_index;
    if (num_previous_siblings) {
      if (num_previous_siblings > 2)
        return false;
      if (num_previous_siblings == 2 &&
          pending_layers_[first_layer_in_parent_group_index].MayDrawContent())
        return false;
      const auto& previous_sibling = pending_layers_[layer_index - 1];
      if (previous_sibling.MayDrawContent() &&
          !previous_sibling.CanMerge(layer, *upcast_state, prefers_lcd_text_))
        return false;
    }
  }

  layer.Upcast(*upcast_state);
  return true;
}

void PaintArtifactCompositor::LayerizeGroup(
    const PaintChunkSubset& chunks,
    const EffectPaintPropertyNode& current_group,
    PaintChunkIterator& chunk_cursor) {
  wtf_size_t first_layer_in_current_group = pending_layers_.size();
  // The worst case time complexity of the algorithm is O(pqd), where
  // p = the number of paint chunks.
  // q = average number of trials to find a squash layer or rejected
  //     for overlapping.
  // d = (sum of) the depth of property trees.
  // The analysis as follows:
  // Every paint chunk will be visited by the main loop below for exactly
  // once, except for chunks that enter or exit groups (case B & C below). For
  // normal chunk visit (case A), the only cost is determining squash, which
  // costs O(qd), where d came from |CanUpcastWith| and geometry mapping.
  // Subtotal: O(pqd)
  // For group entering and exiting, it could cost O(d) for each group, for
  // searching the shallowest subgroup (StrictChildOfAlongPath), thus O(d^2)
  // in total.
  // Also when exiting group, the group may be decomposited and squashed to a
  // previous layer. Again finding the host costs O(qd). Merging would cost
  // O(p) due to copying the chunk list. Subtotal: O((qd + p)d) = O(qd^2 + pd)
  // Assuming p > d, the total complexity would be O(pqd + qd^2 + pd) = O(pqd)
  while (chunk_cursor != chunks.end()) {
    // Look at the effect node of the next chunk. There are 3 possible cases:
    // A. The next chunk belongs to the current group but no subgroup.
    // B. The next chunk does not belong to the current group.
    // C. The next chunk belongs to some subgroup of the current group.
    const auto& chunk_effect = chunk_cursor->properties.Effect().Unalias();
    if (&chunk_effect == &current_group) {
      pending_layers_.emplace_back(chunks, chunk_cursor);
      ++chunk_cursor;
      if (pending_layers_.back().RequiresOwnLayer())
        continue;
    } else {
      const EffectPaintPropertyNode* subgroup =
          StrictUnaliasedChildOfAlongPath(current_group, chunk_effect);
      // Case B: This means we need to close the current group without
      //         processing the next chunk.
      if (!subgroup)
        break;
      // Case C: The following chunks belong to a subgroup. Process them by
      //         a recursion call.
      wtf_size_t first_layer_in_subgroup = pending_layers_.size();
      LayerizeGroup(chunks, *subgroup, chunk_cursor);
      // The above LayerizeGroup generated new layers in pending_layers_
      // [first_layer_in_subgroup .. pending_layers.size() - 1]. If it
      // generated 2 or more layer that we already know can't be merged
      // together, we should not decomposite and try to merge any of them into
      // the previous layers.
      if (first_layer_in_subgroup != pending_layers_.size() - 1)
        continue;
      if (!DecompositeEffect(current_group, first_layer_in_current_group,
                             *subgroup, first_layer_in_subgroup))
        continue;
    }
    // At this point pending_layers_.back() is the either a layer from a
    // "decomposited" subgroup or a layer created from a chunk we just
    // processed. Now determine whether it could be merged into a previous
    // layer.
    PendingLayer& new_layer = pending_layers_.back();
    DCHECK(!new_layer.RequiresOwnLayer());
    DCHECK_EQ(&current_group, &new_layer.GetPropertyTreeState().Effect());
    // This iterates pending_layers_[first_layer_in_current_group:-1] in
    // reverse.
    for (wtf_size_t candidate_index = pending_layers_.size() - 1;
         candidate_index-- > first_layer_in_current_group;) {
      PendingLayer& candidate_layer = pending_layers_[candidate_index];
      if (candidate_layer.Merge(new_layer, prefers_lcd_text_)) {
        pending_layers_.pop_back();
        break;
      }
      if (new_layer.MightOverlap(candidate_layer)) {
        new_layer.SetCompositingType(PendingLayer::kOverlap);
        break;
      }
    }
  }
}

void PaintArtifactCompositor::CollectPendingLayers(
    const HeapVector<PreCompositedLayerInfo>& pre_composited_layers) {
  // Shrink, but do not release the backing. Re-use it from the last frame.
  pending_layers_.Shrink(0);
  for (auto& layer : pre_composited_layers) {
    if (layer.graphics_layer &&
        !layer.graphics_layer->ShouldCreateLayersAfterPaint()) {
      pending_layers_.emplace_back(layer);
      continue;
    }
    auto cursor = layer.chunks.begin();
    LayerizeGroup(layer.chunks, EffectPaintPropertyNode::Root(), cursor);
    DCHECK(cursor == layer.chunks.end());
  }
  pending_layers_.ShrinkToReasonableCapacity();
}

void SynthesizedClip::UpdateLayer(bool needs_layer,
                                  const ClipPaintPropertyNode& clip,
                                  const TransformPaintPropertyNode& transform) {
  if (!needs_layer) {
    layer_.reset();
    return;
  }
  if (!layer_) {
    layer_ = cc::PictureLayer::Create(this);
    layer_->SetIsDrawable(true);
    // The clip layer must be hit testable because the compositor may not know
    // whether the hit test is clipped out.
    // See: cc::LayerTreeHostImpl::IsInitialScrollHitTestReliable().
    layer_->SetHitTestable(true);
  }

  const RefCountedPath* path = clip.ClipPath();
  SkRRect new_rrect(clip.PaintClipRect());
  gfx::Rect layer_bounds = gfx::ToEnclosingRect(clip.PaintClipRect().Rect());
  bool needs_display = false;

  auto new_translation_2d_or_matrix =
      GeometryMapper::SourceToDestinationProjection(clip.LocalTransformSpace(),
                                                    transform);
  new_translation_2d_or_matrix.MapRect(layer_bounds);
  new_translation_2d_or_matrix.PostTranslate(-layer_bounds.x(),
                                             -layer_bounds.y());

  if (!path && new_translation_2d_or_matrix.IsIdentityOr2DTranslation()) {
    const auto& translation = new_translation_2d_or_matrix.Translation2D();
    new_rrect.offset(translation.x(), translation.y());
    needs_display = !rrect_is_local_ || new_rrect != rrect_;
    translation_2d_or_matrix_ = GeometryMapper::Translation2DOrMatrix();
    rrect_is_local_ = true;
  } else {
    needs_display = rrect_is_local_ || new_rrect != rrect_ ||
                    new_translation_2d_or_matrix != translation_2d_or_matrix_ ||
                    (path_ != path && (!path_ || !path || *path_ != *path));
    translation_2d_or_matrix_ = new_translation_2d_or_matrix;
    rrect_is_local_ = false;
  }

  if (needs_display)
    layer_->SetNeedsDisplay();

  layer_->SetOffsetToTransformParent(
      gfx::Vector2dF(layer_bounds.OffsetFromOrigin()));
  layer_->SetBounds(layer_bounds.size());
  rrect_ = new_rrect;
  path_ = path;
}

scoped_refptr<cc::DisplayItemList>
SynthesizedClip::PaintContentsToDisplayList() {
  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>(
      cc::DisplayItemList::kTopLevelDisplayItemList);
  PaintFlags flags;
  flags.setAntiAlias(true);
  cc_list->StartPaint();
  if (rrect_is_local_) {
    cc_list->push<cc::DrawRRectOp>(rrect_, flags);
  } else {
    cc_list->push<cc::SaveOp>();
    if (translation_2d_or_matrix_.IsIdentityOr2DTranslation()) {
      const auto& translation = translation_2d_or_matrix_.Translation2D();
      cc_list->push<cc::TranslateOp>(translation.x(), translation.y());
    } else {
      cc_list->push<cc::ConcatOp>(
          TransformationMatrix::ToSkM44(translation_2d_or_matrix_.Matrix()));
    }
    if (path_) {
      cc_list->push<cc::ClipPathOp>(path_->GetSkPath(), SkClipOp::kIntersect,
                                    true);
    }
    cc_list->push<cc::DrawRRectOp>(rrect_, flags);
    cc_list->push<cc::RestoreOp>();
  }
  cc_list->EndPaintOfUnpaired(gfx::Rect(layer_->bounds()));
  cc_list->Finalize();
  return cc_list;
}

SynthesizedClip& PaintArtifactCompositor::CreateOrReuseSynthesizedClipLayer(
    const ClipPaintPropertyNode& clip,
    const TransformPaintPropertyNode& transform,
    bool needs_layer,
    CompositorElementId& mask_isolation_id,
    CompositorElementId& mask_effect_id) {
  auto* entry =
      std::find_if(synthesized_clip_cache_.begin(),
                   synthesized_clip_cache_.end(), [&clip](const auto& entry) {
                     return entry.key == &clip && !entry.in_use;
                   });
  if (entry == synthesized_clip_cache_.end()) {
    synthesized_clip_cache_.push_back(SynthesizedClipEntry{
        &clip, std::make_unique<SynthesizedClip>(), false});
    entry = synthesized_clip_cache_.end() - 1;
  }

  entry->in_use = true;
  SynthesizedClip& synthesized_clip = *entry->synthesized_clip;
  if (needs_layer) {
    synthesized_clip.UpdateLayer(needs_layer, clip, transform);
    synthesized_clip.Layer()->SetLayerTreeHost(root_layer_->layer_tree_host());
    if (layer_debug_info_enabled_ && !synthesized_clip.Layer()->debug_info())
      synthesized_clip.Layer()->SetDebugName("Synthesized Clip");
  }
  mask_isolation_id = synthesized_clip.GetMaskIsolationId();
  mask_effect_id = synthesized_clip.GetMaskEffectId();
  return synthesized_clip;
}

static void UpdateCompositorViewportProperties(
    const PaintArtifactCompositor::ViewportProperties& properties,
    PropertyTreeManager& property_tree_manager,
    cc::LayerTreeHost* layer_tree_host) {
  // The inner and outer viewports' existence is linked. That is, either they're
  // both null or they both exist.
  DCHECK_EQ(static_cast<bool>(properties.outer_scroll_translation),
            static_cast<bool>(properties.inner_scroll_translation));
  DCHECK(!properties.outer_clip ||
         static_cast<bool>(properties.inner_scroll_translation));

  cc::ViewportPropertyIds ids;
  if (properties.overscroll_elasticity_transform) {
    ids.overscroll_elasticity_transform =
        property_tree_manager.EnsureCompositorTransformNode(
            *properties.overscroll_elasticity_transform);
  }
  if (properties.overscroll_elasticity_effect) {
    ids.overscroll_elasticity_effect =
        properties.overscroll_elasticity_effect->GetCompositorElementId();
  }
  if (properties.page_scale) {
    ids.page_scale_transform =
        property_tree_manager.EnsureCompositorPageScaleTransformNode(
            *properties.page_scale);
  }
  if (properties.inner_scroll_translation) {
    ids.inner_scroll = property_tree_manager.EnsureCompositorInnerScrollNode(
        *properties.inner_scroll_translation);
    if (properties.outer_clip) {
      ids.outer_clip = property_tree_manager.EnsureCompositorClipNode(
          *properties.outer_clip);
    }
    if (properties.outer_scroll_translation) {
      ids.outer_scroll = property_tree_manager.EnsureCompositorOuterScrollNode(
          *properties.outer_scroll_translation);
    }
  }

  layer_tree_host->RegisterViewportPropertyIds(ids);
}

void PaintArtifactCompositor::Update(
    const HeapVector<PreCompositedLayerInfo>& pre_composited_layers,
    const ViewportProperties& viewport_properties,
    const Vector<const TransformPaintPropertyNode*>& scroll_translation_nodes,
    Vector<std::unique_ptr<cc::DocumentTransitionRequest>>
        transition_requests) {
  // See: |UpdateRepaintedLayers| for repaint updates.
  DCHECK(needs_update_);
  DCHECK(scroll_translation_nodes.IsEmpty() ||
         RuntimeEnabledFeatures::ScrollUnificationEnabled());
  DCHECK(root_layer_);

  TRACE_EVENT0("blink", "PaintArtifactCompositor::Update");

  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  cc::LayerTreeHost* host = root_layer_->layer_tree_host();
  if (!host)
    return;

  for (auto& request : transition_requests)
    host->AddDocumentTransitionRequest(std::move(request));

  host->property_trees()->scroll_tree.SetScrollCallbacks(scroll_callbacks_);
  root_layer_->set_property_tree_sequence_number(
      g_s_property_tree_sequence_number);

  // Make compositing decisions, storing the result in |pending_layers_|.
  CollectPendingLayers(pre_composited_layers);
  PendingLayer::DecompositeTransforms(pending_layers_);

  LayerListBuilder layer_list_builder;
  PropertyTreeManager property_tree_manager(*this, *host->property_trees(),
                                            *root_layer_, layer_list_builder,
                                            g_s_property_tree_sequence_number);

  UpdateCompositorViewportProperties(viewport_properties, property_tree_manager,
                                     host);

  // With ScrollUnification, we ensure a cc::ScrollNode for all
  // |scroll_translation_nodes|.
  if (RuntimeEnabledFeatures::ScrollUnificationEnabled())
    property_tree_manager.EnsureCompositorScrollNodes(scroll_translation_nodes);

  Vector<std::unique_ptr<ContentLayerClientImpl>> new_content_layer_clients;
  new_content_layer_clients.ReserveCapacity(pending_layers_.size());
  Vector<scoped_refptr<cc::Layer>> new_scroll_hit_test_layers;
  Vector<scoped_refptr<cc::ScrollbarLayerBase>> new_scrollbar_layers;

  // Maps from cc effect id to blink effects. Containing only the effects
  // having composited layers.
  Vector<const EffectPaintPropertyNode*> blink_effects;

  for (auto& entry : synthesized_clip_cache_)
    entry.in_use = false;

  cc::LayerSelection layer_selection;
  for (const auto& pending_layer : pending_layers_) {
    const auto& property_state = pending_layer.GetPropertyTreeState();
    const auto& transform = property_state.Transform();
    const auto& clip = property_state.Clip();

    scoped_refptr<cc::Layer> layer = CompositedLayerForPendingLayer(
        pending_layer, new_content_layer_clients, new_scroll_hit_test_layers,
        new_scrollbar_layers);

    UpdateLayerProperties(*layer, pending_layer);
    UpdateLayerSelection(*layer, pending_layer, layer_selection);

    layer->SetLayerTreeHost(root_layer_->layer_tree_host());

    int transform_id =
        property_tree_manager.EnsureCompositorTransformNode(transform);
    int clip_id = property_tree_manager.EnsureCompositorClipNode(clip);

    int effect_id = property_tree_manager.SwitchToEffectNodeWithSynthesizedClip(
        property_state.Effect(), clip, layer->DrawsContent());
    if (blink_effects.size() <= static_cast<wtf_size_t>(effect_id))
      blink_effects.resize(effect_id + 1);
    if (!blink_effects[effect_id]) {
      blink_effects[effect_id] = &property_state.Effect();
      // We need additional bookkeeping for backdrop-filter mask.
      if (property_state.Effect().RequiresCompositingForBackdropFilterMask()) {
        static_cast<cc::PictureLayer*>(layer.get())
            ->SetIsBackdropFilterMask(true);
        layer->SetElementId(property_state.Effect().GetCompositorElementId());
        auto& effect_tree = host->property_trees()->effect_tree;
        auto* cc_node = effect_tree.Node(effect_id);
        effect_tree.Node(cc_node->parent_id)->backdrop_mask_element_id =
            property_state.Effect().GetCompositorElementId();
      }
    } else {
      DCHECK_EQ(blink_effects[effect_id], &property_state.Effect());
    }

    // The compositor scroll node is not directly stored in the property tree
    // state but can be created via the scroll offset translation node.
    const auto& scroll_translation =
        NearestScrollTranslationForLayer(pending_layer);
    // TODO(ScrollUnification): We may combine the following two calls to
    // property_tree_manager.SetCcScrollNodeIsComposited(scroll_translation);
    int scroll_id =
        property_tree_manager.EnsureCompositorScrollNode(scroll_translation);
    if (RuntimeEnabledFeatures::ScrollUnificationEnabled())
      property_tree_manager.SetCcScrollNodeIsComposited(scroll_id);

    layer_list_builder.Add(layer);

    layer->set_property_tree_sequence_number(
        root_layer_->property_tree_sequence_number());
    layer->SetTransformTreeIndex(transform_id);
    layer->SetScrollTreeIndex(scroll_id);
    layer->SetClipTreeIndex(clip_id);
    layer->SetEffectTreeIndex(effect_id);
    bool backface_hidden = property_state.Transform().IsBackfaceHidden();
    layer->SetShouldCheckBackfaceVisibility(backface_hidden);

    // If the property tree state has changed between the layer and the root,
    // we need to inform the compositor so damage can be calculated. Calling
    // |PropertyTreeStateChanged| for every pending layer is O(|property
    // nodes|^2) and could be optimized by caching the lookup of nodes known
    // to be changed/unchanged.
    if (layer->subtree_property_changed() ||
        pending_layer.PropertyTreeStateChanged()) {
      layer->SetSubtreePropertyChanged();
      root_layer_->SetNeedsCommit();
    }
  }

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    root_layer_->layer_tree_host()->RegisterSelection(layer_selection);

  property_tree_manager.Finalize();
  content_layer_clients_.swap(new_content_layer_clients);
  scroll_hit_test_layers_.swap(new_scroll_hit_test_layers);
  scrollbar_layers_.swap(new_scrollbar_layers);

  auto* new_end = std::remove_if(
      synthesized_clip_cache_.begin(), synthesized_clip_cache_.end(),
      [](const auto& entry) { return !entry.in_use; });
  synthesized_clip_cache_.Shrink(
      static_cast<wtf_size_t>(new_end - synthesized_clip_cache_.begin()));

  // This should be done before UpdateRenderSurfaceForEffects() for which to
  // get property tree node ids from the layers.
  host->property_trees()->sequence_number = g_s_property_tree_sequence_number;

  auto layers = layer_list_builder.Finalize();
  UpdateRenderSurfaceForEffects(host->property_trees()->effect_tree, layers,
                                blink_effects);
  root_layer_->SetChildLayerList(std::move(layers));

  // Update the host's active registered elements from the new property tree.
  host->UpdateActiveElements();

  // Mark the property trees as having been rebuilt.
  host->property_trees()->needs_rebuild = false;
  host->property_trees()->ResetCachedData();
  previous_update_for_testing_ = PreviousUpdateType::kFull;
  needs_update_ = false;

  UpdateDebugInfo();

  g_s_property_tree_sequence_number++;

  DVLOG(2) << "PaintArtifactCompositor::Update() done\n"
           << "Composited layers:\n"
           << GetLayersAsJSON(VLOG_IS_ON(3) ? 0xffffffff : 0)
                  ->ToPrettyJSONString()
                  .Utf8();
}

void PaintArtifactCompositor::UpdateLayerProperties(
    cc::Layer& layer,
    const PendingLayer& pending_layer) {
  // Properties of foreign layers are managed by their owners.
  if (pending_layer.GetCompositingType() == PendingLayer::kForeignLayer)
    return;

  if (pending_layer.GetGraphicsLayer() &&
      pending_layer.GetGraphicsLayer()->PaintsContentOrHitTest()) {
    PaintChunkSubset chunks(pending_layer.GetGraphicsLayer()
                                ->GetPaintController()
                                .GetPaintArtifactShared());
    PaintChunksToCcLayer::UpdateLayerProperties(
        layer, pending_layer.GetPropertyTreeState(), chunks);
  } else {
    PaintChunksToCcLayer::UpdateLayerProperties(
        layer, pending_layer.GetPropertyTreeState(), pending_layer.Chunks());
  }
}

void PaintArtifactCompositor::UpdateLayerSelection(
    cc::Layer& layer,
    const PendingLayer& pending_layer,
    cc::LayerSelection& layer_selection) {
  // Foreign layers cannot contain selection.
  if (pending_layer.GetCompositingType() == PendingLayer::kForeignLayer)
    return;

  if (pending_layer.GetGraphicsLayer() &&
      pending_layer.GetGraphicsLayer()->PaintsContentOrHitTest()) {
    PaintChunkSubset chunks(pending_layer.GetGraphicsLayer()
                                ->GetPaintController()
                                .GetPaintArtifactShared());
    PaintChunksToCcLayer::UpdateLayerSelection(
        layer, pending_layer.GetPropertyTreeState(), chunks, layer_selection);
  } else {
    PaintChunksToCcLayer::UpdateLayerSelection(
        layer, pending_layer.GetPropertyTreeState(), pending_layer.Chunks(),
        layer_selection);
  }
}

void PaintArtifactCompositor::UpdateRepaintedContentLayerClient(
    const PendingLayer& pending_layer,
    bool pending_layer_chunks_unchanged,
    ContentLayerClientImpl& content_layer_client) {
  // Checking |pending_layer_chunks_unchanged| is an optimization to avoid the
  // expensive call to |UpdateCcPictureLayer| when no repainting occurs for this
  // PendingLayer.
  if (pending_layer_chunks_unchanged) {
    // See RasterInvalidator::SetOldPaintArtifact() for the reason for this.
    content_layer_client.GetRasterInvalidator().SetOldPaintArtifact(
        &pending_layer.Chunks().GetPaintArtifact());
  } else {
    content_layer_client.UpdateCcPictureLayer(
        pending_layer.Chunks(), pending_layer.LayerOffset(),
        pending_layer.LayerBounds(), pending_layer.GetPropertyTreeState());
  }
}

namespace {

// This class iterates forward over the PaintChunks in a vector of
// |PreCompositedLayerInfo|s.
class PreCompositedLayerPaintChunkFinder {
  STACK_ALLOCATED();

 public:
  explicit PreCompositedLayerPaintChunkFinder(
      HeapVector<PreCompositedLayerInfo>& pre_composited_layers)
      : pre_composited_layers_(pre_composited_layers),
        pre_composited_layer_it_(pre_composited_layers_.begin()),
        subset_iterator_(pre_composited_layer_it_->chunks.begin()) {}

  const PaintChunk& current_chunk() {
    CHECK(pre_composited_layer_it_ != pre_composited_layers_.end());
    CHECK(subset_iterator_ != pre_composited_layer_it_->chunks.end());
    return *subset_iterator_;
  }

  const PaintArtifact& current_artifact() {
    CHECK(pre_composited_layer_it_ != pre_composited_layers_.end());
    return pre_composited_layer_it_->chunks.GetPaintArtifact();
  }

  bool AdvanceToMatching(const PaintChunk& chunk) {
    while (pre_composited_layer_it_ != pre_composited_layers_.end()) {
      while (subset_iterator_ != pre_composited_layer_it_->chunks.end()) {
        if (subset_iterator_->Matches(chunk))
          return true;
        ++subset_iterator_;
      }
      pre_composited_layer_it_++;
      if (pre_composited_layer_it_ == pre_composited_layers_.end())
        break;
      subset_iterator_ = pre_composited_layer_it_->chunks.begin();
    }
    // Unable to find a matching paint chunk.
    return false;
  }

 private:
  HeapVector<PreCompositedLayerInfo>& pre_composited_layers_;
  HeapVector<PreCompositedLayerInfo>::iterator pre_composited_layer_it_;
  PaintChunkSubset::Iterator subset_iterator_;
};

}  // namespace

void PaintArtifactCompositor::UpdateRepaintedLayers(
    HeapVector<PreCompositedLayerInfo>& pre_composited_layers) {
  // |Update| should be used for full updates.
  DCHECK(!needs_update_);

#if DCHECK_IS_ON()
  // Any property tree state change should have caused a full update.
  for (const auto& pre_composited_layer : pre_composited_layers) {
    if (pre_composited_layer.graphics_layer) {
      DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
      continue;
    }
    for (const auto& chunk : pre_composited_layer.chunks) {
      // If this fires, a property tree value has changed but we are missing a
      // call to |PaintArtifactCompositor::SetNeedsUpdate|.
      DCHECK(!chunk.properties.GetPropertyTreeState().Unalias().ChangedToRoot(
          PaintPropertyChangeType::kChangedOnlyNonRerasterValues));
    }
  }
#endif

  cc::LayerSelection layer_selection;

  // The loop below iterates over the existing PendingLayers and issues updates.
  PreCompositedLayerPaintChunkFinder repainted_chunk_finder(
      pre_composited_layers);
  auto* content_layer_client_it = content_layer_clients_.begin();
  auto* scroll_hit_test_layer_it = scroll_hit_test_layers_.begin();
  auto* scrollbar_layer_it = scrollbar_layers_.begin();
  for (auto* pending_layer_it = pending_layers_.begin();
       pending_layer_it != pending_layers_.end(); pending_layer_it++) {
    auto compositing_type = pending_layer_it->GetCompositingType();
    if (compositing_type == PendingLayer::kForeignLayer &&
        !RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // These layers are fully managed externally and do not need an update.
    } else if (compositing_type == PendingLayer::kPreCompositedLayer) {
      // These are Pre-CompositeAfterPaint layers where raster invalidation has
      // already occurred and we just need to update layer properties&selection.
      DCHECK(pending_layer_it->GetGraphicsLayer());
      if (pending_layer_it->GetGraphicsLayer()->Repainted()) {
        UpdateLayerProperties(pending_layer_it->GetGraphicsLayer()->CcLayer(),
                              *pending_layer_it);
        UpdateLayerSelection(pending_layer_it->GetGraphicsLayer()->CcLayer(),
                             *pending_layer_it, layer_selection);
      }
    } else {
      // These are CompositeAfterPaint (or CompositeSVG) layers and we need to
      // both copy the repainted paint chunks and update the cc::Layer. To do
      // this, we need the previous PaintChunks (from the PendingLayer) and the
      // matching repainted PaintChunks (from |pre_composited_layers|). Because
      // repaint-only updates cannot add, remove, or re-order PaintChunks,
      // |repainted_chunk_finder| searches forward in |pre_composited_layers|
      // for the matching paint chunk, ensuring this function is O(chunks).
      const PaintChunk& first = *pending_layer_it->Chunks().begin();
      bool did_advance = repainted_chunk_finder.AdvanceToMatching(first);

      // If we do not find a matching PaintChunk, PaintChunks must have been
      // added, removed, or re-ordered, and we should be doing a full update
      // instead of a repaint update.
      CHECK(did_advance);

      // Essentially replace the paint chunks of the pending layer with the
      // repainted chunks in |repainted_artifact|. The pending layer's paint
      // chunks (a |PaintChunkSubset|) actually store indices to |PaintChunk|s
      // in a |PaintArtifact|. In repaint updates, chunks are not added,
      // removed, or re-ordered, so we can simply swap in a repainted
      // |PaintArtifact| instead of copying |PaintChunk|s individually.
      const PaintArtifact& previous_artifact =
          pending_layer_it->Chunks().GetPaintArtifact();
      const PaintArtifact& repainted_artifact =
          repainted_chunk_finder.current_artifact();
      DCHECK_EQ(previous_artifact.PaintChunks().size(),
                repainted_artifact.PaintChunks().size());
      pending_layer_it->SetPaintArtifact(&repainted_artifact);

      bool pending_layer_chunks_unchanged = true;
      for (const auto& chunk : pending_layer_it->Chunks()) {
        if (!chunk.is_moved_from_cached_subsequence) {
          pending_layer_chunks_unchanged = false;
          break;
        }
      }

      cc::Layer* cc_layer = nullptr;
      switch (pending_layer_it->GetCompositingType()) {
        case PendingLayer::kPreCompositedLayer:
          NOTREACHED();
          break;
        case PendingLayer::kForeignLayer:
          continue;
        case PendingLayer::kScrollbarLayer:
          cc_layer = scrollbar_layer_it->get();
          ++scrollbar_layer_it;
          break;
        case PendingLayer::kScrollHitTestLayer:
          cc_layer = scroll_hit_test_layer_it->get();
          ++scroll_hit_test_layer_it;
          break;
        default:
          UpdateRepaintedContentLayerClient(*pending_layer_it,
                                            pending_layer_chunks_unchanged,
                                            **content_layer_client_it);
          cc_layer = &(*content_layer_client_it)->Layer();
          ++content_layer_client_it;
          break;
      }
      DCHECK(cc_layer);

      if (!pending_layer_chunks_unchanged)
        UpdateLayerProperties(*cc_layer, *pending_layer_it);
      UpdateLayerSelection(*cc_layer, *pending_layer_it, layer_selection);
    }
  }

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    root_layer_->layer_tree_host()->RegisterSelection(layer_selection);

  UpdateDebugInfo();

  previous_update_for_testing_ = PreviousUpdateType::kRepaint;
  needs_update_ = false;
}

bool PaintArtifactCompositor::CanDirectlyUpdateProperties() const {
  // Don't try to retrieve property trees if we need an update. The full
  // update will update all of the nodes, so a direct update doesn't need to
  // do anything.
  if (needs_update_)
    return false;

  return root_layer_ && root_layer_->layer_tree_host();
}

bool PaintArtifactCompositor::DirectlyUpdateCompositedOpacityValue(
    const EffectPaintPropertyNode& effect) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(effect.HasDirectCompositingReasons());
  if (CanDirectlyUpdateProperties()) {
    return PropertyTreeManager::DirectlyUpdateCompositedOpacityValue(
        *root_layer_->layer_tree_host(), effect);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlyUpdateScrollOffsetTransform(
    const TransformPaintPropertyNode& transform) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // We can only directly-update compositor values if all content associated
    // with the node is known to be composited. We cannot DCHECK this pre-
    // CompositeAfterPaint because we cannot query CompositedLayerMapping here.
    DCHECK(transform.HasDirectCompositingReasons());
  }
  if (CanDirectlyUpdateProperties()) {
    return PropertyTreeManager::DirectlyUpdateScrollOffsetTransform(
        *root_layer_->layer_tree_host(), transform);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlyUpdateTransform(
    const TransformPaintPropertyNode& transform) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(transform.HasDirectCompositingReasons());
  // We only assume worst-case overlap testing due to animations (see:
  // |PendingLayer::VisualRectForOverlapTesting|) so we can only use the direct
  // transform update (which skips checking for compositing changes) when
  // animations are present.
  DCHECK(transform.HasActiveTransformAnimation());
  if (CanDirectlyUpdateProperties()) {
    return PropertyTreeManager::DirectlyUpdateTransform(
        *root_layer_->layer_tree_host(), transform);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlyUpdatePageScaleTransform(
    const TransformPaintPropertyNode& transform) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(transform.HasDirectCompositingReasons());
  if (CanDirectlyUpdateProperties()) {
    return PropertyTreeManager::DirectlyUpdatePageScaleTransform(
        *root_layer_->layer_tree_host(), transform);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlySetScrollOffset(
    CompositorElementId element_id,
    const gfx::PointF& scroll_offset) {
  if (!root_layer_ || !root_layer_->layer_tree_host())
    return false;
  auto* property_trees = root_layer_->layer_tree_host()->property_trees();
  if (!property_trees->element_id_to_scroll_node_index.contains(element_id))
    return false;
  PropertyTreeManager::DirectlySetScrollOffset(*root_layer_->layer_tree_host(),
                                               element_id, scroll_offset);
  return true;
}

static cc::RenderSurfaceReason GetRenderSurfaceCandidateReason(
    const cc::EffectNode& effect,
    const Vector<const EffectPaintPropertyNode*>& blink_effects) {
  if (effect.HasRenderSurface())
    return cc::RenderSurfaceReason::kNone;
  if (effect.blend_mode != SkBlendMode::kSrcOver)
    return cc::RenderSurfaceReason::kBlendModeDstIn;
  if (effect.opacity != 1.f)
    return cc::RenderSurfaceReason::kOpacity;
  if (static_cast<wtf_size_t>(effect.id) < blink_effects.size() &&
      blink_effects[effect.id] &&
      blink_effects[effect.id]->HasActiveOpacityAnimation())
    return cc::RenderSurfaceReason::kOpacityAnimation;
  // Applying a rounded corner clip to more than one layer descendant
  // with highest quality requires a render surface, due to the possibility
  // of antialiasing issues on the rounded corner edges.
  // is_fast_rounded_corner means to intentionally prefer faster compositing
  // and less memory over highest quality.
  if (effect.mask_filter_info.HasRoundedCorners() &&
      !effect.is_fast_rounded_corner)
    return cc::RenderSurfaceReason::kRoundedCorner;
  return cc::RenderSurfaceReason::kNone;
}

// Every effect is supposed to have render surface enabled for grouping, but
// we can omit one if the effect is opacity- or blend-mode-only, render
// surface is not forced, and the effect has only one compositing child. This
// is both for optimization and not introducing sub-pixel differences in web
// tests.
// TODO(crbug.com/504464): There is ongoing work in cc to delay render surface
// decision until later phase of the pipeline. Remove premature optimization
// here once the work is ready.
void PaintArtifactCompositor::UpdateRenderSurfaceForEffects(
    cc::EffectTree& effect_tree,
    const cc::LayerList& layers,
    const Vector<const EffectPaintPropertyNode*>& blink_effects) {
  // This vector is indexed by effect node id. The value is the number of
  // layers and sub-render-surfaces controlled by this effect.
  Vector<int> effect_layer_counts(static_cast<wtf_size_t>(effect_tree.size()));
  // Initialize the vector to count directly controlled layers.
  for (const auto& layer : layers) {
    if (layer->DrawsContent())
      effect_layer_counts[layer->effect_tree_index()]++;
  }

  // In the effect tree, parent always has lower id than children, so the
  // following loop will check descendants before parents and accumulate
  // effect_layer_counts.
  for (int id = static_cast<int>(effect_tree.size() - 1);
       id > cc::EffectTree::kSecondaryRootNodeId; id--) {
    auto* effect = effect_tree.Node(id);
    if (effect_layer_counts[id] > 1) {
      auto reason = GetRenderSurfaceCandidateReason(*effect, blink_effects);
      if (reason != cc::RenderSurfaceReason::kNone) {
        // The render surface candidate needs a render surface because it
        // controls more than 1 layer.
        effect->render_surface_reason = reason;
      }
    }

    // We should not have visited the parent.
    DCHECK_NE(-1, effect_layer_counts[effect->parent_id]);
    if (effect->HasRenderSurface()) {
      // A sub-render-surface counts as one controlled layer of the parent.
      effect_layer_counts[effect->parent_id]++;
    } else {
      // Otherwise all layers count as controlled layers of the parent.
      effect_layer_counts[effect->parent_id] += effect_layer_counts[id];
    }

#if DCHECK_IS_ON()
    // Mark we have visited this effect.
    effect_layer_counts[id] = -1;
#endif
  }
}

void PaintArtifactCompositor::SetLayerDebugInfoEnabled(bool enabled) {
  if (enabled == layer_debug_info_enabled_)
    return;

  DCHECK(needs_update_);
  layer_debug_info_enabled_ = enabled;

  if (enabled) {
    root_layer_->SetDebugName("root");
  } else {
    root_layer_->ClearDebugInfo();
    for (auto& layer : root_layer_->children())
      layer->ClearDebugInfo();
  }
}

static void UpdateLayerDebugInfo(
    cc::Layer& layer,
    const PaintChunk::Id& id,
    const String& name,
    DOMNodeId owner_node_id,
    CompositingReasons compositing_reasons,
    RasterInvalidationTracking* raster_invalidation_tracking) {
  cc::LayerDebugInfo& debug_info = layer.EnsureDebugInfo();

  debug_info.name = name.Utf8();
  if (id.type == DisplayItem::kForeignLayerContentsWrapper) {
    // This is for backward compatibility in pre-CompositeAfterPaint mode.
    DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    debug_info.name = std::string("ContentsLayer for ") + debug_info.name;
  }

  debug_info.compositing_reasons =
      CompositingReason::Descriptions(compositing_reasons);
  debug_info.compositing_reason_ids =
      CompositingReason::ShortNames(compositing_reasons);
  debug_info.owner_node_id = owner_node_id;

  if (RasterInvalidationTracking::IsTracingRasterInvalidations() &&
      raster_invalidation_tracking) {
    raster_invalidation_tracking->AddToLayerDebugInfo(debug_info);
    raster_invalidation_tracking->ClearInvalidations();
  }
}

static void UpdateLayerDebugInfo(
    cc::Layer& layer,
    const PaintChunk::Id& id,
    const PaintArtifact& paint_artifact,
    CompositingReasons compositing_reasons,
    RasterInvalidationTracking* raster_invalidation_tracking) {
  UpdateLayerDebugInfo(layer, id, paint_artifact.ClientDebugName(id.client_id),
                       paint_artifact.ClientOwnerNodeId(id.client_id),
                       compositing_reasons, raster_invalidation_tracking);
}

void PaintArtifactCompositor::UpdateDebugInfo() const {
  if (!layer_debug_info_enabled_)
    return;

  auto* content_layer_client_it = content_layer_clients_.begin();
  auto* scroll_hit_test_layer_it = scroll_hit_test_layers_.begin();
  auto* scrollbar_layer_it = scrollbar_layers_.begin();
  const PendingLayer* previous_pending_layer = nullptr;
  for (const auto& pending_layer : pending_layers_) {
    cc::Layer* layer;
    RasterInvalidationTracking* tracking = nullptr;
    switch (pending_layer.GetCompositingType()) {
      case PendingLayer::kPreCompositedLayer:
        tracking =
            pending_layer.GetGraphicsLayer()->GetRasterInvalidationTracking();
        layer = &pending_layer.GetGraphicsLayer()->CcLayer();
        break;
      case PendingLayer::kForeignLayer:
        layer = To<ForeignLayerDisplayItem>(pending_layer.FirstDisplayItem())
                    .GetLayer();
        break;
      case PendingLayer::kScrollbarLayer:
        layer = scrollbar_layer_it->get();
        ++scrollbar_layer_it;
        break;
      case PendingLayer::kScrollHitTestLayer:
        layer = scroll_hit_test_layer_it->get();
        ++scroll_hit_test_layer_it;
        break;
      default:
        tracking =
            (*content_layer_client_it)->GetRasterInvalidator().GetTracking();
        layer = &(*content_layer_client_it)->Layer();
        ++content_layer_client_it;
        break;
    }
    if (pending_layer.GetGraphicsLayer()) {
      UpdateLayerDebugInfo(
          *layer,
          PaintChunk::Id(pending_layer.GetGraphicsLayer()->Id(),
                         DisplayItem::kUninitializedType),
          pending_layer.GetGraphicsLayer()->DebugName(),
          pending_layer.GetGraphicsLayer()->OwnerNodeId(),
          GetCompositingReasons(pending_layer, previous_pending_layer),
          tracking);
    } else {
      UpdateLayerDebugInfo(
          *layer, pending_layer.FirstPaintChunk().id,
          pending_layer.Chunks().GetPaintArtifact(),
          GetCompositingReasons(pending_layer, previous_pending_layer),
          tracking);
    }
    previous_pending_layer = &pending_layer;
  }
}

CompositingReasons PaintArtifactCompositor::GetCompositingReasons(
    const PendingLayer& layer,
    const PendingLayer* previous_layer) const {
  DCHECK(layer_debug_info_enabled_);

  if (layer.GetGraphicsLayer())
    return layer.GetGraphicsLayer()->GetCompositingReasons();

  if (layer.RequiresOwnLayer()) {
    if (layer.GetCompositingType() == PendingLayer::kScrollHitTestLayer)
      return CompositingReason::kOverflowScrolling;
    switch (layer.FirstDisplayItem().GetType()) {
      case DisplayItem::kForeignLayerCanvas:
        return CompositingReason::kCanvas;
      case DisplayItem::kForeignLayerPlugin:
        return CompositingReason::kPlugin;
      case DisplayItem::kForeignLayerVideo:
        return CompositingReason::kVideo;
      case DisplayItem::kScrollbarHorizontal:
        return CompositingReason::kLayerForHorizontalScrollbar;
      case DisplayItem::kScrollbarVertical:
        return CompositingReason::kLayerForVerticalScrollbar;
      default:
        return CompositingReason::kLayerForOther;
    }
  }

  CompositingReasons reasons = CompositingReason::kNone;
  if (!previous_layer ||
      &layer.GetPropertyTreeState().Transform() !=
          &previous_layer->GetPropertyTreeState().Transform()) {
    reasons |= layer.GetPropertyTreeState()
                   .Transform()
                   .DirectCompositingReasonsForDebugging();
    if (!layer.GetPropertyTreeState()
             .Transform()
             .BackfaceVisibilitySameAsParent())
      reasons |= CompositingReason::kBackfaceVisibilityHidden;
  }

  if (!previous_layer || &layer.GetPropertyTreeState().Effect() !=
                             &previous_layer->GetPropertyTreeState().Effect()) {
    const auto& effect = layer.GetPropertyTreeState().Effect();
    if (effect.HasDirectCompositingReasons())
      reasons |= effect.DirectCompositingReasonsForDebugging();
    if (reasons == CompositingReason::kNone &&
        layer.GetCompositingType() == PendingLayer::kOther) {
      if (effect.Opacity() != 1.0f)
        reasons |= CompositingReason::kOpacityWithCompositedDescendants;
      if (!effect.Filter().IsEmpty())
        reasons |= CompositingReason::kFilterWithCompositedDescendants;
      if (effect.BlendMode() == SkBlendMode::kDstIn)
        reasons |= CompositingReason::kMaskWithCompositedDescendants;
      else if (effect.BlendMode() != SkBlendMode::kSrcOver)
        reasons |= CompositingReason::kBlendingWithCompositedDescendants;
    }
  }

  if (reasons == CompositingReason::kNone &&
      layer.GetCompositingType() == PendingLayer::kOverlap)
    reasons = CompositingReason::kOverlap;

  return reasons;
}

Vector<cc::Layer*> PaintArtifactCompositor::SynthesizedClipLayersForTesting()
    const {
  Vector<cc::Layer*> synthesized_clip_layers;
  for (const auto& entry : synthesized_clip_cache_)
    synthesized_clip_layers.push_back(entry.synthesized_clip->Layer());
  return synthesized_clip_layers;
}

void PaintArtifactCompositor::ClearPropertyTreeChangedState() {
  for (auto& layer : pending_layers_) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // The chunks ref-counted property tree state keeps the |layer|'s non-ref
      // property tree pointers alive and all chunk property tree states should
      // be descendants of the |layer|'s. Therefore, we can just CHECK that the
      // first chunk's references are keeping the |layer|'s property tree state
      // alive.
      CHECK(!layer.Chunks().IsEmpty());
      const auto& layer_state = layer.GetPropertyTreeState();
      const auto& first_chunk_state =
          layer.Chunks().begin()->properties.GetPropertyTreeState();
      CHECK(
          layer_state.Transform().IsAncestorOf(first_chunk_state.Transform()));
      CHECK(layer_state.Clip().IsAncestorOf(first_chunk_state.Clip()));
      CHECK(layer_state.Effect().IsAncestorOf(first_chunk_state.Effect()));
    }

    layer.GetPropertyTreeState().ClearChangedTo(PropertyTreeState::Root());

    PaintChunkSubset chunks =
        layer.GetGraphicsLayer() &&
                layer.GetGraphicsLayer()->PaintsContentOrHitTest()
            ? PaintChunkSubset(layer.GetGraphicsLayer()
                                   ->GetPaintController()
                                   .GetPaintArtifactShared())
            : layer.Chunks();
    for (auto& chunk : chunks) {
      // Calling |ClearChangedTo| for every chunk could be O(|property nodes|^2)
      // in the worst case and could be optimized by caching which nodes that
      // have already been cleared.
      chunk.properties.GetPropertyTreeState().ClearChangedTo(
          layer.GetPropertyTreeState());
    }
  }
}

size_t PaintArtifactCompositor::ApproximateUnsharedMemoryUsage() const {
  size_t result = sizeof(*this) + content_layer_clients_.CapacityInBytes() +
                  synthesized_clip_cache_.CapacityInBytes() +
                  scroll_hit_test_layers_.CapacityInBytes() +
                  scrollbar_layers_.CapacityInBytes() +
                  pending_layers_.CapacityInBytes();

  for (auto& client : content_layer_clients_)
    result += client->ApproximateUnsharedMemoryUsage();

  for (auto& layer : pending_layers_) {
    size_t chunks_size = layer.Chunks().ApproximateUnsharedMemoryUsage();
    DCHECK_GE(chunks_size, sizeof(layer.Chunks()));
    result += chunks_size - sizeof(layer.Chunks());
  }
  return result;
}

void PaintArtifactCompositor::SetScrollbarNeedsDisplay(
    CompositorElementId element_id) {
  for (auto& layer : scrollbar_layers_) {
    if (layer->element_id() == element_id) {
      layer->SetNeedsDisplay();
      return;
    }
  }
}

void LayerListBuilder::Add(scoped_refptr<cc::Layer> layer) {
#if DCHECK_IS_ON()
  DCHECK(list_valid_);
  DCHECK(!layer_ids_.Contains(layer->id()));
  layer_ids_.insert(layer->id());
#endif
  list_.push_back(layer);
}

cc::LayerList LayerListBuilder::Finalize() {
  DCHECK(list_valid_);
  list_valid_ = false;
  return std::move(list_);
}

#if DCHECK_IS_ON()
void PaintArtifactCompositor::ShowDebugData() {
  LOG(INFO) << GetLayersAsJSON(kLayerTreeIncludesDebugInfo |
                               kLayerTreeIncludesDetailedInvalidations)
                   ->ToPrettyJSONString()
                   .Utf8();
}
#endif

}  // namespace blink

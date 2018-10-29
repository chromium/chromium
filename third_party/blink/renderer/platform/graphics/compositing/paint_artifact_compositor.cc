// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

#include <memory>
#include <utility>

#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_hit_test_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

// cc property trees make use of a sequence number to identify when tree
// topology changes. For now we naively increment the sequence number each time
// we update the property trees. We should explore optimizing our management of
// the sequence number through the use of a dirty bit or similar. See
// http://crbug.com/692842#c4.
static int g_s_property_tree_sequence_number = 1;

PaintArtifactCompositor::PaintArtifactCompositor(
    base::RepeatingCallback<void(const gfx::ScrollOffset&,
                                 const cc::ElementId&)> scroll_callback)
    : scroll_callback_(std::move(scroll_callback)),
      tracks_raster_invalidations_(false) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      !RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    return;
  root_layer_ = cc::Layer::Create();
}

PaintArtifactCompositor::~PaintArtifactCompositor() {
  // TODO(crbug.com/836897, crbug.com/836912):
  // In BlinkGenPropertyTrees mode, some of the layers passed from Blink core
  // have pre-filled element ID. Need to figure out what is the best place to
  // setup them.
  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    return;
  for (auto child : root_layer_->children())
    DCHECK(!child->element_id());
}

void PaintArtifactCompositor::EnableExtraDataForTesting() {
  extra_data_for_testing_enabled_ = true;
  extra_data_for_testing_ = std::make_unique<ExtraDataForTesting>();
}

void PaintArtifactCompositor::SetTracksRasterInvalidations(bool should_track) {
  for (auto& client : content_layer_clients_)
    client->GetRasterInvalidator().SetTracksRasterInvalidations(should_track);
}

void PaintArtifactCompositor::WillBeRemovedFromFrame() {
  root_layer_->RemoveAllChildren();
  if (extra_data_for_testing_enabled_) {
    extra_data_for_testing_->content_layers.clear();
    extra_data_for_testing_->synthesized_clip_layers.clear();
    extra_data_for_testing_->scroll_hit_test_layers.clear();
  }
}

std::unique_ptr<JSONObject> PaintArtifactCompositor::LayersAsJSON(
    LayerTreeFlags flags) const {
  ContentLayerClientImpl::LayerAsJSONContext context(flags);
  std::unique_ptr<JSONArray> layers_json = JSONArray::Create();
  for (const auto& client : content_layer_clients_) {
    layers_json->PushObject(client->LayerAsJSON(context));
  }
  std::unique_ptr<JSONObject> json = JSONObject::Create();
  json->SetArray("layers", std::move(layers_json));
  if (context.transforms_json)
    json->SetArray("transforms", std::move(context.transforms_json));
  return json;
}

static scoped_refptr<cc::Layer> ForeignLayerForPaintChunk(
    const PaintArtifact& paint_artifact,
    const PaintChunk& paint_chunk,
    gfx::Vector2dF& layer_offset) {
  if (paint_chunk.size() != 1)
    return nullptr;

  const auto& display_item =
      paint_artifact.GetDisplayItemList()[paint_chunk.begin_index];
  if (!display_item.IsForeignLayer())
    return nullptr;

  const auto& foreign_layer_display_item =
      static_cast<const ForeignLayerDisplayItem&>(display_item);
  layer_offset = gfx::Vector2dF(foreign_layer_display_item.Location().X(),
                                foreign_layer_display_item.Location().Y());
  scoped_refptr<cc::Layer> layer = foreign_layer_display_item.GetLayer();
  DCHECK(layer->bounds() ==
         static_cast<gfx::Size>(foreign_layer_display_item.Bounds()))
      << "\n  layer bounds: " << layer->bounds().ToString()
      << "\n  display item bounds: " << foreign_layer_display_item.Bounds();
  return layer;
}

const TransformPaintPropertyNode&
PaintArtifactCompositor::ScrollTranslationForPendingLayer(
    const PaintArtifact& paint_artifact,
    const PendingLayer& pending_layer) {
  if (const auto* scroll_translation = ScrollTranslationForScrollHitTestLayer(
          paint_artifact, pending_layer)) {
    return *scroll_translation;
  }
  const auto* transform = pending_layer.property_tree_state.Transform();
  // TODO(pdr): This could be a performance issue because it crawls up the
  // transform tree for each pending layer. If this is on profiles, we should
  // cache a lookup of transform node to scroll translation transform node.
  return transform->NearestScrollTranslationNode();
}

const TransformPaintPropertyNode*
PaintArtifactCompositor::ScrollTranslationForScrollHitTestLayer(
    const PaintArtifact& paint_artifact,
    const PendingLayer& pending_layer) {
  auto paint_chunks =
      paint_artifact.GetPaintChunkSubset(pending_layer.paint_chunk_indices);
  DCHECK(paint_chunks.size());
  const auto& first_paint_chunk = paint_chunks[0];
  if (first_paint_chunk.size() != 1)
    return nullptr;

  const auto& display_item =
      paint_artifact.GetDisplayItemList()[first_paint_chunk.begin_index];
  if (!display_item.IsScrollHitTest())
    return nullptr;

  const auto& scroll_hit_test_display_item =
      static_cast<const ScrollHitTestDisplayItem&>(display_item);
  return &scroll_hit_test_display_item.scroll_offset_node();
}

scoped_refptr<cc::Layer>
PaintArtifactCompositor::ScrollHitTestLayerForPendingLayer(
    const PaintArtifact& paint_artifact,
    const PendingLayer& pending_layer,
    gfx::Vector2dF& layer_offset) {
  const auto* scroll_offset_node =
      ScrollTranslationForScrollHitTestLayer(paint_artifact, pending_layer);
  if (!scroll_offset_node)
    return nullptr;

  const auto& scroll_node = *scroll_offset_node->ScrollNode();
  auto scroll_element_id = scroll_node.GetCompositorElementId();

  scoped_refptr<cc::Layer> scroll_layer;
  for (auto& existing_layer : scroll_hit_test_layers_) {
    if (existing_layer && existing_layer->element_id() == scroll_element_id)
      scroll_layer = existing_layer;
  }
  if (!scroll_layer) {
    scroll_layer = cc::Layer::Create();
    scroll_layer->SetElementId(scroll_element_id);
  }

  // TODO(pdr): Add a helper for blink::FloatPoint to gfx::Vector2dF.
  auto offset = scroll_node.ContainerRect().Location();
  layer_offset = gfx::Vector2dF(offset.X(), offset.Y());
  // TODO(pdr): The scroll layer's bounds are currently set to the clipped
  // container bounds but this does not include the border. We may want to
  // change this behavior to make non-composited and composited hit testing
  // match (see: crbug.com/753124).
  auto bounds = scroll_node.ContainerRect().Size();
  // Mark the layer as scrollable.
  // TODO(pdr): When SPV2 launches this parameter for bounds will not be needed.
  scroll_layer->SetScrollable(static_cast<gfx::Size>(bounds));
  // Set the layer's bounds equal to the container because the scroll layer
  // does not scroll.
  scroll_layer->SetBounds(static_cast<gfx::Size>(bounds));
  scroll_layer->set_did_scroll_callback(scroll_callback_);
  return scroll_layer;
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
    scoped_refptr<const PaintArtifact> paint_artifact,
    const PendingLayer& pending_layer,
    gfx::Vector2dF& layer_offset,
    Vector<std::unique_ptr<ContentLayerClientImpl>>& new_content_layer_clients,
    Vector<scoped_refptr<cc::Layer>>& new_scroll_hit_test_layers) {
  auto paint_chunks =
      paint_artifact->GetPaintChunkSubset(pending_layer.paint_chunk_indices);
  DCHECK(paint_chunks.size());
  const PaintChunk& first_paint_chunk = paint_chunks[0];
  DCHECK(first_paint_chunk.size());

  // If the paint chunk is a foreign layer, just return that layer.
  if (scoped_refptr<cc::Layer> foreign_layer = ForeignLayerForPaintChunk(
          *paint_artifact, first_paint_chunk, layer_offset)) {
    DCHECK_EQ(paint_chunks.size(), 1u);
    if (extra_data_for_testing_enabled_)
      extra_data_for_testing_->content_layers.push_back(foreign_layer);
    return foreign_layer;
  }

  // If the paint chunk is a scroll hit test layer, lookup/create the layer.
  if (scoped_refptr<cc::Layer> scroll_layer = ScrollHitTestLayerForPendingLayer(
          *paint_artifact, pending_layer, layer_offset)) {
    new_scroll_hit_test_layers.push_back(scroll_layer);
    if (extra_data_for_testing_enabled_)
      extra_data_for_testing_->scroll_hit_test_layers.push_back(scroll_layer);
    return scroll_layer;
  }

  // The common case: create or reuse a PictureLayer for painted content.
  std::unique_ptr<ContentLayerClientImpl> content_layer_client =
      ClientForPaintChunk(first_paint_chunk);

  gfx::Rect cc_combined_bounds(EnclosingIntRect(pending_layer.bounds));
  layer_offset = cc_combined_bounds.OffsetFromOrigin();

  auto cc_layer = content_layer_client->UpdateCcPictureLayer(
      paint_artifact, paint_chunks, cc_combined_bounds,
      pending_layer.property_tree_state);
  new_content_layer_clients.push_back(std::move(content_layer_client));
  if (extra_data_for_testing_enabled_)
    extra_data_for_testing_->content_layers.push_back(cc_layer);

  // Set properties that foreign layers would normally control for themselves
  // here to avoid changing foreign layers. This includes things set by
  // GraphicsLayer on the ContentsLayer() or by video clients etc.
  cc_layer->SetContentsOpaque(pending_layer.rect_known_to_be_opaque.Contains(
      FloatRect(EnclosingIntRect(pending_layer.bounds))));

  return cc_layer;
}

PaintArtifactCompositor::PendingLayer::PendingLayer(
    const PaintChunk& first_paint_chunk,
    size_t chunk_index,
    bool chunk_requires_own_layer)
    : bounds(first_paint_chunk.bounds),
      rect_known_to_be_opaque(
          first_paint_chunk.known_to_be_opaque ? bounds : FloatRect()),
      property_tree_state(
          first_paint_chunk.properties.GetPropertyTreeState().Unalias()),
      requires_own_layer(chunk_requires_own_layer) {
  paint_chunk_indices.push_back(chunk_index);
}

void PaintArtifactCompositor::PendingLayer::Merge(const PendingLayer& guest) {
  DCHECK(!requires_own_layer && !guest.requires_own_layer);

  paint_chunk_indices.AppendVector(guest.paint_chunk_indices);
  FloatClipRect guest_bounds_in_home(guest.bounds);
  GeometryMapper::LocalToAncestorVisualRect(
      guest.property_tree_state, property_tree_state, guest_bounds_in_home);
  bounds.Unite(guest_bounds_in_home.Rect());
  // TODO(crbug.com/701991): Upgrade GeometryMapper.
  // If we knew the new bounds is enclosed by the mapped opaque region of
  // the guest layer, we can deduce the merged layer being opaque too, and
  // update rect_known_to_be_opaque accordingly.
}

static bool CanUpcastTo(const PropertyTreeState& guest,
                        const PropertyTreeState& home);
bool PaintArtifactCompositor::PendingLayer::CanMerge(
    const PendingLayer& guest) const {
  if (requires_own_layer || guest.requires_own_layer)
    return false;
  if (property_tree_state.Effect()->Unalias() !=
      guest.property_tree_state.Effect()->Unalias()) {
    return false;
  }
  return CanUpcastTo(guest.property_tree_state, property_tree_state);
}

void PaintArtifactCompositor::PendingLayer::Upcast(
    const PropertyTreeState& new_state) {
  DCHECK(!requires_own_layer);
  FloatClipRect float_clip_rect(bounds);
  GeometryMapper::LocalToAncestorVisualRect(property_tree_state, new_state,
                                            float_clip_rect);
  bounds = float_clip_rect.Rect();

  property_tree_state = new_state;
  // TODO(crbug.com/701991): Upgrade GeometryMapper.
  // A local visual rect mapped to an ancestor space may become a polygon
  // (e.g. consider transformed clip), also effects may affect the opaque
  // region. To determine whether the layer is still opaque, we need to
  // query conservative opaque rect after mapping to an ancestor space,
  // which is not supported by GeometryMapper yet.
  rect_known_to_be_opaque = FloatRect();
}

static bool IsNonCompositingAncestorOf(
    const TransformPaintPropertyNode* unaliased_ancestor,
    const TransformPaintPropertyNode* node) {
  for (; node != unaliased_ancestor; node = SafeUnalias(node->Parent())) {
    if (!node || node->HasDirectCompositingReasons())
      return false;
  }
  return true;
}

// Determines whether drawings based on the 'guest' state can be painted into
// a layer with the 'home' state. A number of criteria need to be met:
// 1. The guest effect must be a descendant of the home effect. However this
//    check is enforced by the layerization recursion. Here we assume the guest
//    has already been upcasted to the same effect.
// 2. The guest transform and the home transform have compatible backface
//    visibility.
// 3. The guest clip must be a descendant of the home clip.
// 4. The local space of each clip and effect node on the ancestor chain must
//    be within compositing boundary of the home transform space.
// 5. The guest transform space must be within compositing boundary of the home
//    transform space.
static bool CanUpcastTo(const PropertyTreeState& guest,
                        const PropertyTreeState& home) {
  DCHECK_EQ(home.Effect()->Unalias(), guest.Effect()->Unalias());

  if (home.Transform()->IsBackfaceHidden() !=
      guest.Transform()->IsBackfaceHidden())
    return false;

  auto* home_clip = home.Clip()->Unalias();
  for (const ClipPaintPropertyNode* current_clip = guest.Clip()->Unalias();
       current_clip != home_clip;
       current_clip = current_clip->Parent() ? current_clip->Parent()->Unalias()
                                             : nullptr) {
    if (!current_clip || current_clip->HasDirectCompositingReasons())
      return false;
    if (!IsNonCompositingAncestorOf(
            home.Transform()->Unalias(),
            current_clip->LocalTransformSpace()->Unalias())) {
      return false;
    }
  }

  return IsNonCompositingAncestorOf(home.Transform()->Unalias(),
                                    guest.Transform()->Unalias());
}

// Returns nullptr if 'ancestor' is not a strict ancestor of 'node'.
// Otherwise, return the child of 'ancestor' that is an ancestor of 'node' or
// 'node' itself.
static const EffectPaintPropertyNode* StrictUnaliasedChildOfAlongPath(
    const EffectPaintPropertyNode* unaliased_ancestor,
    const EffectPaintPropertyNode* node) {
  node = node->Unalias();
  while (node) {
    auto* parent = SafeUnalias(node->Parent());
    if (parent == unaliased_ancestor)
      return node;
    node = parent;
  }
  return nullptr;
}

bool PaintArtifactCompositor::MightOverlap(const PendingLayer& layer_a,
                                           const PendingLayer& layer_b) {
  FloatClipRect bounds_a(layer_a.bounds);
  GeometryMapper::LocalToAncestorVisualRect(
      layer_a.property_tree_state, PropertyTreeState::Root(), bounds_a);
  FloatClipRect bounds_b(layer_b.bounds);
  GeometryMapper::LocalToAncestorVisualRect(
      layer_b.property_tree_state, PropertyTreeState::Root(), bounds_b);

  return bounds_a.Rect().Intersects(bounds_b.Rect());
}

bool PaintArtifactCompositor::CanDecompositeEffect(
    const EffectPaintPropertyNode* unaliased_effect,
    const PendingLayer& layer) {
  // If the effect associated with the layer is deeper than than the effect
  // we are attempting to decomposite, than implies some previous decision
  // did not allow to decomposite intermediate effects.
  if (layer.property_tree_state.Effect()->Unalias() != unaliased_effect)
    return false;
  if (layer.requires_own_layer)
    return false;
  // TODO(trchen): Exotic blending layer may be decomposited only if it could
  // be merged into the first layer of the current group.
  if (unaliased_effect->BlendMode() != SkBlendMode::kSrcOver)
    return false;
  if (unaliased_effect->HasDirectCompositingReasons())
    return false;
  if (!CanUpcastTo(layer.property_tree_state,
                   PropertyTreeState(unaliased_effect->LocalTransformSpace(),
                                     unaliased_effect->OutputClip()
                                         ? unaliased_effect->OutputClip()
                                         : layer.property_tree_state.Clip(),
                                     unaliased_effect)))
    return false;
  return true;
}

static bool EffectGroupContainsChunk(
    const EffectPaintPropertyNode& unaliased_group_effect,
    const PaintChunk& chunk) {
  const auto* effect = SafeUnalias(chunk.properties.Effect());
  return effect == &unaliased_group_effect ||
         StrictUnaliasedChildOfAlongPath(&unaliased_group_effect, effect);
}

static bool SkipGroupIfEffectivelyInvisible(
    const PaintArtifact& paint_artifact,
    const EffectPaintPropertyNode& unaliased_group,
    Vector<PaintChunk>::const_iterator& chunk_it) {
  // The lower bound of visibility is considered to be 0.0004f < 1/2048. With
  // 10-bit color channels (only available on the newest Macs as of 2016;
  // otherwise it's 8-bit), we see that an alpha of 1/2048 or less leads to a
  // color output of less than 0.5 in all channels, hence not visible.
  static const float kMinimumVisibleOpacity = 0.0004f;
  if (unaliased_group.Opacity() >= kMinimumVisibleOpacity ||
      unaliased_group.HasDirectCompositingReasons()) {
    return false;
  }

  // Fast-forward to just past the end of the chunk sequence within this
  // effect group.
  DCHECK(EffectGroupContainsChunk(unaliased_group, *chunk_it));
  while (++chunk_it != paint_artifact.PaintChunks().end()) {
    if (!EffectGroupContainsChunk(unaliased_group, *chunk_it))
      break;
  }
  return true;
}

void PaintArtifactCompositor::LayerizeGroup(
    const PaintArtifact& paint_artifact,
    Vector<PendingLayer>& pending_layers,
    const EffectPaintPropertyNode& current_group,
    Vector<PaintChunk>::const_iterator& chunk_it) {
  // Skip paint chunks that are effectively invisible due to opacity and don't
  // have a direct compositing reason.
  const auto& unaliased_group = *current_group.Unalias();
  if (SkipGroupIfEffectivelyInvisible(paint_artifact, unaliased_group,
                                      chunk_it))
    return;

  size_t first_layer_in_current_group = pending_layers.size();
  // The worst case time complexity of the algorithm is O(pqd), where
  // p = the number of paint chunks.
  // q = average number of trials to find a squash layer or rejected
  //     for overlapping.
  // d = (sum of) the depth of property trees.
  // The analysis as follows:
  // Every paint chunk will be visited by the main loop below for exactly once,
  // except for chunks that enter or exit groups (case B & C below).
  // For normal chunk visit (case A), the only cost is determining squash,
  // which costs O(qd), where d came from "canUpcastTo" and geometry mapping.
  // Subtotal: O(pqd)
  // For group entering and exiting, it could cost O(d) for each group, for
  // searching the shallowest subgroup (strictChildOfAlongPath), thus O(d^2)
  // in total.
  // Also when exiting group, the group may be decomposited and squashed to a
  // previous layer. Again finding the host costs O(qd). Merging would cost O(p)
  // due to copying the chunk list. Subtotal: O((qd + p)d) = O(qd^2 + pd)
  // Assuming p > d, the total complexity would be O(pqd + qd^2 + pd) = O(pqd)
  while (chunk_it != paint_artifact.PaintChunks().end()) {
    // Look at the effect node of the next chunk. There are 3 possible cases:
    // A. The next chunk belongs to the current group but no subgroup.
    // B. The next chunk does not belong to the current group.
    // C. The next chunk belongs to some subgroup of the current group.
    DCHECK(chunk_it->properties.Effect());
    const auto* chunk_effect = chunk_it->properties.Effect()->Unalias();
    if (chunk_effect == &unaliased_group) {
      // Case A: The next chunk belongs to the current group but no subgroup.
      const auto& last_display_item =
          paint_artifact.GetDisplayItemList()[chunk_it->begin_index];
      bool requires_own_layer = last_display_item.IsForeignLayer() ||
                                // TODO(pdr): This should require a direct
                                // compositing reason.
                                last_display_item.IsScrollHitTest();
      pending_layers.push_back(PendingLayer(
          *chunk_it, chunk_it - paint_artifact.PaintChunks().begin(),
          requires_own_layer));
      chunk_it++;
      if (requires_own_layer)
        continue;
    } else {
      const EffectPaintPropertyNode* unaliased_subgroup =
          StrictUnaliasedChildOfAlongPath(&unaliased_group, chunk_effect);
      // Case B: This means we need to close the current group without
      //         processing the next chunk.
      if (!unaliased_subgroup)
        break;
      // Case C: The following chunks belong to a subgroup. Process them by
      //         a recursion call.
      size_t first_layer_in_subgroup = pending_layers.size();
      LayerizeGroup(paint_artifact, pending_layers, *unaliased_subgroup,
                    chunk_it);
      // Now the chunk iterator stepped over the subgroup we just saw.
      // If the subgroup generated 2 or more layers then the subgroup must be
      // composited to satisfy grouping requirement.
      // i.e. Grouping effects generally must be applied atomically,
      // for example,  Opacity(A+B) != Opacity(A) + Opacity(B), thus an effect
      // either applied 100% within a layer, or not at all applied within layer
      // (i.e. applied by compositor render surface instead).
      if (pending_layers.size() != first_layer_in_subgroup + 1)
        continue;
      // Now attempt to "decomposite" subgroup.
      PendingLayer& subgroup_layer = pending_layers[first_layer_in_subgroup];
      if (!CanDecompositeEffect(unaliased_subgroup, subgroup_layer))
        continue;
      subgroup_layer.Upcast(
          PropertyTreeState(unaliased_subgroup->LocalTransformSpace(),
                            unaliased_subgroup->OutputClip()
                                ? unaliased_subgroup->OutputClip()
                                : subgroup_layer.property_tree_state.Clip(),
                            &unaliased_group));
    }
    // At this point pending_layers.back() is the either a layer from a
    // "decomposited" subgroup or a layer created from a chunk we just
    // processed. Now determine whether it could be merged into a previous
    // layer.
    const PendingLayer& new_layer = pending_layers.back();
    DCHECK(!new_layer.requires_own_layer);
    DCHECK_EQ(&unaliased_group, new_layer.property_tree_state.Effect());
    // This iterates pending_layers[first_layer_in_current_group:-1] in reverse.
    for (size_t candidate_index = pending_layers.size() - 1;
         candidate_index-- > first_layer_in_current_group;) {
      PendingLayer& candidate_layer = pending_layers[candidate_index];
      if (candidate_layer.CanMerge(new_layer)) {
        candidate_layer.Merge(new_layer);
        pending_layers.pop_back();
        break;
      }
      if (MightOverlap(new_layer, candidate_layer))
        break;
    }
  }
}

void PaintArtifactCompositor::CollectPendingLayers(
    const PaintArtifact& paint_artifact,
    Vector<PendingLayer>& pending_layers) {
  Vector<PaintChunk>::const_iterator cursor =
      paint_artifact.PaintChunks().begin();
  LayerizeGroup(paint_artifact, pending_layers, EffectPaintPropertyNode::Root(),
                cursor);
  DCHECK_EQ(paint_artifact.PaintChunks().end(), cursor);
}

// This class maintains a persistent mask layer and unique stable cc effect IDs
// for reuse across compositing cycles. The mask layer paints a rounded rect,
// which is an updatable parameter of the class. The caller is responsible for
// inserting the layer into layer list and associating with property nodes.
//
// The typical application of the mask layer is to create an isolating effect
// node to paint the clipped contents, and at the end draw the mask layer with
// a kDstIn blend effect. This is why two stable cc effect IDs are provided for
// the convenience of the caller, although they are not directly related to the
// class functionality.
class SynthesizedClip : private cc::ContentLayerClient {
 public:
  SynthesizedClip() : layer_(cc::PictureLayer::Create(this)) {
    mask_isolation_id_ =
        CompositorElementIdFromUniqueObjectId(NewUniqueObjectId());
    mask_effect_id_ =
        CompositorElementIdFromUniqueObjectId(NewUniqueObjectId());
    layer_->SetIsDrawable(true);
  }

  void Update(const FloatRoundedRect& rrect,
              scoped_refptr<const RefCountedPath> path) {
    IntRect layer_bounds = EnclosingIntRect(rrect.Rect());
    gfx::Vector2dF new_layer_origin(layer_bounds.X(), layer_bounds.Y());

    SkRRect new_local_rrect = rrect;
    new_local_rrect.offset(-new_layer_origin.x(), -new_layer_origin.y());

    bool path_in_layer_changed = false;
    if (path_ == path) {
      path_in_layer_changed = path && layer_origin_ != new_layer_origin;
    } else if (!path_ || !path) {
      path_in_layer_changed = true;
    } else {
      SkPath new_path = path->GetSkPath();
      new_path.offset(layer_origin_.x() - new_layer_origin.x(),
                      layer_origin_.y() - new_layer_origin.y());
      path_in_layer_changed = path_->GetSkPath() != new_path;
    }

    if (local_rrect_ != new_local_rrect || path_in_layer_changed) {
      layer_->SetNeedsDisplay();
    }
    layer_->SetOffsetToTransformParent(new_layer_origin);
    layer_->SetBounds(gfx::Size(layer_bounds.Width(), layer_bounds.Height()));

    layer_origin_ = new_layer_origin;
    local_rrect_ = new_local_rrect;
    path_ = std::move(path);
  }

  cc::Layer* GetLayer() const { return layer_.get(); }
  CompositorElementId GetMaskIsolationId() const { return mask_isolation_id_; }
  CompositorElementId GetMaskEffectId() const { return mask_effect_id_; }

 private:
  // ContentLayerClient implementation.
  gfx::Rect PaintableRegion() final { return gfx::Rect(layer_->bounds()); }
  bool FillsBoundsCompletely() const final { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const final { return 0; }

  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) final {
    auto cc_list = base::MakeRefCounted<cc::DisplayItemList>(
        cc::DisplayItemList::kTopLevelDisplayItemList);
    PaintFlags flags;
    flags.setAntiAlias(true);
    cc_list->StartPaint();
    if (!path_) {
      cc_list->push<cc::DrawRRectOp>(local_rrect_, flags);
    } else {
      cc_list->push<cc::SaveOp>();
      cc_list->push<cc::TranslateOp>(-layer_origin_.x(), -layer_origin_.x());
      cc_list->push<cc::ClipPathOp>(path_->GetSkPath(), SkClipOp::kIntersect,
                                    true);
      SkRRect rrect = local_rrect_;
      rrect.offset(layer_origin_.x(), layer_origin_.y());
      cc_list->push<cc::DrawRRectOp>(rrect, flags);
      cc_list->push<cc::RestoreOp>();
    }
    cc_list->EndPaintOfUnpaired(gfx::Rect(layer_->bounds()));
    cc_list->Finalize();
    return cc_list;
  }

 private:
  scoped_refptr<cc::PictureLayer> layer_;
  gfx::Vector2dF layer_origin_;
  SkRRect local_rrect_ = SkRRect::MakeEmpty();
  scoped_refptr<const RefCountedPath> path_;
  CompositorElementId mask_isolation_id_;
  CompositorElementId mask_effect_id_;
};

// TODO(pdr): There is no test that synthetic clip layers are re-used.
cc::Layer* PaintArtifactCompositor::CreateOrReuseSynthesizedClipLayer(
    const ClipPaintPropertyNode* node,
    CompositorElementId& mask_isolation_id,
    CompositorElementId& mask_effect_id) {
  auto entry = std::find_if(
      synthesized_clip_cache_.begin(), synthesized_clip_cache_.end(),
      [node](const auto& entry) { return entry.key == node && !entry.in_use; });
  if (entry == synthesized_clip_cache_.end()) {
    auto clip = std::make_unique<SynthesizedClip>();
    clip->GetLayer()->SetLayerTreeHost(root_layer_->layer_tree_host());
    entry = synthesized_clip_cache_.insert(
        entry, SynthesizedClipEntry{node, std::move(clip), false});
  }

  entry->in_use = true;
  SynthesizedClip& synthesized_clip = *entry->synthesized_clip;
  synthesized_clip.Update(node->ClipRect(), node->ClipPath());
  mask_isolation_id = synthesized_clip.GetMaskIsolationId();
  mask_effect_id = synthesized_clip.GetMaskEffectId();
  return synthesized_clip.GetLayer();
}

void PaintArtifactCompositor::Update(
    scoped_refptr<const PaintArtifact> paint_artifact,
    CompositorElementIdSet& composited_element_ids,
    TransformPaintPropertyNode* viewport_scale_node) {
  DCHECK(root_layer_);

  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  cc::LayerTreeHost* host = root_layer_->layer_tree_host();
  if (!host)
    return;

  // When using BlinkGenPropertyTrees, the compositor accepts a list of layers
  // and property trees instead of building property trees. This DCHECK ensures
  // we have not forgotten to set |use_layer_lists|.
  DCHECK(!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() ||
         host->GetSettings().use_layer_lists);

  if (extra_data_for_testing_enabled_)
    extra_data_for_testing_.reset(new ExtraDataForTesting);

  root_layer_->set_property_tree_sequence_number(
      g_s_property_tree_sequence_number);

  LayerListBuilder layer_list_builder;

  PropertyTreeManager property_tree_manager(
      *this, *host->property_trees(), root_layer_.get(), &layer_list_builder);
  Vector<PendingLayer, 0> pending_layers;
  CollectPendingLayers(*paint_artifact, pending_layers);

  // The page scale layer would create this below but we need to use the
  // special EnsureCompositorPageScaleTransformNode method since the transform
  // created in a different way so we call it here.
  if (viewport_scale_node) {
    property_tree_manager.EnsureCompositorPageScaleTransformNode(
        viewport_scale_node);
  }

  Vector<std::unique_ptr<ContentLayerClientImpl>> new_content_layer_clients;
  new_content_layer_clients.ReserveCapacity(pending_layers.size());
  Vector<scoped_refptr<cc::Layer>> new_scroll_hit_test_layers;

  for (auto& entry : synthesized_clip_cache_)
    entry.in_use = false;

  for (auto& pending_layer : pending_layers) {
    const auto& property_state = pending_layer.property_tree_state;
    const auto* transform = property_state.Transform();
    const auto* clip = property_state.Clip();

    if (clip->LocalTransformSpace() == transform) {
      // Limit layer bounds to hide the areas that will be never visible because
      // of the clip.
      pending_layer.bounds.Intersect(clip->ClipRect().Rect());
    } else if (const auto* scroll = transform->ScrollNode()) {
      // Limit layer bounds to the scroll range to hide the areas that will
      // never be scrolled into the visible area.
      pending_layer.bounds.Intersect(
          FloatRect(FloatPoint(), FloatSize(scroll->ContentsSize())));
    }

    gfx::Vector2dF layer_offset;
    scoped_refptr<cc::Layer> layer = CompositedLayerForPendingLayer(
        paint_artifact, pending_layer, layer_offset, new_content_layer_clients,
        new_scroll_hit_test_layers);
    // Get the compositor element id for the layer. Scrollable layers are only
    // associated with scroll element ids which are set in
    // ScrollHitTestLayerForPendingLayer.
    CompositorElementId element_id =
        layer->scrollable()
            ? layer->element_id()
            : property_state.GetCompositorElementId(composited_element_ids);
    // TODO(wkorman): Cease setting element id on layer once
    // animation subsystem no longer requires element id to layer
    // map. http://crbug.com/709137
    // TODO(pdr): Element ids will still need to be set on scroll layers.
    layer->SetElementId(element_id);
    if (element_id)
      composited_element_ids.insert(element_id);
    layer->SetLayerTreeHost(root_layer_->layer_tree_host());

    int transform_id =
        property_tree_manager.EnsureCompositorTransformNode(transform);
    int clip_id = property_tree_manager.EnsureCompositorClipNode(clip);
    int effect_id = property_tree_manager.SwitchToEffectNodeWithSynthesizedClip(
        *property_state.Effect(), *clip);
    // The compositor scroll node is not directly stored in the property tree
    // state but can be created via the scroll offset translation node.
    const auto& scroll_translation =
        ScrollTranslationForPendingLayer(*paint_artifact, pending_layer);
    int scroll_id =
        property_tree_manager.EnsureCompositorScrollNode(&scroll_translation);

    layer->SetOffsetToTransformParent(layer_offset);

    layer_list_builder.Add(layer);

    layer->set_property_tree_sequence_number(
        root_layer_->property_tree_sequence_number());
    layer->SetTransformTreeIndex(transform_id);
    layer->SetScrollTreeIndex(scroll_id);
    layer->SetClipTreeIndex(clip_id);
    layer->SetEffectTreeIndex(effect_id);
    bool backface_hidden =
        pending_layer.property_tree_state.Transform()->IsBackfaceHidden();
    layer->SetDoubleSided(!backface_hidden);
    // TODO(wangxianzhu): cc::PropertyTreeBuilder has a more sophisticated
    // condition for this. Do we need to do the same here?
    layer->SetShouldCheckBackfaceVisibility(backface_hidden);
  }
  property_tree_manager.Finalize();
  content_layer_clients_.swap(new_content_layer_clients);
  scroll_hit_test_layers_.swap(new_scroll_hit_test_layers);

  synthesized_clip_cache_.erase(
      std::remove_if(synthesized_clip_cache_.begin(),
                     synthesized_clip_cache_.end(),
                     [](const auto& entry) { return !entry.in_use; }),
      synthesized_clip_cache_.end());
  if (extra_data_for_testing_enabled_) {
    for (const auto& entry : synthesized_clip_cache_) {
      extra_data_for_testing_->synthesized_clip_layers.push_back(
          entry.synthesized_clip->GetLayer());
    }
  }

  root_layer_->SetChildLayerList(layer_list_builder.Finalize());

  // Mark the property trees as having been rebuilt.
  host->property_trees()->sequence_number = g_s_property_tree_sequence_number;
  host->property_trees()->needs_rebuild = false;
  host->property_trees()->ResetCachedData();

  g_s_property_tree_sequence_number++;

#if DCHECK_IS_ON()
  if (VLOG_IS_ON(2)) {
    static String s_previous_output;
    LayerTreeFlags flags = VLOG_IS_ON(3) ? 0xffffffff : 0;
    String new_output = LayersAsJSON(flags)->ToPrettyJSONString();
    if (new_output != s_previous_output) {
      LOG(ERROR) << "PaintArtifactCompositor::Update() done\n"
                 << "Composited layers:\n"
                 << new_output.Utf8().data();
      s_previous_output = new_output;
    }
  }
#endif
}

void LayerListBuilder::Add(scoped_refptr<cc::Layer> layer) {
  DCHECK(list_valid_);
  list_.push_back(layer);
}

cc::LayerList LayerListBuilder::Finalize() {
  DCHECK(list_valid_);
  list_valid_ = false;
  return std::move(list_);
}

cc::Layer*
PaintArtifactCompositor::ExtraDataForTesting::ScrollHitTestWebLayerAt(
    unsigned index) {
  return scroll_hit_test_layers[index].get();
}

#if DCHECK_IS_ON()
void PaintArtifactCompositor::ShowDebugData() {
  LOG(ERROR) << LayersAsJSON(kLayerTreeIncludesDebugInfo |
                             kLayerTreeIncludesPaintInvalidations)
                    ->ToPrettyJSONString()
                    .Utf8()
                    .data();
}
#endif

}  // namespace blink

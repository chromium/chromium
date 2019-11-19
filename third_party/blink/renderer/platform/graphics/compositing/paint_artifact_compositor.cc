// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

#include <memory>
#include <utility>

#include "cc/layers/layer.h"
#include "cc/paint/display_item_list.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host.h"
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
    : scroll_callbacks_(std::move(scroll_callbacks)) {
  root_layer_ = cc::Layer::Create();
}

PaintArtifactCompositor::~PaintArtifactCompositor() {}

void PaintArtifactCompositor::EnableExtraDataForTesting() {
  if (extra_data_for_testing_enabled_)
    return;
  extra_data_for_testing_enabled_ = true;
  extra_data_for_testing_ = std::make_unique<ExtraDataForTesting>();
  // Ensure |extra_data_for_testing_| is populated.
  SetNeedsUpdate();
}

void PaintArtifactCompositor::SetTracksRasterInvalidations(bool should_track) {
  tracks_raster_invalidations_ = should_track;
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

// Get a JSON representation of what layers exist for this PAC.  Note that
// |paint_artifact| is only needed for pre-CAP mode.
std::unique_ptr<JSONObject> PaintArtifactCompositor::GetLayersAsJSON(
    LayerTreeFlags flags,
    const PaintArtifact* paint_artifact) const {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
         paint_artifact);
  LayersAsJSON layers_as_json(flags);
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (!tracks_raster_invalidations_)
      flags &= ~kLayerTreeIncludesPaintInvalidations;
    for (const auto& layer : root_layer_->children()) {
      if (!layer->DrawsContent() && !(flags & kLayerTreeIncludesRootLayer))
        continue;
      const LayerAsJSONClient* json_client = nullptr;
      const TransformPaintPropertyNode* transform = nullptr;
      for (const auto& client : content_layer_clients_) {
        if (&client->Layer() == layer.get()) {
          json_client = client.get();
          transform = &client->State().Transform();
          break;
        }
      }
      if (!transform) {
        for (const auto& pending_layer : pending_layers_) {
          if (pending_layer.property_tree_state.Transform().CcNodeId(
                  layer->property_tree_sequence_number()) ==
              layer->transform_tree_index()) {
            transform = &pending_layer.property_tree_state.Transform();
            break;
          }
        }
      }
      DCHECK(transform);
      layers_as_json.AddLayer(*layer,
                              FloatPoint(layer->offset_to_transform_parent()),
                              *transform, json_client);
    }
  } else {
    for (const auto& paint_chunk : paint_artifact->PaintChunks()) {
      const auto& display_item =
          paint_artifact->GetDisplayItemList()[paint_chunk.begin_index];
      DCHECK(display_item.IsForeignLayer());
      const auto& foreign_layer_display_item =
          static_cast<const ForeignLayerDisplayItem&>(display_item);
      cc::Layer* layer = foreign_layer_display_item.GetLayer();
      if ((layer->DrawsContent()) || (flags & kLayerTreeIncludesRootLayer)) {
        layers_as_json.AddLayer(
            *layer, foreign_layer_display_item.Offset(),
            paint_chunk.properties.Transform(),
            foreign_layer_display_item.GetLayerAsJSONClient());
      }
    }
  }
  return layers_as_json.Finalize();
}

static scoped_refptr<cc::Layer> ForeignLayerForPaintChunk(
    const PaintArtifact& paint_artifact,
    const PaintChunk& paint_chunk,
    const FloatPoint& pending_layer_offset) {
  if (paint_chunk.size() != 1)
    return nullptr;

  const auto& display_item =
      paint_artifact.GetDisplayItemList()[paint_chunk.begin_index];
  if (!display_item.IsForeignLayer())
    return nullptr;

  // When a foreign layer's offset_to_transform_parent() changes, we don't
  // call PaintArtifaceCompositor::SetNeedsUpdate() because the update won't
  // change anything but cause unnecessary commit. Though
  // UpdateTouchActionRects() depends on offset_to_transform_parent(), a
  // foreign layer chunk doesn't have hit_test_data.
  DCHECK(!paint_chunk.hit_test_data);

  const auto& foreign_layer_display_item =
      static_cast<const ForeignLayerDisplayItem&>(display_item);
  auto* layer = foreign_layer_display_item.GetLayer();
  layer->SetOffsetToTransformParent(gfx::Vector2dF(
      foreign_layer_display_item.Offset() + pending_layer_offset));
  return layer;
}

const TransformPaintPropertyNode&
PaintArtifactCompositor::ScrollOffsetTranslationForLayer(
    const PaintArtifact& paint_artifact,
    const PendingLayer& pending_layer) {
  if (const auto* scroll_hit_test =
          ScrollHitTestForLayer(paint_artifact, pending_layer)) {
    if (scroll_hit_test->scroll_offset)
      return *scroll_hit_test->scroll_offset;
  }
  const auto& transform = pending_layer.property_tree_state.Transform();
  // TODO(pdr): This could be a performance issue because it crawls up the
  // transform tree for each pending layer. If this is on profiles, we should
  // cache a lookup of transform node to scroll translation transform node.
  return transform.NearestScrollTranslationNode();
}

const HitTestData::ScrollHitTest*
PaintArtifactCompositor::ScrollHitTestForLayer(
    const PaintArtifact& paint_artifact,
    const PendingLayer& pending_layer) {
  auto paint_chunks =
      paint_artifact.GetPaintChunkSubset(pending_layer.paint_chunk_indices);
  DCHECK(paint_chunks.size());
  const auto& first_paint_chunk = paint_chunks[0];
  if (first_paint_chunk.size() != 1)
    return nullptr;

  const HitTestData* hit_test_data = first_paint_chunk.hit_test_data.get();
  if (!hit_test_data)
    return nullptr;

  return base::OptionalOrNullptr(hit_test_data->scroll_hit_test);
}

scoped_refptr<cc::Layer>
PaintArtifactCompositor::ScrollHitTestLayerForPendingLayer(
    const PaintArtifact& paint_artifact,
    const PendingLayer& pending_layer) {
  const auto* scroll_hit_test =
      ScrollHitTestForLayer(paint_artifact, pending_layer);
  if (!scroll_hit_test)
    return nullptr;
  const auto* scroll_offset = scroll_hit_test->scroll_offset;
  if (!scroll_offset)
    return nullptr;

  // We shouldn't decomposite scroll transform nodes.
  DCHECK_EQ(FloatPoint(), pending_layer.offset_of_decomposited_transforms);

  const auto& scroll_node = *scroll_offset->ScrollNode();
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

  scroll_layer->SetOffsetToTransformParent(
      gfx::Vector2dF(FloatPoint(scroll_node.ContainerRect().Location())));
  // TODO(pdr): The scroll layer's bounds are currently set to the clipped
  // container bounds but this does not include the border. We may want to
  // change this behavior to make non-composited and composited hit testing
  // match (see: crbug.com/753124). To do this, use
  // |scroll_hit_test->scroll_container_bounds|.
  auto bounds = scroll_node.ContainerRect().Size();
  // Mark the layer as scrollable.
  // TODO(pdr): When CAP launches this parameter for bounds will not be
  // needed.
  scroll_layer->SetScrollable(static_cast<gfx::Size>(bounds));
  // Set the layer's bounds equal to the container because the scroll layer
  // does not scroll.
  scroll_layer->SetBounds(static_cast<gfx::Size>(bounds));
  return scroll_layer;
}

scoped_refptr<cc::Layer> PaintArtifactCompositor::ScrollbarLayerForPendingLayer(
    const PaintArtifact& paint_artifact,
    const PendingLayer& pending_layer) {
  auto paint_chunks =
      paint_artifact.GetPaintChunkSubset(pending_layer.paint_chunk_indices);
  if (paint_chunks.size() != 1)
    return nullptr;
  const auto& paint_chunk = paint_chunks[0];
  if (paint_chunk.size() != 1)
    return nullptr;

  const auto& item =
      paint_artifact.GetDisplayItemList()[paint_chunk.begin_index];
  if (!item.IsScrollbar())
    return nullptr;

  const auto& scrollbar_item = static_cast<const ScrollbarDisplayItem&>(item);
  scoped_refptr<cc::Layer> scrollbar_layer;
  for (auto& existing_layer : scrollbar_layers_) {
    if (existing_layer->element_id() == scrollbar_item.ElementId())
      scrollbar_layer = existing_layer;
  }
  if (scrollbar_layer) {
    cc::Scrollbar* scrollbar = scrollbar_item.GetScrollbar();
    if (scrollbar->NeedsRepaintPart(cc::THUMB) ||
        scrollbar->NeedsRepaintPart(cc::TRACK_BUTTONS_TICKMARKS))
      scrollbar_layer->SetNeedsDisplay();
  } else {
    scrollbar_layer = scrollbar_item.CreateLayer();
  }

  // We should never decomposite scroll translations, so we don't need to adjust
  // the layer's offset for decomposited transforms.
  DCHECK_EQ(FloatPoint(), pending_layer.offset_of_decomposited_transforms);
  const IntRect& rect = scrollbar_item.GetRect();
  scrollbar_layer->SetOffsetToTransformParent(
      gfx::Vector2dF(FloatPoint(rect.Location())));
  scrollbar_layer->SetBounds(gfx::Size(rect.Size()));
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
    scoped_refptr<const PaintArtifact> paint_artifact,
    const PendingLayer& pending_layer,
    Vector<std::unique_ptr<ContentLayerClientImpl>>& new_content_layer_clients,
    Vector<scoped_refptr<cc::Layer>>& new_scroll_hit_test_layers,
    Vector<scoped_refptr<cc::Layer>>& new_scrollbar_layers) {
  auto paint_chunks =
      paint_artifact->GetPaintChunkSubset(pending_layer.paint_chunk_indices);
  DCHECK(paint_chunks.size());
  const PaintChunk& first_paint_chunk = paint_chunks[0];
  DCHECK(first_paint_chunk.size());

  // If the paint chunk is a foreign layer, just return that layer.
  if (scoped_refptr<cc::Layer> foreign_layer = ForeignLayerForPaintChunk(
          *paint_artifact, first_paint_chunk,
          pending_layer.offset_of_decomposited_transforms)) {
    DCHECK_EQ(paint_chunks.size(), 1u);
    if (extra_data_for_testing_enabled_)
      extra_data_for_testing_->content_layers.push_back(foreign_layer);
    return foreign_layer;
  }

  // If the paint chunk is a scroll hit test layer, lookup/create the layer.
  if (scoped_refptr<cc::Layer> scroll_layer =
          ScrollHitTestLayerForPendingLayer(*paint_artifact, pending_layer)) {
    new_scroll_hit_test_layers.push_back(scroll_layer);
    if (extra_data_for_testing_enabled_)
      extra_data_for_testing_->scroll_hit_test_layers.push_back(scroll_layer);
    return scroll_layer;
  }

  if (scoped_refptr<cc::Layer> scrollbar_layer =
          ScrollbarLayerForPendingLayer(*paint_artifact, pending_layer)) {
    new_scrollbar_layers.push_back(scrollbar_layer);
    if (extra_data_for_testing_enabled_)
      extra_data_for_testing_->scrollbar_layers.push_back(scrollbar_layer);
    return scrollbar_layer;
  }

  // The common case: create or reuse a PictureLayer for painted content.
  std::unique_ptr<ContentLayerClientImpl> content_layer_client =
      ClientForPaintChunk(first_paint_chunk);

  IntRect cc_combined_bounds = EnclosingIntRect(pending_layer.bounds);
  auto cc_layer = content_layer_client->UpdateCcPictureLayer(
      paint_artifact, paint_chunks, cc_combined_bounds,
      pending_layer.property_tree_state);
  if (cc_combined_bounds.IsEmpty())
    cc_layer->SetIsDrawable(false);

  new_content_layer_clients.push_back(std::move(content_layer_client));
  if (extra_data_for_testing_enabled_)
    extra_data_for_testing_->content_layers.push_back(cc_layer);

  // Set properties that foreign layers would normally control for themselves
  // here to avoid changing foreign layers. This includes things set by
  // GraphicsLayer on the ContentsLayer() or by video clients etc.
  cc_layer->SetContentsOpaque(pending_layer.rect_known_to_be_opaque.Contains(
      FloatRect(cc_combined_bounds)));

  return cc_layer;
}

void PaintArtifactCompositor::UpdateTouchActionRects(
    cc::Layer* layer,
    // TODO(wangxianzhu): Remove this parameter and use
    // layer->offset_to_transform_parent() after we fully launch
    // BlinkGenPropertyTrees.
    const gfx::Vector2dF& layer_offset,
    const PropertyTreeState& layer_state,
    const PaintChunkSubset& paint_chunks) {
  cc::TouchActionRegion touch_action_in_layer_space;
  for (const auto& chunk : paint_chunks) {
    const auto* hit_test_data = chunk.hit_test_data.get();
    if (!hit_test_data || hit_test_data->touch_action_rects.IsEmpty())
      continue;

    const auto& chunk_state = chunk.properties.GetPropertyTreeState();
    for (const auto& touch_action_rect : hit_test_data->touch_action_rects) {
      auto rect =
          FloatClipRect(FloatRect(PixelSnappedIntRect(touch_action_rect.rect)));
      if (!GeometryMapper::LocalToAncestorVisualRect(chunk_state, layer_state,
                                                     rect)) {
        continue;
      }
      rect.MoveBy(FloatPoint(-layer_offset.x(), -layer_offset.y()));
      touch_action_in_layer_space.Union(
          touch_action_rect.allowed_touch_action,
          (gfx::Rect)EnclosingIntRect(rect.Rect()));
    }
  }
  layer->SetTouchActionRegion(std::move(touch_action_in_layer_space));
}

void PaintArtifactCompositor::UpdateNonFastScrollableRegions(
    cc::Layer* layer,
    // TODO(wangxianzhu): Remove this parameter and use
    // layer->offset_to_transform_parent() after we fully launch
    // BlinkGenPropertyTrees.
    const gfx::Vector2dF& layer_offset,
    const PropertyTreeState& layer_state,
    const PaintChunkSubset& paint_chunks) {
  cc::Region non_fast_scrollable_regions_in_layer_space;
  for (const auto& chunk : paint_chunks) {
    // Add any non-fast scrollable hit test data from the paint chunk.
    const auto* hit_test_data = chunk.hit_test_data.get();
    if (!hit_test_data || !hit_test_data->scroll_hit_test)
      continue;

    // Skip the scroll hit test rect if it is for scrolling this cc::Layer.
    // This is only needed for CompositeAfterPaint because
    // pre-CompositeAfterPaint does not paint scroll hit test data for
    // composited scrollers.
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      if (layer->scrollable()) {
        const auto* scroll_offset =
            hit_test_data->scroll_hit_test->scroll_offset;
        if (scroll_offset) {
          const auto& scroll_node = *scroll_offset->ScrollNode();
          auto scroll_element_id = scroll_node.GetCompositorElementId();
          if (layer->element_id() == scroll_element_id)
            continue;
        }
      }
    }

    const auto& bounds =
        hit_test_data->scroll_hit_test->scroll_container_bounds;
    auto rect = FloatClipRect(FloatRect(bounds));
    const auto& chunk_state = chunk.properties.GetPropertyTreeState();
    if (!GeometryMapper::LocalToAncestorVisualRect(chunk_state, layer_state,
                                                   rect)) {
      continue;
    }
    rect.MoveBy(FloatPoint(-layer_offset.x(), -layer_offset.y()));
    non_fast_scrollable_regions_in_layer_space.Union(
        (gfx::Rect)EnclosingIntRect(rect.Rect()));
  }
  layer->SetNonFastScrollableRegion(non_fast_scrollable_regions_in_layer_space);
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

bool PaintArtifactCompositor::PropertyTreeStateChanged(
    const PropertyTreeState& state) const {
  const auto& root = PropertyTreeState::Root();
  auto change = PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  return state.Transform().Changed(change, root.Transform()) ||
         state.Clip().Changed(change, root, &state.Transform()) ||
         state.Effect().Changed(change, root, &state.Transform());
}

PaintArtifactCompositor::PendingLayer::PendingLayer(
    const PaintChunk& first_paint_chunk,
    wtf_size_t chunk_index,
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
    const PendingLayer& guest,
    const PropertyTreeState& guest_state) const {
  if (requires_own_layer || guest.requires_own_layer)
    return false;
  if (&property_tree_state.Effect().Unalias() !=
      &guest_state.Effect().Unalias()) {
    return false;
  }
  return CanUpcastTo(guest_state, property_tree_state);
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

const PaintChunk& PaintArtifactCompositor::PendingLayer::FirstPaintChunk(
    const PaintArtifact& paint_artifact) const {
  return paint_artifact.PaintChunks()[paint_chunk_indices[0]];
}

static bool IsNonCompositingAncestorOf(
    const TransformPaintPropertyNode& unaliased_ancestor,
    const TransformPaintPropertyNode& node) {
  for (const auto* n = &node; n != &unaliased_ancestor;
       n = SafeUnalias(n->Parent())) {
    if (!n || n->HasDirectCompositingReasons())
      return false;
  }
  return true;
}

// Determines whether drawings based on the 'guest' state can be painted into
// a layer with the 'home' state. A number of criteria need to be met:
// 1. The guest effect must be a descendant of the home effect. However this
//    check is enforced by the layerization recursion. Here we assume the
//    guest has already been upcasted to the same effect.
// 2. The guest transform and the home transform have compatible backface
//    visibility.
// 3. The guest clip must be a descendant of the home clip.
// 4. The local space of each clip and effect node on the ancestor chain must
//    be within compositing boundary of the home transform space.
// 5. The guest transform space must be within compositing boundary of the
// home
//    transform space.
static bool CanUpcastTo(const PropertyTreeState& guest,
                        const PropertyTreeState& home) {
  DCHECK_EQ(&home.Effect().Unalias(), &guest.Effect().Unalias());

  if (home.Transform().IsBackfaceHidden() !=
      guest.Transform().IsBackfaceHidden())
    return false;

  const auto& home_clip = home.Clip().Unalias();
  for (const auto* current_clip = &guest.Clip().Unalias();
       current_clip != &home_clip;
       current_clip = SafeUnalias(current_clip->Parent())) {
    // If we had direct compositing reasons on a clip node, we would want to
    // return false here.
    if (!current_clip)
      return false;
    if (!IsNonCompositingAncestorOf(
            home.Transform().Unalias(),
            current_clip->LocalTransformSpace().Unalias())) {
      return false;
    }
  }

  return IsNonCompositingAncestorOf(home.Transform().Unalias(),
                                    guest.Transform().Unalias());
}

// Returns nullptr if 'ancestor' is not a strict ancestor of 'node'.
// Otherwise, return the child of 'ancestor' that is an ancestor of 'node' or
// 'node' itself.
static const EffectPaintPropertyNode* StrictUnaliasedChildOfAlongPath(
    const EffectPaintPropertyNode& unaliased_ancestor,
    const EffectPaintPropertyNode& node) {
  const auto* n = &node.Unalias();
  while (n) {
    const auto* parent = SafeUnalias(n->Parent());
    if (parent == &unaliased_ancestor)
      return n;
    n = parent;
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

bool PaintArtifactCompositor::DecompositeEffect(
    const EffectPaintPropertyNode& unaliased_parent_effect,
    size_t first_layer_in_parent_group_index,
    const EffectPaintPropertyNode& unaliased_effect,
    size_t layer_index) {
  // The layer must be the last layer in pending_layers_.
  DCHECK_EQ(layer_index, pending_layers_.size() - 1);

  // If the effect associated with the layer is deeper than than the effect
  // we are attempting to decomposite, than implies some previous decision
  // did not allow to decomposite intermediate effects.
  PendingLayer& layer = pending_layers_[layer_index];
  if (&layer.property_tree_state.Effect().Unalias() != &unaliased_effect)
    return false;
  if (layer.requires_own_layer)
    return false;
  if (unaliased_effect.HasDirectCompositingReasons())
    return false;

  PropertyTreeState group_state(unaliased_effect.LocalTransformSpace(),
                                unaliased_effect.OutputClip()
                                    ? *unaliased_effect.OutputClip()
                                    : layer.property_tree_state.Clip(),
                                unaliased_effect);
  if (!CanUpcastTo(layer.property_tree_state, group_state))
    return false;

  PropertyTreeState upcast_state = group_state;
  upcast_state.SetEffect(unaliased_parent_effect);

  // Exotic blending layer can be decomposited only if its parent group
  // (which defines the scope of the blending) has only one layer before it,
  // and it can be merged into that layer.
  if (unaliased_effect.BlendMode() != SkBlendMode::kSrcOver) {
    if (layer_index - 1 != first_layer_in_parent_group_index)
      return false;
    if (!pending_layers_[first_layer_in_parent_group_index].CanMerge(
            layer, upcast_state))
      return false;
  }

  layer.Upcast(upcast_state);
  return true;
}

static bool EffectGroupContainsChunk(
    const EffectPaintPropertyNode& unaliased_group_effect,
    const PaintChunk& chunk) {
  const auto& effect = chunk.properties.Effect().Unalias();
  return &effect == &unaliased_group_effect ||
         StrictUnaliasedChildOfAlongPath(unaliased_group_effect, effect);
}

static bool SkipGroupIfEffectivelyInvisible(
    const PaintArtifact& paint_artifact,
    const EffectPaintPropertyNode& unaliased_group,
    Vector<PaintChunk>::const_iterator& chunk_it) {
  // In pre-CompositeAfterPaint, existence of composited layers is decided
  // during compositing update before paint. Each chunk contains a foreign
  // layer corresponding a composited layer. We should not skip any of them to
  // ensure correct composited hit testing and animation.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return false;

  // The lower bound of visibility is considered to be 0.0004f < 1/2048. With
  // 10-bit color channels (only available on the newest Macs as of 2016;
  // otherwise it's 8-bit), we see that an alpha of 1/2048 or less leads to a
  // color output of less than 0.5 in all channels, hence not visible.
  static const float kMinimumVisibleOpacity = 0.0004f;
  if (unaliased_group.Opacity() >= kMinimumVisibleOpacity ||
      // TODO(crbug.com/937573): We should disable the optimization for all
      // cases that the invisible group will be composited, to ensure correct
      // composited hit testing and animation. Checking the effect node's
      // HasDirectCompositingReasons() is not enough.
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

static bool IsCompositedScrollbar(const DisplayItem& item) {
  if (!item.IsScrollbar())
    return false;
  const auto* scroll_translation =
      static_cast<const ScrollbarDisplayItem&>(item).ScrollTranslation();
  return scroll_translation &&
         scroll_translation->HasDirectCompositingReasons();
}

void PaintArtifactCompositor::LayerizeGroup(
    const PaintArtifact& paint_artifact,
    const Settings& settings,
    const EffectPaintPropertyNode& current_group,
    Vector<PaintChunk>::const_iterator& chunk_it) {
  // Skip paint chunks that are effectively invisible due to opacity and don't
  // have a direct compositing reason.
  const auto& unaliased_group = current_group.Unalias();
  if (SkipGroupIfEffectivelyInvisible(paint_artifact, unaliased_group,
                                      chunk_it))
    return;

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
  // costs O(qd), where d came from "canUpcastTo" and geometry mapping.
  // Subtotal: O(pqd)
  // For group entering and exiting, it could cost O(d) for each group, for
  // searching the shallowest subgroup (strictChildOfAlongPath), thus O(d^2)
  // in total.
  // Also when exiting group, the group may be decomposited and squashed to a
  // previous layer. Again finding the host costs O(qd). Merging would cost
  // O(p) due to copying the chunk list. Subtotal: O((qd + p)d) = O(qd^2 + pd)
  // Assuming p > d, the total complexity would be O(pqd + qd^2 + pd) = O(pqd)
  while (chunk_it != paint_artifact.PaintChunks().end()) {
    // Look at the effect node of the next chunk. There are 3 possible cases:
    // A. The next chunk belongs to the current group but no subgroup.
    // B. The next chunk does not belong to the current group.
    // C. The next chunk belongs to some subgroup of the current group.
    const auto& chunk_effect = chunk_it->properties.Effect().Unalias();
    if (&chunk_effect == &unaliased_group) {
      // Case A: The next chunk belongs to the current group but no subgroup.
      const auto& last_display_item =
          paint_artifact.GetDisplayItemList()[chunk_it->begin_index];
      bool item_for_scrolling = last_display_item.IsScrollHitTest() &&
                                !last_display_item.IsResizerScrollHitTest() &&
                                !last_display_item.IsPluginScrollHitTest();
      bool requires_own_layer = last_display_item.IsForeignLayer() ||
                                // TODO(pdr): This should require a direct
                                // compositing reason.
                                item_for_scrolling ||
                                IsCompositedScrollbar(last_display_item);
      pending_layers_.emplace_back(
          *chunk_it, chunk_it - paint_artifact.PaintChunks().begin(),
          requires_own_layer);
      chunk_it++;
      if (requires_own_layer)
        continue;
    } else {
      const EffectPaintPropertyNode* unaliased_subgroup =
          StrictUnaliasedChildOfAlongPath(unaliased_group, chunk_effect);
      // Case B: This means we need to close the current group without
      //         processing the next chunk.
      if (!unaliased_subgroup)
        break;
      // Case C: The following chunks belong to a subgroup. Process them by
      //         a recursion call.
      wtf_size_t first_layer_in_subgroup = pending_layers_.size();
      LayerizeGroup(paint_artifact, settings, *unaliased_subgroup, chunk_it);
      // The above LayerizeGroup generated new layers in pending_layers_
      // [first_layer_in_subgroup .. pending_layers.size() - 1]. If it
      // generated 2 or more layer that we already know can't be merged
      // together, we should not decomposite and try to merge any of them into
      // the previous layers.
      if (first_layer_in_subgroup != pending_layers_.size() - 1)
        continue;
      if (!DecompositeEffect(unaliased_group, first_layer_in_current_group,
                             *unaliased_subgroup, first_layer_in_subgroup))
        continue;
    }
    // At this point pending_layers_.back() is the either a layer from a
    // "decomposited" subgroup or a layer created from a chunk we just
    // processed. Now determine whether it could be merged into a previous
    // layer.
    const PendingLayer& new_layer = pending_layers_.back();
    DCHECK(!new_layer.requires_own_layer);
    DCHECK_EQ(&unaliased_group, &new_layer.property_tree_state.Effect());
    // This iterates pending_layers_[first_layer_in_current_group:-1] in
    // reverse.
    for (wtf_size_t candidate_index = pending_layers_.size() - 1;
         candidate_index-- > first_layer_in_current_group;) {
      PendingLayer& candidate_layer = pending_layers_[candidate_index];
      if (candidate_layer.CanMerge(new_layer, new_layer.property_tree_state)) {
        candidate_layer.Merge(new_layer);
        pending_layers_.pop_back();
        break;
      }
      if (MightOverlap(new_layer, candidate_layer))
        break;
    }
  }
}

void PaintArtifactCompositor::CollectPendingLayers(
    const PaintArtifact& paint_artifact,
    const Settings& settings) {
  Vector<PaintChunk>::const_iterator cursor =
      paint_artifact.PaintChunks().begin();
  // Shrink, but do not release the backing. Re-use it from the last frame.
  pending_layers_.Shrink(0);
  LayerizeGroup(paint_artifact, settings, EffectPaintPropertyNode::Root(),
                cursor);
  DCHECK_EQ(paint_artifact.PaintChunks().end(), cursor);
  pending_layers_.ShrinkToReasonableCapacity();
}

void SynthesizedClip::UpdateLayer(bool needs_layer,
                                  const FloatRoundedRect& rrect,
                                  scoped_refptr<const RefCountedPath> path) {
  if (!needs_layer) {
    layer_.reset();
    return;
  }
  if (!layer_) {
    layer_ = cc::PictureLayer::Create(this);
    layer_->SetIsDrawable(true);
  }

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

scoped_refptr<cc::DisplayItemList> SynthesizedClip::PaintContentsToDisplayList(
    PaintingControlSetting) {
  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>(
      cc::DisplayItemList::kTopLevelDisplayItemList);
  PaintFlags flags;
  flags.setAntiAlias(true);
  cc_list->StartPaint();
  if (!path_) {
    cc_list->push<cc::DrawRRectOp>(local_rrect_, flags);
  } else {
    cc_list->push<cc::SaveOp>();
    cc_list->push<cc::TranslateOp>(-layer_origin_.x(), -layer_origin_.y());
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

SynthesizedClip& PaintArtifactCompositor::CreateOrReuseSynthesizedClipLayer(
    const ClipPaintPropertyNode& node,
    bool needs_layer,
    CompositorElementId& mask_isolation_id,
    CompositorElementId& mask_effect_id) {
  auto* entry =
      std::find_if(synthesized_clip_cache_.begin(),
                   synthesized_clip_cache_.end(), [&node](const auto& entry) {
                     return entry.key == &node && !entry.in_use;
                   });
  if (entry == synthesized_clip_cache_.end()) {
    auto clip = std::make_unique<SynthesizedClip>();
    synthesized_clip_cache_.push_back(
        SynthesizedClipEntry{&node, std::move(clip), false});
    entry = synthesized_clip_cache_.end() - 1;
  }

  entry->in_use = true;
  SynthesizedClip& synthesized_clip = *entry->synthesized_clip;
  if (needs_layer) {
    synthesized_clip.UpdateLayer(needs_layer, node.ClipRect(), node.ClipPath());
    synthesized_clip.Layer()->SetLayerTreeHost(root_layer_->layer_tree_host());
  }
  mask_isolation_id = synthesized_clip.GetMaskIsolationId();
  mask_effect_id = synthesized_clip.GetMaskEffectId();
  return synthesized_clip;
}

static void UpdateCompositorViewportProperties(
    const PaintArtifactCompositor::ViewportProperties& properties,
    PropertyTreeManager& property_tree_manager,
    cc::LayerTreeHost* layer_tree_host) {
  cc::LayerTreeHost::ViewportPropertyIds ids;
  if (properties.overscroll_elasticity_transform) {
    ids.overscroll_elasticity_transform =
        property_tree_manager.EnsureCompositorTransformNode(
            *properties.overscroll_elasticity_transform);
  }
  if (properties.page_scale) {
    ids.page_scale_transform =
        property_tree_manager.EnsureCompositorPageScaleTransformNode(
            *properties.page_scale);
  }
  if (properties.inner_scroll_translation) {
    ids.inner_scroll = property_tree_manager.EnsureCompositorScrollNode(
        *properties.inner_scroll_translation);
    if (properties.outer_clip) {
      ids.outer_clip = property_tree_manager.EnsureCompositorClipNode(
          *properties.outer_clip);
    }
    if (properties.outer_scroll_translation) {
      ids.outer_scroll = property_tree_manager.EnsureCompositorScrollNode(
          *properties.outer_scroll_translation);
    }
  } else {
    // Outer viewport properties exist only if inner viewport property exists.
    DCHECK(!properties.outer_clip);
    DCHECK(!properties.outer_scroll_translation);
  }
  layer_tree_host->RegisterViewportPropertyIds(ids);
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
void PaintArtifactCompositor::DecompositeTransforms(
    const PaintArtifact& paint_artifact) {
  WTF::HashMap<const TransformPaintPropertyNode*, bool> can_be_decomposited;
  WTF::HashSet<const void*> clips_and_effects_seen;
  for (const auto& pending_layer : pending_layers_) {
    const auto& property_state = pending_layer.property_tree_state;

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
    for (const auto* node = &property_state.Transform().Unalias();
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
    for (const auto* node = &property_state.Clip().Unalias();
         !node->IsRoot() && !clips_and_effects_seen.Contains(node);
         node = &node->Parent()->Unalias()) {
      clips_and_effects_seen.insert(node);
      mark_not_decompositable(&node->LocalTransformSpace());
    }
    for (const auto* node = &property_state.Effect().Unalias();
         !node->IsRoot() && !clips_and_effects_seen.Contains(node);
         node = &node->Parent()->Unalias()) {
      clips_and_effects_seen.insert(node);
      mark_not_decompositable(&node->LocalTransformSpace());
    }

    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // The scroll translation node of a scroll hit test layer may not be
      // referenced by any pending layer's property tree state. Disallow
      // decomposition of it (and its ancestors).
      if (const auto* scroll_hit_test =
              ScrollHitTestForLayer(paint_artifact, pending_layer)) {
        if (const auto* scroll_offset = scroll_hit_test->scroll_offset)
          mark_not_decompositable(scroll_offset);
      }
    }
  }

  // Now, for any transform nodes that can be de-composited, re-map their
  // transform to point to the correct parent, and set the
  // offset_to_transform_parent.
  for (auto& pending_layer : pending_layers_) {
    const auto* transform =
        &pending_layer.property_tree_state.Transform().Unalias();
    while (!transform->IsRoot() && can_be_decomposited.at(transform)) {
      pending_layer.offset_of_decomposited_transforms +=
          transform->Translation2D();
      transform = &transform->Parent()->Unalias();
    }
    pending_layer.property_tree_state.SetTransform(*transform);
    // Move bounds into the new transform space.
    pending_layer.bounds.MoveBy(
        pending_layer.offset_of_decomposited_transforms);
    pending_layer.rect_known_to_be_opaque.MoveBy(
        pending_layer.offset_of_decomposited_transforms);
  }
}

void PaintArtifactCompositor::Update(
    scoped_refptr<const PaintArtifact> paint_artifact,
    const ViewportProperties& viewport_properties,
    const Settings& settings) {
  DCHECK(NeedsUpdate());
  DCHECK(root_layer_);
  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  cc::LayerTreeHost* host = root_layer_->layer_tree_host();
  if (!host)
    return;

  TRACE_EVENT0("blink", "PaintArtifactCompositor::Update");

  if (extra_data_for_testing_enabled_)
    extra_data_for_testing_.reset(new ExtraDataForTesting);

  host->property_trees()->scroll_tree.SetScrollCallbacks(scroll_callbacks_);
  root_layer_->set_property_tree_sequence_number(
      g_s_property_tree_sequence_number);

  LayerListBuilder layer_list_builder;
  PropertyTreeManager property_tree_manager(*this, *host->property_trees(),
                                            *root_layer_, layer_list_builder,
                                            g_s_property_tree_sequence_number);
  CollectPendingLayers(*paint_artifact, settings);

  UpdateCompositorViewportProperties(viewport_properties, property_tree_manager,
                                     host);

  Vector<std::unique_ptr<ContentLayerClientImpl>> new_content_layer_clients;
  new_content_layer_clients.ReserveCapacity(pending_layers_.size());
  Vector<scoped_refptr<cc::Layer>> new_scroll_hit_test_layers;
  Vector<scoped_refptr<cc::Layer>> new_scrollbar_layers;

  // Maps from cc effect id to blink effects. Containing only the effects
  // having composited layers.
  Vector<const EffectPaintPropertyNode*> blink_effects;

  for (auto& entry : synthesized_clip_cache_)
    entry.in_use = false;

  // See if we can de-composite any transforms.
  DecompositeTransforms(*paint_artifact);

  for (auto& pending_layer : pending_layers_) {
    const auto& property_state = pending_layer.property_tree_state;
    const auto& transform = property_state.Transform();
    const auto& clip = property_state.Clip();

    if (&clip.LocalTransformSpace() == &transform) {
      // Limit layer bounds to hide the areas that will be never visible
      // because of the clip.
      pending_layer.bounds.Intersect(clip.ClipRect().Rect());
    } else if (const auto* scroll = transform.ScrollNode()) {
      // Limit layer bounds to the scroll range to hide the areas that will
      // never be scrolled into the visible area.
      pending_layer.bounds.Intersect(FloatRect(
          IntRect(scroll->ContainerRect().Location(), scroll->ContentsSize())));
    }

    scoped_refptr<cc::Layer> layer = CompositedLayerForPendingLayer(
        paint_artifact, pending_layer, new_content_layer_clients,
        new_scroll_hit_test_layers, new_scrollbar_layers);

    // In Pre-CompositeAfterPaint, touch action rects and non-fast scrollable
    // regions are updated through ScrollingCoordinator.
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      auto paint_chunks = paint_artifact->GetPaintChunkSubset(
          pending_layer.paint_chunk_indices);
      UpdateTouchActionRects(layer.get(), layer->offset_to_transform_parent(),
                             property_state, paint_chunks);
      UpdateNonFastScrollableRegions(layer.get(),
                                     layer->offset_to_transform_parent(),
                                     property_state, paint_chunks);
    }

    layer->SetLayerTreeHost(root_layer_->layer_tree_host());

    int transform_id =
        property_tree_manager.EnsureCompositorTransformNode(transform);
    int clip_id = property_tree_manager.EnsureCompositorClipNode(clip);
    int effect_id = property_tree_manager.SwitchToEffectNodeWithSynthesizedClip(
        property_state.Effect(), clip, layer->DrawsContent());
    blink_effects.resize(effect_id + 1);
    blink_effects[effect_id] = &property_state.Effect();
    // The compositor scroll node is not directly stored in the property tree
    // state but can be created via the scroll offset translation node.
    const auto& scroll_translation =
        ScrollOffsetTranslationForLayer(*paint_artifact, pending_layer);
    int scroll_id =
        property_tree_manager.EnsureCompositorScrollNode(scroll_translation);

    layer_list_builder.Add(layer);

    layer->set_property_tree_sequence_number(
        root_layer_->property_tree_sequence_number());
    layer->SetTransformTreeIndex(transform_id);
    layer->SetScrollTreeIndex(scroll_id);
    layer->SetClipTreeIndex(clip_id);
    layer->SetEffectTreeIndex(effect_id);
    bool backface_hidden = property_state.Transform().IsBackfaceHidden();
    layer->SetDoubleSided(!backface_hidden);
    layer->SetShouldCheckBackfaceVisibility(backface_hidden);

    // If the property tree state has changed between the layer and the root,
    // we need to inform the compositor so damage can be calculated. Calling
    // |PropertyTreeStateChanged| for every pending layer is O(|property
    // nodes|^2) and could be optimized by caching the lookup of nodes known
    // to be changed/unchanged.
    if (layer->subtree_property_changed() ||
        PropertyTreeStateChanged(property_state)) {
      layer->SetSubtreePropertyChanged();
      root_layer_->SetNeedsCommit();
    }

    if (!layer_debug_info_enabled_) {
      layer->ClearDebugInfo();
    } else if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
               !layer->debug_info()) {
      // About the above condition: in pre-CompositeAfterPaint, debug info of
      // cc::Layers that are created by GraphicsLayers are updated in
      // LocalFrameView, so here we only update the other layers that don't
      // have debug info yet.
      RasterInvalidationTracking* tracking = nullptr;
      if (new_content_layer_clients.size() &&
          &new_content_layer_clients.back()->Layer() == layer) {
        tracking = new_content_layer_clients.back()
                       ->GetRasterInvalidator()
                       .GetTracking();
      }
      // TODO(wangxianzhu): pass real compositing reasons.
      UpdateLayerDebugInfo(layer.get(),
                           pending_layer.FirstPaintChunk(*paint_artifact).id,
                           CompositingReason::kNone, tracking);
    }
  }

  property_tree_manager.Finalize();
  content_layer_clients_.swap(new_content_layer_clients);
  scroll_hit_test_layers_.swap(new_scroll_hit_test_layers);
  scrollbar_layers_.swap(new_scrollbar_layers);

  auto pos = std::remove_if(synthesized_clip_cache_.begin(),
                            synthesized_clip_cache_.end(),
                            [](const auto& entry) { return !entry.in_use; }) -
             synthesized_clip_cache_.begin();
  synthesized_clip_cache_.EraseAt(pos, synthesized_clip_cache_.size() - pos);

  if (extra_data_for_testing_enabled_) {
    for (const auto& entry : synthesized_clip_cache_) {
      extra_data_for_testing_->synthesized_clip_layers.push_back(
          entry.synthesized_clip->Layer());
    }
  }

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
  needs_update_ = false;

  g_s_property_tree_sequence_number++;

#if DCHECK_IS_ON()
  if (VLOG_IS_ON(2)) {
    static String s_previous_output;
    LayerTreeFlags flags = VLOG_IS_ON(3) ? 0xffffffff : 0;
    String new_output =
        GetLayersAsJSON(flags, paint_artifact.get())->ToPrettyJSONString();
    if (new_output != s_previous_output) {
      LOG(ERROR) << "PaintArtifactCompositor::Update() done\n"
                 << "Composited layers:\n"
                 << new_output.Utf8();
      s_previous_output = new_output;
    }
  }
#endif
}

cc::PropertyTrees* PaintArtifactCompositor::GetPropertyTreesForDirectUpdate() {
  // Don't try to retrieve property trees if we need an update. The full
  // update will update all of the nodes, so a direct update doesn't need to
  // do anything.
  if (needs_update_)
    return nullptr;

  if (!root_layer_)
    return nullptr;

  auto* host = root_layer_->layer_tree_host();
  if (!host)
    return nullptr;
  return host->property_trees();
}

bool PaintArtifactCompositor::DirectlyUpdateCompositedOpacityValue(
    const EffectPaintPropertyNode& effect) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(effect.HasDirectCompositingReasons());
  if (auto* property_trees = GetPropertyTreesForDirectUpdate()) {
    return PropertyTreeManager::DirectlyUpdateCompositedOpacityValue(
        property_trees, *root_layer_->layer_tree_host(), effect);
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
  if (auto* property_trees = GetPropertyTreesForDirectUpdate()) {
    return PropertyTreeManager::DirectlyUpdateScrollOffsetTransform(
        property_trees, *root_layer_->layer_tree_host(), transform);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlyUpdateTransform(
    const TransformPaintPropertyNode& transform) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(transform.HasDirectCompositingReasons());
  if (auto* property_trees = GetPropertyTreesForDirectUpdate()) {
    return PropertyTreeManager::DirectlyUpdateTransform(
        property_trees, *root_layer_->layer_tree_host(), transform);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlyUpdatePageScaleTransform(
    const TransformPaintPropertyNode& transform) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(transform.HasDirectCompositingReasons());
  if (auto* property_trees = GetPropertyTreesForDirectUpdate()) {
    return PropertyTreeManager::DirectlyUpdatePageScaleTransform(
        property_trees, *root_layer_->layer_tree_host(), transform);
  }
  return false;
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
  if (static_cast<size_t>(effect.id) < blink_effects.size() &&
      blink_effects[effect.id] &&
      blink_effects[effect.id]->HasActiveOpacityAnimation())
    return cc::RenderSurfaceReason::kOpacityAnimation;
  // Applying a rounded corner clip to more than one layer descendant
  // with highest quality requires a render surface, due to the possibility
  // of antialiasing issues on the rounded corner edges.
  // is_fast_rounded_corner means to intentionally prefer faster compositing
  // and less memory over highest quality.
  if (!effect.rounded_corner_bounds.IsEmpty() && !effect.is_fast_rounded_corner)
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
  Vector<int> effect_layer_counts(effect_tree.size());
  // Initialize the vector to count directly controlled layers.
  for (const auto& layer : layers) {
    if (layer->DrawsContent())
      effect_layer_counts[layer->effect_tree_index()]++;
  }

  // In the effect tree, parent always has lower id than children, so the
  // following loop will check descendants before parents and accumulate
  // effect_layer_counts.
  for (auto id = effect_tree.size() - 1;
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

  if (enabled)
    root_layer_->EnsureDebugInfo().name = "root";
  else
    root_layer_->ClearDebugInfo();
}

void PaintArtifactCompositor::UpdateLayerDebugInfo(
    cc::Layer* layer,
    const PaintChunk::Id& id,
    CompositingReasons compositing_reasons,
    RasterInvalidationTracking* raster_invalidation_tracking) {
  cc::LayerDebugInfo& debug_info = layer->EnsureDebugInfo();

  debug_info.name = id.client.DebugName().Utf8();
  if (id.type == DisplayItem::kForeignLayerContentsWrapper) {
    // This is for backward compatibility in pre-CompositeAfterPaint mode.
    DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    debug_info.name = std::string("ContentsLayer for ") + debug_info.name;
  }

  debug_info.compositing_reasons =
      CompositingReason::Descriptions(compositing_reasons);
  debug_info.owner_node_id = id.client.OwnerNodeId();

  if (RasterInvalidationTracking::IsTracingRasterInvalidations() &&
      raster_invalidation_tracking) {
    raster_invalidation_tracking->AddToLayerDebugInfo(debug_info);
    raster_invalidation_tracking->ClearInvalidations();
  }
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
  LOG(ERROR) << GetLayersAsJSON(kLayerTreeIncludesDebugInfo |
                                kLayerTreeIncludesPaintInvalidations)
                    ->ToPrettyJSONString()
                    .Utf8();
}
#endif

}  // namespace blink

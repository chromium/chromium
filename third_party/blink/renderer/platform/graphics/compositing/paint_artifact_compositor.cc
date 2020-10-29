// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

#include <memory>
#include <utility>

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
    : scroll_callbacks_(std::move(scroll_callbacks)) {
  root_layer_ = cc::Layer::Create();
}

PaintArtifactCompositor::~PaintArtifactCompositor() {}

void PaintArtifactCompositor::SetTracksRasterInvalidations(bool should_track) {
  tracks_raster_invalidations_ = should_track;
  for (auto& client : content_layer_clients_)
    client->GetRasterInvalidator().SetTracksRasterInvalidations(should_track);
}

void PaintArtifactCompositor::WillBeRemovedFromFrame() {
  root_layer_->RemoveAllChildren();
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
        if (pending_layer.compositing_type ==
            PendingLayer::kPreCompositedLayer) {
          if (&pending_layer.graphics_layer->CcLayer() == layer.get()) {
            json_client = pending_layer.graphics_layer;
            break;
          }
        }
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
    layers_as_json.AddLayer(*layer, *transform, json_client);
  }
  return layers_as_json.Finalize();
}

scoped_refptr<cc::Layer> PaintArtifactCompositor::WrappedCcLayerForPendingLayer(
    const PendingLayer& pending_layer) {
  if (pending_layer.compositing_type != PendingLayer::kForeignLayer &&
      pending_layer.compositing_type != PendingLayer::kPreCompositedLayer)
    return nullptr;

  cc::Layer* layer = nullptr;
  FloatPoint layer_offset;
  if (pending_layer.compositing_type == PendingLayer::kPreCompositedLayer) {
    DCHECK(pending_layer.graphics_layer);
    DCHECK(!pending_layer.graphics_layer->ShouldCreateLayersAfterPaint());
    layer = &pending_layer.graphics_layer->CcLayer();
    layer_offset =
        FloatPoint(pending_layer.graphics_layer->GetOffsetFromTransformNode());
  } else {
    DCHECK_EQ(pending_layer.compositing_type, PendingLayer::kForeignLayer);
    // UpdateTouchActionRects() depends on the layer's offset, but when the
    // layer's offset changes, we do not call SetNeedsUpdate() (this is an
    // optimization because the update would only cause an extra commit)
    // This is only OK if the ForeignLayer doesn't have hit test data.
    DCHECK(!pending_layer.FirstPaintChunk().hit_test_data);
    const auto& foreign_layer_display_item =
        static_cast<const ForeignLayerDisplayItem&>(
            pending_layer.FirstDisplayItem());
    layer = foreign_layer_display_item.GetLayer();
    layer_offset = FloatPoint(foreign_layer_display_item.Offset());
  }
  layer->SetOffsetToTransformParent(gfx::Vector2dF(
      layer_offset + pending_layer.offset_of_decomposited_transforms));
  return layer;
}

const TransformPaintPropertyNode&
PaintArtifactCompositor::NearestScrollTranslationForLayer(
    const PendingLayer& pending_layer) {
  if (const auto* scroll_translation = ScrollTranslationForLayer(pending_layer))
    return *scroll_translation;

  const auto& transform = pending_layer.property_tree_state.Transform();
  // TODO(pdr): This could be a performance issue because it crawls up the
  // transform tree for each pending layer. If this is on profiles, we should
  // cache a lookup of transform node to scroll translation transform node.
  return transform.NearestScrollTranslationNode();
}

const TransformPaintPropertyNode*
PaintArtifactCompositor::ScrollTranslationForLayer(
    const PendingLayer& pending_layer) {
  // Not checking PendingLayer::kScrollHitTestLayer because a scroll hit test
  // chunk without a direct compositing reasons can still be composited (e.g.
  // when it can't be merged into any other layer).
  if (pending_layer.chunks.size() != 1)
    return nullptr;

  const auto& paint_chunk = pending_layer.FirstPaintChunk();
  if (!paint_chunk.hit_test_data)
    return nullptr;

  return paint_chunk.hit_test_data->scroll_translation;
}

scoped_refptr<cc::Layer>
PaintArtifactCompositor::ScrollHitTestLayerForPendingLayer(
    const PendingLayer& pending_layer) {
  const auto* scroll_translation = ScrollTranslationForLayer(pending_layer);
  if (!scroll_translation)
    return nullptr;

  // We shouldn't decomposite scroll transform nodes.
  DCHECK_EQ(FloatPoint(), pending_layer.offset_of_decomposited_transforms);

  const auto& scroll_node = *scroll_translation->ScrollNode();
  auto scroll_element_id = scroll_node.GetCompositorElementId();

  scoped_refptr<cc::Layer> scroll_layer;
  for (auto& existing_layer : scroll_hit_test_layers_) {
    if (existing_layer && existing_layer->element_id() == scroll_element_id)
      scroll_layer = existing_layer;
  }
  if (!scroll_layer) {
    scroll_layer = cc::Layer::Create();
    scroll_layer->SetElementId(scroll_element_id);
    scroll_layer->SetHitTestable(true);
  }

  scroll_layer->SetOffsetToTransformParent(
      gfx::Vector2dF(FloatPoint(scroll_node.ContainerRect().Location())));
  // TODO(pdr): The scroll layer's bounds are currently set to the clipped
  // container bounds but this does not include the border. We may want to
  // change this behavior to make non-composited and composited hit testing
  // match (see: crbug.com/753124). To do this, use
  // |scroll_hit_test->scroll_container_bounds|.
  auto bounds = scroll_node.ContainerRect().Size();
  // Set the layer's bounds equal to the container because the scroll layer
  // does not scroll.
  scroll_layer->SetBounds(static_cast<gfx::Size>(bounds));

  if (scroll_node.NodeChanged() != PaintPropertyChangeType::kUnchanged) {
    scroll_layer->SetNeedsPushProperties();
    scroll_layer->SetNeedsCommit();
  }

  return scroll_layer;
}

scoped_refptr<cc::ScrollbarLayerBase>
PaintArtifactCompositor::ScrollbarLayerForPendingLayer(
    const PendingLayer& pending_layer) {
  if (pending_layer.compositing_type != PendingLayer::kScrollbarLayer)
    return nullptr;

  const auto& item = pending_layer.FirstDisplayItem();
  DCHECK(item.IsScrollbar());

  // We should never decomposite scroll translations, so we don't need to adjust
  // the layer's offset for decomposited transforms.
  DCHECK_EQ(FloatPoint(), pending_layer.offset_of_decomposited_transforms);

  const auto& scrollbar_item = static_cast<const ScrollbarDisplayItem&>(item);
  cc::ScrollbarLayerBase* existing_layer = nullptr;
  for (auto& layer : scrollbar_layers_) {
    if (layer->element_id() == scrollbar_item.ElementId()) {
      existing_layer = layer.get();
      break;
    }
  }
  return scrollbar_item.CreateOrReuseLayer(existing_layer);
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

  IntRect cc_combined_bounds = EnclosingIntRect(pending_layer.bounds);
  auto cc_layer = content_layer_client->UpdateCcPictureLayer(
      pending_layer.chunks, cc_combined_bounds,
      pending_layer.property_tree_state);

  new_content_layer_clients.push_back(std::move(content_layer_client));

  // Set properties that foreign layers would normally control for themselves
  // here to avoid changing foreign layers. This includes things set by
  // GraphicsLayer on the ContentsLayer() or by video clients etc.
  cc_layer->SetContentsOpaque(pending_layer.rect_known_to_be_opaque.Contains(
      FloatRect(cc_combined_bounds)));

  return cc_layer;
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
    const PaintChunkSubset& chunks,
    const PaintChunkIterator& first_chunk,
    CompositingType compositing_type)
    : bounds(first_chunk->bounds),
      rect_known_to_be_opaque(first_chunk->known_to_be_opaque ? bounds
                                                              : FloatRect()),
      chunks(&chunks.GetPaintArtifact(), first_chunk.IndexInPaintArtifact()),
      property_tree_state(
          first_chunk->properties.GetPropertyTreeState().Unalias()),
      compositing_type(compositing_type) {
  DCHECK(!RequiresOwnLayer() || first_chunk->size() <= 1u);
}

PaintArtifactCompositor::PendingLayer::PendingLayer(
    const PreCompositedLayerInfo& pre_composited_layer)
    : chunks(pre_composited_layer.chunks),
      property_tree_state(
          pre_composited_layer.graphics_layer->GetPropertyTreeState()
              .Unalias()),
      graphics_layer(pre_composited_layer.graphics_layer),
      compositing_type(kPreCompositedLayer) {
  DCHECK(graphics_layer);
  DCHECK(!graphics_layer->ShouldCreateLayersAfterPaint());
}

FloatRect PaintArtifactCompositor::PendingLayer::UniteRectsKnownToBeOpaque(
    const FloatRect& a,
    const FloatRect& b) {
  // Check a or b by itself.
  FloatRect maximum(a);
  float maximum_area = a.Size().Area();
  if (b.Size().Area() > maximum_area) {
    maximum = b;
    maximum_area = b.Size().Area();
  }
  // Check the regions that include the intersection of a and b. This can be
  // done by taking the intersection and expanding it vertically and
  // horizontally. These expanded intersections will both still be fully opaque.
  FloatRect intersection = a;
  intersection.InclusiveIntersect(b);
  if (!intersection.IsZero()) {
    FloatRect vert_expanded_intersection(intersection);
    vert_expanded_intersection.ShiftYEdgeTo(std::min(a.Y(), b.Y()));
    vert_expanded_intersection.ShiftMaxYEdgeTo(std::max(a.MaxY(), b.MaxY()));
    if (vert_expanded_intersection.Size().Area() > maximum_area) {
      maximum = vert_expanded_intersection;
      maximum_area = vert_expanded_intersection.Size().Area();
    }
    FloatRect horiz_expanded_intersection(intersection);
    horiz_expanded_intersection.ShiftXEdgeTo(std::min(a.X(), b.X()));
    horiz_expanded_intersection.ShiftMaxXEdgeTo(std::max(a.MaxX(), b.MaxX()));
    if (horiz_expanded_intersection.Size().Area() > maximum_area) {
      maximum = horiz_expanded_intersection;
      maximum_area = horiz_expanded_intersection.Size().Area();
    }
  }
  return maximum;
}

FloatRect PaintArtifactCompositor::PendingLayer::MapRectKnownToBeOpaque(
    const PropertyTreeState& new_state) const {
  if (rect_known_to_be_opaque.IsEmpty())
    return FloatRect();

  FloatClipRect float_clip_rect(rect_known_to_be_opaque);
  GeometryMapper::LocalToAncestorVisualRect(property_tree_state, new_state,
                                            float_clip_rect);
  return float_clip_rect.IsTight() ? float_clip_rect.Rect() : FloatRect();
}

std::unique_ptr<JSONObject> PaintArtifactCompositor::PendingLayer::ToJSON()
    const {
  std::unique_ptr<JSONObject> result = std::make_unique<JSONObject>();
  result->SetArray("bounds", RectAsJSONArray(bounds));
  result->SetArray("rect_known_to_be_opaque",
                   RectAsJSONArray(rect_known_to_be_opaque));
  result->SetObject("property_tree_state", property_tree_state.ToJSON());
  result->SetArray("offset_of_decomposited_transforms",
                   PointAsJSONArray(offset_of_decomposited_transforms));
  std::unique_ptr<JSONArray> json_chunks = std::make_unique<JSONArray>();
  for (auto it = chunks.begin(); it != chunks.end(); ++it) {
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

FloatRect PaintArtifactCompositor::PendingLayer::VisualRectForOverlapTesting()
    const {
  FloatClipRect visual_rect(bounds);
  GeometryMapper::LocalToAncestorVisualRect(
      property_tree_state, PropertyTreeState::Root(), visual_rect,
      kIgnoreOverlayScrollbarSize, kNonInclusiveIntersect,
      kExpandVisualRectForAnimation);
  return visual_rect.Rect();
}

bool PaintArtifactCompositor::PendingLayer::Merge(const PendingLayer& guest) {
  PropertyTreeState new_state = PropertyTreeState::Uninitialized();
  if (!CanMerge(guest, guest.property_tree_state, &new_state, &bounds))
    return false;

  chunks.Merge(guest.chunks);
  rect_known_to_be_opaque =
      UniteRectsKnownToBeOpaque(MapRectKnownToBeOpaque(new_state),
                                guest.MapRectKnownToBeOpaque(new_state));
  property_tree_state = new_state;
  return true;
}

void PaintArtifactCompositor::PendingLayer::Upcast(
    const PropertyTreeState& new_state) {
  DCHECK(!RequiresOwnLayer());
  if (property_tree_state == new_state)
    return;

  FloatClipRect float_clip_rect(bounds);
  GeometryMapper::LocalToAncestorVisualRect(property_tree_state, new_state,
                                            float_clip_rect);
  bounds = float_clip_rect.Rect();

  rect_known_to_be_opaque = MapRectKnownToBeOpaque(new_state);
  property_tree_state = new_state;
}

const PaintChunk& PaintArtifactCompositor::PendingLayer::FirstPaintChunk()
    const {
  DCHECK(!RequiresOwnLayer() || chunks.size() == 1);
  return *chunks.begin();
}

const DisplayItem& PaintArtifactCompositor::PendingLayer::FirstDisplayItem()
    const {
#if DCHECK_IS_ON()
  // This method should never be called if the first paint chunk is empty.
  if (RequiresOwnLayer())
    DCHECK_EQ(FirstPaintChunk().size(), 1u);
  else
    DCHECK_GE(FirstPaintChunk().size(), 1u);
#endif
  return *chunks.begin().DisplayItems().begin();
}

static bool HasCompositedTransformToAncestor(
    const TransformPaintPropertyNode& node,
    const TransformPaintPropertyNode& ancestor) {
  for (const auto* n = &node; n != &ancestor; n = n->UnaliasedParent()) {
    if (n->HasDirectCompositingReasons())
      return true;
  }
  return false;
}

// Returns the lowest common ancestor if there is no composited transform
// between the two transforms.
static const TransformPaintPropertyNode* NonCompositedLowestCommonAncestor(
    const TransformPaintPropertyNode& transform1,
    const TransformPaintPropertyNode& transform2) {
  const auto& lca = transform1.LowestCommonAncestor(transform2).Unalias();
  if (HasCompositedTransformToAncestor(transform1, lca) ||
      HasCompositedTransformToAncestor(transform2, lca))
    return nullptr;
  return &lca;
}

static bool ClipChainHasCompositedTransformTo(
    const ClipPaintPropertyNode& node,
    const ClipPaintPropertyNode& ancestor,
    const TransformPaintPropertyNode& transform) {
  for (const auto* n = &node; n != &ancestor; n = n->UnaliasedParent()) {
    if (!NonCompositedLowestCommonAncestor(n->LocalTransformSpace().Unalias(),
                                           transform))
      return true;
  }
  return false;
}

// Determines whether drawings based on the 'guest' state can be painted into
// a layer with the 'home' state, and if yes, returns the common ancestor state
// to which both layer will be upcasted.
// A number of criteria need to be met:
// 1. The guest effect must be a descendant of the home effect. However this
//    check is enforced by the layerization recursion. Here we assume the
//    guest has already been upcasted to the same effect.
// 2. The guest transform and the home transform have compatible backface
//    visibility.
// 3. The guest transform space must be within compositing boundary of the home
//    transform space.
// 4. The local space of each clip and effect node on the ancestor chain must
//    be within compositing boundary of the home transform space.
base::Optional<PropertyTreeState> CanUpcastWith(const PropertyTreeState& guest,
                                                const PropertyTreeState& home) {
  DCHECK_EQ(&home.Effect(), &guest.Effect());

  if (home.Transform().IsBackfaceHidden() !=
      guest.Transform().IsBackfaceHidden())
    return base::nullopt;

  auto* upcast_transform =
      NonCompositedLowestCommonAncestor(home.Transform(), guest.Transform());
  if (!upcast_transform)
    return base::nullopt;

  const auto& clip_lca =
      home.Clip().LowestCommonAncestor(guest.Clip()).Unalias();
  if (ClipChainHasCompositedTransformTo(home.Clip(), clip_lca,
                                        *upcast_transform) ||
      ClipChainHasCompositedTransformTo(guest.Clip(), clip_lca,
                                        *upcast_transform))
    return base::nullopt;

  return PropertyTreeState(*upcast_transform, clip_lca, home.Effect());
}

// We will only allow merging if the merged-area:home-area+guest-area doesn't
// exceed the ratio |kMergingSparsityTolerance|:1.
static constexpr float kMergeSparsityTolerance = 6;

bool PaintArtifactCompositor::PendingLayer::CanMerge(
    const PendingLayer& guest,
    const PropertyTreeState& guest_state,
    PropertyTreeState* out_merged_state,
    FloatRect* out_merged_bounds) const {
  if (&chunks.GetPaintArtifact() != &guest.chunks.GetPaintArtifact())
    return false;
  if (RequiresOwnLayer() || guest.RequiresOwnLayer())
    return false;
  if (&property_tree_state.Effect() != &guest_state.Effect())
    return false;

  const base::Optional<PropertyTreeState>& merged_state =
      CanUpcastWith(guest_state, property_tree_state);
  if (!merged_state)
    return false;

  FloatClipRect new_home_bounds(bounds);
  GeometryMapper::LocalToAncestorVisualRect(property_tree_state, *merged_state,
                                            new_home_bounds);
  FloatClipRect new_guest_bounds(guest.bounds);
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

  if (out_merged_state)
    *out_merged_state = *merged_state;
  if (out_merged_bounds)
    *out_merged_bounds = merged_bounds;
  return true;
}

bool PaintArtifactCompositor::PendingLayer::MayDrawContent() const {
  return chunks.size() > 1 || FirstPaintChunk().size() > 0;
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

bool PaintArtifactCompositor::MightOverlap(const PendingLayer& layer_a,
                                           const PendingLayer& layer_b) {
  return layer_a.VisualRectForOverlapTesting().Intersects(
      layer_b.VisualRectForOverlapTesting());
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
  if (&layer.property_tree_state.Effect() != &effect)
    return false;
  if (layer.RequiresOwnLayer())
    return false;
  if (effect.HasDirectCompositingReasons())
    return false;

  PropertyTreeState group_state(effect.LocalTransformSpace().Unalias(),
                                effect.OutputClip()
                                    ? effect.OutputClip()->Unalias()
                                    : layer.property_tree_state.Clip(),
                                effect);
  base::Optional<PropertyTreeState> upcast_state =
      CanUpcastWith(layer.property_tree_state, group_state);
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
          !previous_sibling.CanMerge(layer, *upcast_state))
        return false;
    }
  }

  layer.Upcast(*upcast_state);
  return true;
}

static bool EffectGroupContainsChunk(
    const EffectPaintPropertyNode& group_effect,
    const PaintChunk& chunk) {
  const auto& effect = chunk.properties.Effect().Unalias();
  return &effect == &group_effect ||
         StrictUnaliasedChildOfAlongPath(group_effect, effect);
}

static bool SkipGroupIfEffectivelyInvisible(
    const PaintChunkSubset& chunks,
    const EffectPaintPropertyNode& group,
    PaintChunkIterator& chunk_cursor) {
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
  if (group.Opacity() >= kMinimumVisibleOpacity ||
      // TODO(crbug.com/937573): We should disable the optimization for all
      // cases that the invisible group will be composited, to ensure correct
      // composited hit testing and animation. Checking the effect node's
      // HasDirectCompositingReasons() is not enough.
      group.HasDirectCompositingReasons()) {
    return false;
  }

  // Fast-forward to just past the end of the chunk sequence within this
  // effect group.
  DCHECK(EffectGroupContainsChunk(group, *chunk_cursor));
  while (++chunk_cursor != chunks.end()) {
    if (!EffectGroupContainsChunk(group, *chunk_cursor))
      break;
  }
  return true;
}

static bool IsCompositedScrollHitTest(const PaintChunk& chunk) {
  if (!chunk.hit_test_data)
    return false;
  const auto* scroll_translation = chunk.hit_test_data->scroll_translation;
  return scroll_translation &&
         scroll_translation->HasDirectCompositingReasons();
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
    const PaintChunkSubset& chunks,
    const EffectPaintPropertyNode& current_group,
    PaintChunkIterator& chunk_cursor) {
  // Skip paint chunks that are effectively invisible due to opacity and don't
  // have a direct compositing reason.
  if (SkipGroupIfEffectivelyInvisible(chunks, current_group, chunk_cursor))
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
      // Case A: The next chunk belongs to the current group but no subgroup.
      PendingLayer::CompositingType compositing_type = PendingLayer::kOther;
      if (IsCompositedScrollHitTest(*chunk_cursor)) {
        compositing_type = PendingLayer::kScrollHitTestLayer;
      } else if (chunk_cursor->size()) {
        const auto& first_display_item = *chunk_cursor.DisplayItems().begin();
        if (first_display_item.IsForeignLayer())
          compositing_type = PendingLayer::kForeignLayer;
        else if (IsCompositedScrollbar(first_display_item))
          compositing_type = PendingLayer::kScrollbarLayer;
      }

      pending_layers_.emplace_back(chunks, chunk_cursor, compositing_type);
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
    DCHECK_EQ(&current_group, &new_layer.property_tree_state.Effect());
    // This iterates pending_layers_[first_layer_in_current_group:-1] in
    // reverse.
    for (wtf_size_t candidate_index = pending_layers_.size() - 1;
         candidate_index-- > first_layer_in_current_group;) {
      PendingLayer& candidate_layer = pending_layers_[candidate_index];
      if (candidate_layer.Merge(new_layer)) {
        pending_layers_.pop_back();
        break;
      }
      if (MightOverlap(new_layer, candidate_layer)) {
        new_layer.compositing_type = PendingLayer::kOverlap;
        break;
      }
    }
  }
}

void PaintArtifactCompositor::CollectPendingLayers(
    const Vector<PreCompositedLayerInfo>& pre_composited_layers) {
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
  SkRRect new_rrect = clip.PixelSnappedClipRect();
  IntRect layer_bounds = EnclosingIntRect(clip.PixelSnappedClipRect().Rect());
  bool needs_display = false;

  auto new_translation_2d_or_matrix =
      GeometryMapper::SourceToDestinationProjection(clip.LocalTransformSpace(),
                                                    transform);
  new_translation_2d_or_matrix.MapRect(layer_bounds);
  new_translation_2d_or_matrix.PostTranslate(-layer_bounds.X(),
                                             -layer_bounds.Y());

  if (!path && new_translation_2d_or_matrix.IsIdentityOr2DTranslation()) {
    const auto& translation = new_translation_2d_or_matrix.Translation2D();
    new_rrect.offset(translation.Width(), translation.Height());
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
      gfx::Vector2dF(layer_bounds.X(), layer_bounds.Y()));
  layer_->SetBounds(gfx::Size(layer_bounds.Size()));
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
      cc_list->push<cc::TranslateOp>(translation.Width(), translation.Height());
    } else {
      cc_list->push<cc::ConcatOp>(SkMatrix(TransformationMatrix::ToSkMatrix44(
          translation_2d_or_matrix_.Matrix())));
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
void PaintArtifactCompositor::DecompositeTransforms() {
  HashMap<const TransformPaintPropertyNode*, bool> can_be_decomposited;
  HashSet<const void*> clips_and_effects_seen;
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
      if (const auto* translation = ScrollTranslationForLayer(pending_layer))
        mark_not_decompositable(translation);
    }
  }

  // Now, for any transform nodes that can be de-composited, re-map their
  // transform to point to the correct parent, and set the
  // offset_to_transform_parent.
  for (auto& pending_layer : pending_layers_) {
    const auto* transform = &pending_layer.property_tree_state.Transform();
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
    const Vector<PreCompositedLayerInfo>& pre_composited_layers,
    const ViewportProperties& viewport_properties,
    const Vector<const TransformPaintPropertyNode*>& scroll_translation_nodes) {
  DCHECK(scroll_translation_nodes.IsEmpty() ||
         RuntimeEnabledFeatures::ScrollUnificationEnabled());
  DCHECK(root_layer_);
  DCHECK(NeedsUpdate());

  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  cc::LayerTreeHost* host = root_layer_->layer_tree_host();
  if (!host)
    return;

  TRACE_EVENT0("blink", "PaintArtifactCompositor::Update");

  host->property_trees()->scroll_tree.SetScrollCallbacks(scroll_callbacks_);
  root_layer_->set_property_tree_sequence_number(
      g_s_property_tree_sequence_number);

  LayerListBuilder layer_list_builder;
  PropertyTreeManager property_tree_manager(*this, *host->property_trees(),
                                            *root_layer_, layer_list_builder,
                                            g_s_property_tree_sequence_number);
  CollectPendingLayers(pre_composited_layers);

  UpdateCompositorViewportProperties(viewport_properties, property_tree_manager,
                                     host);

  // With ScrollUnification, we ensure a cc::ScrollNode for all
  // |scroll_translation_nodes|.
  if (RuntimeEnabledFeatures::ScrollUnificationEnabled()) {
    property_tree_manager.EnsureCompositorScrollNodes(scroll_translation_nodes);
  }

  Vector<std::unique_ptr<ContentLayerClientImpl>> new_content_layer_clients;
  new_content_layer_clients.ReserveCapacity(pending_layers_.size());
  Vector<scoped_refptr<cc::Layer>> new_scroll_hit_test_layers;
  Vector<scoped_refptr<cc::ScrollbarLayerBase>> new_scrollbar_layers;

  // Maps from cc effect id to blink effects. Containing only the effects
  // having composited layers.
  Vector<const EffectPaintPropertyNode*> blink_effects;

  for (auto& entry : synthesized_clip_cache_)
    entry.in_use = false;

  // See if we can de-composite any transforms.
  DecompositeTransforms();

  const PendingLayer* previous_pending_layer = nullptr;
  for (auto& pending_layer : pending_layers_) {
    const auto& property_state = pending_layer.property_tree_state;
    const auto& transform = property_state.Transform();
    const auto& clip = property_state.Clip();

    if (&clip.LocalTransformSpace() == &transform) {
      // Limit layer bounds to hide the areas that will be never visible
      // because of the clip.
      pending_layer.bounds.Intersect(clip.PixelSnappedClipRect().Rect());
    } else if (const auto* scroll = transform.ScrollNode()) {
      // Limit layer bounds to the scroll range to hide the areas that will
      // never be scrolled into the visible area.
      pending_layer.bounds.Intersect(FloatRect(
          IntRect(scroll->ContainerRect().Location(), scroll->ContentsSize())));
    }

    scoped_refptr<cc::Layer> layer = CompositedLayerForPendingLayer(
        pending_layer, new_content_layer_clients, new_scroll_hit_test_layers,
        new_scrollbar_layers);

    UpdateLayerProperties(*layer, pending_layer, &property_tree_manager);

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
        NearestScrollTranslationForLayer(pending_layer);
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
        PropertyTreeStateChanged(property_state)) {
      layer->SetSubtreePropertyChanged();
      root_layer_->SetNeedsCommit();
    }

    previous_pending_layer = &pending_layer;
  }

  property_tree_manager.Finalize();
  content_layer_clients_.swap(new_content_layer_clients);
  scroll_hit_test_layers_.swap(new_scroll_hit_test_layers);
  scrollbar_layers_.swap(new_scrollbar_layers);

  auto* new_end = std::remove_if(
      synthesized_clip_cache_.begin(), synthesized_clip_cache_.end(),
      [](const auto& entry) { return !entry.in_use; });
  synthesized_clip_cache_.Shrink(new_end - synthesized_clip_cache_.begin());

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
    const PendingLayer& pending_layer,
    PropertyTreeManager* property_tree_manager) {
  // Properties of foreign layers are managed by their owners.
  if (pending_layer.compositing_type == PendingLayer::kForeignLayer)
    return;

  PaintChunkSubset chunks = pending_layer.chunks;
  if (pending_layer.graphics_layer &&
      pending_layer.graphics_layer->PaintsContentOrHitTest()) {
    chunks = PaintChunkSubset(pending_layer.graphics_layer->GetPaintController()
                                  .GetPaintArtifactShared());
  }
  PaintChunksToCcLayer::UpdateLayerProperties(
      layer, pending_layer.property_tree_state, chunks, property_tree_manager);
}

void PaintArtifactCompositor::UpdateRepaintedLayerProperties() const {
  // TODO(paint-dev): Implement repaint-only update for CompositeAfterPaint.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  for (const auto& pending_layer : pending_layers_) {
    if (pending_layer.compositing_type != PendingLayer::kPreCompositedLayer)
      continue;
    DCHECK(pending_layer.graphics_layer);
    if (pending_layer.graphics_layer->Repainted()) {
      UpdateLayerProperties(pending_layer.graphics_layer->CcLayer(),
                            pending_layer);
    }
  }

  UpdateDebugInfo();
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
    const FloatPoint& scroll_offset) {
  if (!root_layer_ || !root_layer_->layer_tree_host())
    return false;
  auto* property_trees = root_layer_->layer_tree_host()->property_trees();
  if (!property_trees->element_id_to_scroll_node_index.contains(element_id))
    return false;
  PropertyTreeManager::DirectlySetScrollOffset(
      *root_layer_->layer_tree_host(), element_id,
      gfx::ScrollOffset(scroll_offset));
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
    CompositingReasons compositing_reasons,
    RasterInvalidationTracking* raster_invalidation_tracking) {
  cc::LayerDebugInfo& debug_info = layer.EnsureDebugInfo();

  debug_info.name = id.client.DebugName().Utf8();
  if (id.type == DisplayItem::kForeignLayerContentsWrapper) {
    // This is for backward compatibility in pre-CompositeAfterPaint mode.
    DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    debug_info.name = std::string("ContentsLayer for ") + debug_info.name;
  }

  debug_info.compositing_reasons =
      CompositingReason::Descriptions(compositing_reasons);
  debug_info.compositing_reason_ids =
      CompositingReason::ShortNames(compositing_reasons);
  debug_info.owner_node_id = id.client.OwnerNodeId();

  if (RasterInvalidationTracking::IsTracingRasterInvalidations() &&
      raster_invalidation_tracking) {
    raster_invalidation_tracking->AddToLayerDebugInfo(debug_info);
    raster_invalidation_tracking->ClearInvalidations();
  }
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
    switch (pending_layer.compositing_type) {
      case PendingLayer::kScrollHitTestLayer:
        layer = scroll_hit_test_layer_it->get();
        ++scroll_hit_test_layer_it;
        break;
      case PendingLayer::kPreCompositedLayer:
        tracking =
            pending_layer.graphics_layer->GetRasterInvalidationTracking();
        layer = &pending_layer.graphics_layer->CcLayer();
        break;
      case PendingLayer::kForeignLayer:
        layer = static_cast<const ForeignLayerDisplayItem&>(
                    pending_layer.FirstDisplayItem())
                    .GetLayer();
        break;
      case PendingLayer::kScrollbarLayer:
        layer = scrollbar_layer_it->get();
        ++scrollbar_layer_it;
        break;
      default:
        tracking =
            (*content_layer_client_it)->GetRasterInvalidator().GetTracking();
        layer = &(*content_layer_client_it)->Layer();
        ++content_layer_client_it;
        break;
    }
    UpdateLayerDebugInfo(
        *layer,
        pending_layer.graphics_layer
            ? PaintChunk::Id(*pending_layer.graphics_layer,
                             DisplayItem::kUninitializedType)
            : pending_layer.FirstPaintChunk().id,
        GetCompositingReasons(pending_layer, previous_pending_layer), tracking);
    previous_pending_layer = &pending_layer;
  }
}

CompositingReasons PaintArtifactCompositor::GetCompositingReasons(
    const PendingLayer& layer,
    const PendingLayer* previous_layer) const {
  DCHECK(layer_debug_info_enabled_);

  if (layer.graphics_layer)
    return layer.graphics_layer->GetCompositingReasons();

  if (layer.RequiresOwnLayer()) {
    const auto& first_chunk = layer.FirstPaintChunk();
    if (IsCompositedScrollHitTest(first_chunk))
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
  if (!previous_layer || &layer.property_tree_state.Transform() !=
                             &previous_layer->property_tree_state.Transform()) {
    reasons |= layer.property_tree_state.Transform()
                   .DirectCompositingReasonsForDebugging();
    if (!layer.property_tree_state.Transform().BackfaceVisibilitySameAsParent())
      reasons |= CompositingReason::kBackfaceVisibilityHidden;
  }

  if (!previous_layer || &layer.property_tree_state.Effect() !=
                             &previous_layer->property_tree_state.Effect()) {
    const auto& effect = layer.property_tree_state.Effect();
    if (effect.HasDirectCompositingReasons())
      reasons |= effect.DirectCompositingReasonsForDebugging();
    if (reasons == CompositingReason::kNone &&
        layer.compositing_type == PendingLayer::kOther) {
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
      layer.compositing_type == PendingLayer::kOverlap)
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
    layer.property_tree_state.ClearChangedTo(PropertyTreeState::Root());
    PaintChunkSubset chunks =
        layer.graphics_layer && layer.graphics_layer->PaintsContentOrHitTest()
            ? PaintChunkSubset(layer.graphics_layer->GetPaintController()
                                   .GetPaintArtifactShared())
            : layer.chunks;
    for (auto& chunk : chunks) {
      // Calling |ClearChangedTo| for every chunk could be O(|property nodes|^2)
      // in the worst case and could be optimized by caching which nodes that
      // have already been cleared.
      chunk.properties.GetPropertyTreeState().ClearChangedTo(
          layer.property_tree_state);
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
    size_t chunks_size = layer.chunks.ApproximateUnsharedMemoryUsage();
    DCHECK_GE(chunks_size, sizeof(layer.chunks));
    result += chunks_size - sizeof(layer.chunks);
  }
  return result;
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

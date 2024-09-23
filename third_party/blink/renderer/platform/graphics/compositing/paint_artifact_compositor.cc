// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

#include <memory>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "cc/base/features.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_flags.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host.h"
#include "cc/view_transition/view_transition_request.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/adjust_mask_layer_geometry.h"
#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
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

class PaintArtifactCompositor::OldPendingLayerMatcher {
  STACK_ALLOCATED();

 public:
  explicit OldPendingLayerMatcher(PendingLayers pending_layers)
      : pending_layers_(std::move(pending_layers)) {}

  // Finds the next PendingLayer that can be matched by |new_layer|.
  // It's efficient if most of the pending layers can be matched sequentially.
  PendingLayer* Find(const PendingLayer& new_layer) {
    if (pending_layers_.empty())
      return nullptr;
    if (!new_layer.FirstPaintChunk().CanMatchOldChunk())
      return nullptr;
    wtf_size_t i = next_index_;
    do {
      wtf_size_t next = (i + 1) % pending_layers_.size();
      if (new_layer.Matches(pending_layers_[i])) {
        next_index_ = next;
        return &pending_layers_[i];
      }
      i = next;
    } while (i != next_index_);
    return nullptr;
  }

 private:
  wtf_size_t next_index_ = 0;
  PendingLayers pending_layers_;
};

PaintArtifactCompositor::PaintArtifactCompositor(
    base::WeakPtr<CompositorScrollCallbacks> scroll_callbacks)
    : scroll_callbacks_(std::move(scroll_callbacks)),
      tracks_raster_invalidations_(VLOG_IS_ON(3)) {
  root_layer_ = cc::Layer::Create();
}

PaintArtifactCompositor::~PaintArtifactCompositor() {}

void PaintArtifactCompositor::Trace(Visitor* visitor) const {
  visitor->Trace(pending_layers_);
  visitor->Trace(painted_scroll_translations_);
  visitor->Trace(synthesized_clip_cache_);
}

void PaintArtifactCompositor::SetTracksRasterInvalidations(bool should_track) {
  tracks_raster_invalidations_ = should_track || VLOG_IS_ON(3);
  for (auto& pending_layer : pending_layers_) {
    if (auto* client = pending_layer.GetContentLayerClient())
      client->GetRasterInvalidator().SetTracksRasterInvalidations(should_track);
  }
}

void PaintArtifactCompositor::WillBeRemovedFromFrame() {
  root_layer_->RemoveAllChildren();
}

void PaintArtifactCompositor::SetLCDTextPreference(
    LCDTextPreference preference) {
  if (lcd_text_preference_ == preference) {
    return;
  }
  SetNeedsUpdate();
  lcd_text_preference_ = preference;
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
  if (!tracks_raster_invalidations_) {
    flags &= ~(kLayerTreeIncludesInvalidations |
               kLayerTreeIncludesDetailedInvalidations);
  }

  LayersAsJSON layers_as_json(flags);
  for (const auto& layer : root_layer_->children()) {
    const ContentLayerClientImpl* layer_client = nullptr;
    const TransformPaintPropertyNode* transform = nullptr;
    for (const auto& pending_layer : pending_layers_) {
      if (layer.get() == &pending_layer.CcLayer()) {
        layer_client = pending_layer.GetContentLayerClient();
        transform = &pending_layer.GetPropertyTreeState().Transform();
        break;
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
    layers_as_json.AddLayer(*layer, *transform, layer_client);
  }
  return layers_as_json.Finalize();
}

const TransformPaintPropertyNode&
PaintArtifactCompositor::ScrollTranslationStateForLayer(
    const PendingLayer& pending_layer) {
  if (pending_layer.GetCompositingType() == PendingLayer::kScrollHitTestLayer) {
    return pending_layer.ScrollTranslationForScrollHitTestLayer();
  }

  // When HitTestOpaqueness is enabled, use the correct scroll state for fixed
  // position content, so scrolls on fixed content is correctly handled on the
  // compositor if the fixed content is opaque to hit test.
  const auto& transform = pending_layer.GetPropertyTreeState().Transform();
  return RuntimeEnabledFeatures::HitTestOpaquenessEnabled()
             ? transform.ScrollTranslationState()
             : transform.NearestScrollTranslationNode();
}

bool PaintArtifactCompositor::NeedsCompositedScrolling(
    const TransformPaintPropertyNode& scroll_translation) const {
  // This function needs painted_scroll_translations_ which is only available
  // during full update.
  DCHECK(needs_update_);
  DCHECK(scroll_translation.ScrollNode());
  if (scroll_translation.HasDirectCompositingReasons()) {
    return true;
  }
  // Note: main thread scrolling reasons are not checked here because even if
  // the scroller needs main thread to update scroll, compositing the scroller
  // can still benefit performance by reducing raster invalidations.
  auto it = painted_scroll_translations_.find(&scroll_translation);
  if (it == painted_scroll_translations_.end()) {
    // Negative z-index scrolling contents in a non-stacking-context scroller
    // appear earlier than the ScrollHitTest of the scroller, and this
    // method can be called before ComputeNeedsCompositedScrolling() for the
    // ScrollHitTest. If LCD-text is strongly preferred, here we assume the
    // scroller is not composited. Even if later the scroller is found to
    // have an opaque background and composited, not compositing the negative
    // z-index contents won't cause any problem because they (with possible
    // wrong rendering) are obscured by the opaque background.
    return lcd_text_preference_ != LCDTextPreference::kStronglyPreferred;
  }
  return it->value.is_composited;
}

bool PaintArtifactCompositor::ShouldForceMainThreadRepaint(
    const TransformPaintPropertyNode& scroll_translation) const {
  DCHECK(!NeedsCompositedScrolling(scroll_translation));
  auto it = painted_scroll_translations_.find(&scroll_translation);
  return it != painted_scroll_translations_.end() &&
         it->value.force_main_thread_repaint;
}

bool PaintArtifactCompositor::ComputeNeedsCompositedScrolling(
    const PaintArtifact& artifact,
    PaintChunks::const_iterator chunk_cursor) const {
  // The chunk must be a ScrollHitTest chunk which contains no display items.
  DCHECK(chunk_cursor->hit_test_data);
  DCHECK(chunk_cursor->hit_test_data->scroll_translation);
  DCHECK_EQ(chunk_cursor->size(), 0u);
  const auto& scroll_translation =
      *chunk_cursor->hit_test_data->scroll_translation;
  DCHECK(scroll_translation.ScrollNode());
  if (scroll_translation.HasDirectCompositingReasons()) {
    return true;
  }
  // Don't automatically composite non-user-scrollable scrollers.
  if (!scroll_translation.ScrollNode()->UserScrollable()) {
    return false;
  }
  auto preference =
      scroll_translation.ScrollNode()->GetCompositedScrollingPreference();
  if (preference == CompositedScrollingPreference::kNotPreferred) {
    return false;
  }
  if (preference == CompositedScrollingPreference::kPreferred) {
    return true;
  }
  if (lcd_text_preference_ != LCDTextPreference::kStronglyPreferred) {
    return true;
  }
  // Find the chunk containing the scrolling background which normally defines
  // the opaqueness of the scrolling contents. If it has an opaque rect
  // covering the whole scrolling contents, we can use composited scrolling
  // without losing LCD text.
  for (auto next = chunk_cursor + 1; next != artifact.GetPaintChunks().end();
       ++next) {
    if (&next->properties.Transform() ==
        &chunk_cursor->properties.Transform()) {
      // Skip scroll controls that are painted in the same transform space
      // as the ScrollHitTest.
      continue;
    }
    return &next->properties.Transform().Unalias() == &scroll_translation &&
           &next->properties.Clip().Unalias() ==
               scroll_translation.ScrollNode()->OverflowClipNode() &&
           &next->properties.Effect().Unalias() ==
               &chunk_cursor->properties.Effect().Unalias() &&
           next->rect_known_to_be_opaque.Contains(
               scroll_translation.ScrollNode()->ContentsRect());
  }
  return true;
}

void PaintArtifactCompositor::UpdatePaintedScrollTranslationsBeforeLayerization(
    const PaintArtifact& artifact,
    PaintChunks::const_iterator chunk_cursor) {
  const PaintChunk& chunk = *chunk_cursor;
  const HitTestData* hit_test_data = chunk.hit_test_data.Get();
  if (hit_test_data && hit_test_data->scroll_translation) {
    const auto& scroll_translation = *hit_test_data->scroll_translation;
    bool is_composited =
        ComputeNeedsCompositedScrolling(artifact, chunk_cursor);
    auto it = painted_scroll_translations_.find(&scroll_translation);
    if (it == painted_scroll_translations_.end()) {
      painted_scroll_translations_.insert(
          &scroll_translation,
          ScrollTranslationInfo{.scrolling_contents_cull_rect =
                                    hit_test_data->scrolling_contents_cull_rect,
                                .is_composited = is_composited});
    } else {
      // The node was added in the second half of this function before.
      // Update the is_composited field now.
      it->value.scrolling_contents_cull_rect =
          hit_test_data->scrolling_contents_cull_rect;
      if (is_composited) {
        it->value.is_composited = true;
        it->value.force_main_thread_repaint = false;
      } else {
        CHECK(!it->value.is_composited);
      }
    }
  }

  // Touch action region, wheel event region, region capture and selection
  // under a non-composited scroller depend on the scroll offset so need to
  // force main-thread repaint. Non-fast scrollable region doesn't matter
  // because that of a nested non-composited scroller is always covered by
  // that of the parent non-composited scroller.
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled() &&
      ((hit_test_data &&
        (!hit_test_data->touch_action_rects.empty() ||
         !hit_test_data->wheel_event_rects.empty() ||
         // HitTestData of these types induce touch action regions.
         chunk.id.type == DisplayItem::Type::kScrollbarHitTest ||
         chunk.id.type == DisplayItem::Type::kResizerScrollHitTest)) ||
       chunk.region_capture_data || chunk.layer_selection_data)) {
    const auto& transform = chunk.properties.Transform().Unalias();
    // Mark all non-composited scroll ancestors within the same direct
    // compositing boundary (ideally we should check for both direct and
    // indirect compositing boundaries but that's impossible before full
    // layerization) also needing main thread repaint.
    const auto* composited_ancestor =
        transform.NearestDirectlyCompositedAncestor();
    for (const auto* scroll_translation =
             &transform.NearestScrollTranslationNode();
         scroll_translation;
         scroll_translation =
             scroll_translation->ParentScrollTranslationNode()) {
      if (scroll_translation->NearestDirectlyCompositedAncestor() !=
          composited_ancestor) {
        break;
      }
      auto it = painted_scroll_translations_.find(scroll_translation);
      if (it == painted_scroll_translations_.end()) {
        // The paint chunk appears before the ScrollHitTest of the scroll
        // translation. We'll complete the data when we see the ScrollHitTest.
        painted_scroll_translations_.insert(
            scroll_translation,
            ScrollTranslationInfo{.force_main_thread_repaint = true});
      } else {
        if (it->value.is_composited || it->value.force_main_thread_repaint) {
          break;
        }
        it->value.force_main_thread_repaint = true;
      }
    }
  }
}

PendingLayer::CompositingType PaintArtifactCompositor::ChunkCompositingType(
    const PaintArtifact& artifact,
    const PaintChunk& chunk) const {
  if (chunk.hit_test_data && chunk.hit_test_data->scroll_translation &&
      NeedsCompositedScrolling(*chunk.hit_test_data->scroll_translation)) {
    return PendingLayer::kScrollHitTestLayer;
  }
  if (chunk.size() == 1) {
    const auto& item = artifact.GetDisplayItemList()[chunk.begin_index];
    if (item.IsForeignLayer()) {
      return PendingLayer::kForeignLayer;
    }
    if (const auto* scrollbar = DynamicTo<ScrollbarDisplayItem>(item)) {
      if (const auto* scroll_translation = scrollbar->ScrollTranslation()) {
        if (RuntimeEnabledFeatures::RasterInducingScrollEnabled() ||
            NeedsCompositedScrolling(*scroll_translation)) {
          return PendingLayer::kScrollbarLayer;
        }
      }
    }
  }
  return PendingLayer::kOther;
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
    DCHECK_EQ(previous.DrawsContent(), repainted.DrawsContent());
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
      base::debug::DumpWithoutCrashing();
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
  // Whether background color is transparent affects cc::Layers's contents
  // opaque property.
  if ((previous.background_color.color == SkColors::kTransparent) !=
      (repainted.background_color.color == SkColors::kTransparent)) {
    return true;
  }

  // |has_text| affects compositing decisions (see:
  // |PendingLayer::MergeInternal|).
  if (previous.has_text != repainted.has_text)
    return true;

  // |PaintChunk::DrawsContent()| affects whether a layer draws content which
  // affects whether mask layers are created (see:
  // |SwitchToEffectNodeWithSynthesizedClip|).
  if (previous.DrawsContent() != repainted.DrawsContent())
    return true;

  // Solid color status change requires full update to change the cc::Layer
  // type.
  if (previous.background_color.is_solid_color !=
      repainted.background_color.is_solid_color) {
    return true;
  }

  // Hit test opaqueness of the paint chunk may affect that of cc::Layer.
  if (previous.hit_test_opaqueness != repainted.hit_test_opaqueness) {
    return true;
  }

  // Debugging for https://crbug.com/1237389 and https://crbug.com/1230104.
  // Before returning that a full update is not needed, check that the
  // properties are changed, which would indicate a missing call to
  // SetNeedsUpdate.
  if (previous.properties != repainted.properties) {
    base::debug::DumpWithoutCrashing();
    return true;
  }

  return false;
}

}  // namespace

void PaintArtifactCompositor::SetNeedsFullUpdateAfterPaintIfNeeded(
    const PaintArtifact& previous,
    const PaintArtifact& repainted) {
  if (needs_update_)
    return;

  // Adding or removing chunks requires a full update to add/remove cc::layers.
  if (previous.GetPaintChunks().size() != repainted.GetPaintChunks().size()) {
    SetNeedsUpdate();
    return;
  }

  // Loop over both paint chunk subsets in order.
  for (wtf_size_t i = 0; i < previous.GetPaintChunks().size(); i++) {
    if (NeedsFullUpdateAfterPaintingChunk(
            previous.GetPaintChunks()[i], previous,
            repainted.GetPaintChunks()[i], repainted)) {
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

class PaintArtifactCompositor::Layerizer {
  STACK_ALLOCATED();

 public:
  Layerizer(PaintArtifactCompositor& compositor,
            const PaintArtifact& artifact,
            wtf_size_t reserve_capacity)
      : compositor_(compositor),
        artifact_(artifact),
        chunk_cursor_(artifact.GetPaintChunks().begin()) {
    pending_layers_.reserve(reserve_capacity);
  }

  PendingLayers Layerize();

 private:
  // This is the internal recursion of Layerize(). This function loops over the
  // list of paint chunks, scoped by an isolated group (i.e. effect node).
  // Inside of the loop, chunks are tested for overlap and merge compatibility.
  // Subgroups are handled by recursion, and will be tested for "decompositing"
  // upon return.
  //
  // Merge compatibility means consecutive chunks may be layerized into the
  // same backing (i.e. merged) if their property states don't cross
  // direct-compositing boundary.
  //
  // Non-consecutive chunks that are nevertheless compatible may still be
  // merged, if reordering of the chunks won't affect the ultimate result.
  // This is determined by overlap testing such that chunks can be safely
  // reordered if their effective bounds in screen space can't overlap.
  //
  // The recursion only tests merge & overlap for chunks scoped by the same
  // group. This is where "decompositing" came in. Upon returning from a
  // recursion, the layerization of the subgroup may be tested for merge &
  // overlap with other chunks in the parent group, if grouping requirement
  // can be satisfied (and the effect node has no direct reason).
  void LayerizeGroup(const EffectPaintPropertyNode&, bool force_draws_content);
  bool DecompositeEffect(const EffectPaintPropertyNode& parent_effect,
                         wtf_size_t first_layer_in_parent_group_index,
                         const EffectPaintPropertyNode& effect,
                         wtf_size_t layer_index);

  PaintArtifactCompositor& compositor_;
  const PaintArtifact& artifact_;
  PaintChunks::const_iterator chunk_cursor_;
  PendingLayers pending_layers_;
  // This is to optimize the first time a paint property tree node is
  // encountered that has direct compositing reasons. This case will always
  // start a new layer and can skip merge tests. New values are added when
  // transform nodes are first encountered.
  HeapHashSet<Member<const TransformPaintPropertyNode>>
      directly_composited_transforms_;
};

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

bool PaintArtifactCompositor::Layerizer::DecompositeEffect(
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
  if (layer.ChunkRequiresOwnLayer())
    return false;
  if (effect.HasDirectCompositingReasons())
    return false;

  PropertyTreeState group_state(effect.LocalTransformSpace().Unalias(),
                                effect.OutputClip()
                                    ? effect.OutputClip()->Unalias()
                                    : layer.GetPropertyTreeState().Clip(),
                                effect);
  auto is_composited_scroll = [this](const TransformPaintPropertyNode& t) {
    return compositor_.NeedsCompositedScrolling(t);
  };
  std::optional<PropertyTreeState> upcast_state = group_state.CanUpcastWith(
      layer.GetPropertyTreeState(), is_composited_scroll);
  if (!upcast_state)
    return false;

  upcast_state->SetEffect(parent_effect);

  // An exotic blend mode can be decomposited only if the src (`layer`) and
  // the dest (previous layers in the parent group) will be in the same
  // composited layer to ensure the blend mode has access to both the src and
  // the dest.
  if (effect.BlendMode() != SkBlendMode::kSrcOver) {
    auto num_previous_siblings =
        layer_index - first_layer_in_parent_group_index;
    // If num_previous_siblings is zero, the dest is empty, and the blend mode
    // can be decomposited.
    if (num_previous_siblings) {
      if (num_previous_siblings > 2) {
        // If the dest has multiple composited layers, the blend mode must be
        // composited, too.
        return false;
      }
      if (num_previous_siblings == 2 &&
          // Same as the above, but if the first layer doesn't draw content,
          // only the second layer is the dest, and we'll check CanMerge()
          // with the second layer below.
          pending_layers_[first_layer_in_parent_group_index].DrawsContent()) {
        return false;
      }
      // The previous sibling is the dest. Check whether the src (`layer`), if
      // it's upcasted, can be merged with the dest so that they will be in
      // the same composited layer.
      const auto& previous_sibling = pending_layers_[layer_index - 1];
      if (previous_sibling.DrawsContent() &&
          !previous_sibling.CanMergeWithDecompositedBlendMode(
              layer, *upcast_state, is_composited_scroll)) {
        return false;
      }
    }
  }

  layer.Upcast(*upcast_state);
  return true;
}

void PaintArtifactCompositor::Layerizer::LayerizeGroup(
    const EffectPaintPropertyNode& current_group,
    bool force_draws_content) {
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
  while (chunk_cursor_ != artifact_.GetPaintChunks().end()) {
    // Look at the effect node of the next chunk. There are 3 possible cases:
    // A. The next chunk belongs to the current group but no subgroup.
    // B. The next chunk does not belong to the current group.
    // C. The next chunk belongs to some subgroup of the current group.
    const auto& chunk_effect = chunk_cursor_->properties.Effect().Unalias();
    if (&chunk_effect == &current_group) {
      compositor_.UpdatePaintedScrollTranslationsBeforeLayerization(
          artifact_, chunk_cursor_);
      pending_layers_.emplace_back(
          artifact_, *chunk_cursor_,
          compositor_.ChunkCompositingType(artifact_, *chunk_cursor_));
      ++chunk_cursor_;
      // force_draws_content doesn't apply to pending layers that require own
      // layer, specifically scrollbar layers, foreign layers, scroll hit
      // testing layers.
      if (pending_layers_.back().ChunkRequiresOwnLayer()) {
        continue;
      }
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
      LayerizeGroup(*subgroup, force_draws_content || subgroup->DrawsContent());
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
    DCHECK(!new_layer.ChunkRequiresOwnLayer());
    DCHECK_EQ(&current_group, &new_layer.GetPropertyTreeState().Effect());
    if (force_draws_content)
      new_layer.ForceDrawsContent();

    // If the new layer is the first using the nearest directly composited
    // ancestor, it can't be merged into any previous layers, so skip the merge
    // and overlap loop below.
    if (const auto* composited_transform =
            new_layer.GetPropertyTreeState()
                .Transform()
                .NearestDirectlyCompositedAncestor()) {
      if (directly_composited_transforms_.insert(composited_transform)
              .is_new_entry) {
        continue;
      }
    }

    // This iterates pending_layers_[first_layer_in_current_group:-1] in
    // reverse.
    auto is_composited_scroll = [this](const TransformPaintPropertyNode& t) {
      return compositor_.NeedsCompositedScrolling(t);
    };
    for (wtf_size_t candidate_index = pending_layers_.size() - 1;
         candidate_index-- > first_layer_in_current_group;) {
      PendingLayer& candidate_layer = pending_layers_[candidate_index];
      if (candidate_layer.Merge(new_layer, compositor_.lcd_text_preference_,
                                is_composited_scroll)) {
        pending_layers_.pop_back();
        break;
      }
      if (new_layer.MightOverlap(candidate_layer)) {
        new_layer.SetCompositingTypeToOverlap();
        break;
      }
    }
  }
}

PendingLayers PaintArtifactCompositor::Layerizer::Layerize() {
  LayerizeGroup(EffectPaintPropertyNode::Root(), /*force_draws_content=*/false);
  DCHECK(chunk_cursor_ == artifact_.GetPaintChunks().end());
  pending_layers_.ShrinkToReasonableCapacity();
  return std::move(pending_layers_);
}

void SynthesizedClip::UpdateLayer(const ClipPaintPropertyNode& clip,
                                  const TransformPaintPropertyNode& transform) {
  if (!layer_) {
    layer_ = cc::PictureLayer::Create(this);
    layer_->SetIsDrawable(true);
    // The clip layer must be hit testable because the compositor may not know
    // whether the hit test is clipped out.
    // See: cc::LayerTreeHostImpl::IsInitialScrollHitTestReliable().
    layer_->SetHitTestable(true);
  }
  CHECK_EQ(layer_->client(), this);

  const auto& path = clip.ClipPath();
  SkRRect new_rrect(clip.PaintClipRect());
  gfx::Rect layer_rect = gfx::ToEnclosingRect(clip.PaintClipRect().Rect());
  bool needs_display = false;

  gfx::Transform new_projection = GeometryMapper::SourceToDestinationProjection(
      clip.LocalTransformSpace(), transform);
  layer_rect = new_projection.MapRect(layer_rect);
  gfx::Vector2dF layer_offset(layer_rect.OffsetFromOrigin());
  gfx::Size layer_bounds = layer_rect.size();
  AdjustMaskLayerGeometry(transform, layer_offset, layer_bounds);
  new_projection.PostTranslate(-layer_offset);

  if (!path && new_projection.IsIdentityOr2dTranslation()) {
    gfx::Vector2dF translation = new_projection.To2dTranslation();
    new_rrect.offset(translation.x(), translation.y());
    needs_display = !rrect_is_local_ || new_rrect != rrect_;
    projection_.MakeIdentity();
    rrect_is_local_ = true;
  } else {
    needs_display = rrect_is_local_ || new_rrect != rrect_ ||
                    new_projection != projection_ ||
                    !clip.ClipPathEquals(path_);
    projection_ = new_projection;
    rrect_is_local_ = false;
  }

  if (needs_display)
    layer_->SetNeedsDisplay();

  layer_->SetOffsetToTransformParent(layer_offset);
  layer_->SetBounds(layer_bounds);
  rrect_ = new_rrect;
  path_ = path;
}

scoped_refptr<cc::DisplayItemList>
SynthesizedClip::PaintContentsToDisplayList() {
  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  cc_list->StartPaint();
  if (rrect_is_local_) {
    cc_list->push<cc::DrawRRectOp>(rrect_, flags);
  } else {
    cc_list->push<cc::SaveOp>();
    if (projection_.IsIdentityOr2dTranslation()) {
      gfx::Vector2dF translation = projection_.To2dTranslation();
      cc_list->push<cc::TranslateOp>(translation.x(), translation.y());
    } else {
      cc_list->push<cc::ConcatOp>(gfx::TransformToSkM44(projection_));
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
  auto entry = base::ranges::find_if(
      synthesized_clip_cache_, [&clip](const auto& entry) {
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
    synthesized_clip.UpdateLayer(clip, transform);
    synthesized_clip.Layer()->SetLayerTreeHost(root_layer_->layer_tree_host());
    if (layer_debug_info_enabled_ && !synthesized_clip.Layer()->debug_info())
      synthesized_clip.Layer()->SetDebugName("Synthesized Clip");
  }

  if (!should_always_update_on_scroll_) {
    // If there is any scroll translation between `clip.LocalTransformSpace`
    // and `transform`, the synthesized clip's fast rounded border or layer
    // geometry and paint operations depend on the scroll offset and we need to
    // update them on each scroll of the scroller.
    const auto& clip_transform = clip.LocalTransformSpace().Unalias();
    if (&clip_transform != &transform &&
        &clip_transform.NearestScrollTranslationNode() !=
            &transform.NearestScrollTranslationNode()) {
      should_always_update_on_scroll_ = true;
    }
  }

  mask_isolation_id = synthesized_clip.GetMaskIsolationId();
  mask_effect_id = synthesized_clip.GetMaskEffectId();
  return synthesized_clip;
}

void PaintArtifactCompositor::UpdateCompositorViewportProperties(
    const ViewportProperties& properties,
    PropertyTreeManager& property_tree_manager,
    cc::LayerTreeHost* layer_tree_host) {
  // The inner and outer viewports' existence is linked. That is, either they're
  // both null or they both exist.
  CHECK_EQ(static_cast<bool>(properties.outer_scroll_translation),
           static_cast<bool>(properties.inner_scroll_translation));
  CHECK(!properties.outer_clip ||
        static_cast<bool>(properties.inner_scroll_translation));

  cc::ViewportPropertyIds ids;
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
    ids.inner_scroll =
        property_tree_manager.EnsureCompositorInnerScrollAndTransformNode(
            *properties.inner_scroll_translation);
    if (properties.outer_clip) {
      ids.outer_clip = property_tree_manager.EnsureCompositorClipNode(
          *properties.outer_clip);
    }
    CHECK(properties.outer_scroll_translation);
    ids.outer_scroll =
        property_tree_manager.EnsureCompositorOuterScrollAndTransformNode(
            *properties.outer_scroll_translation);

    CHECK(NeedsCompositedScrolling(*properties.inner_scroll_translation));
    CHECK(NeedsCompositedScrolling(*properties.outer_scroll_translation));
    painted_scroll_translations_.insert(
        properties.inner_scroll_translation,
        ScrollTranslationInfo{InfiniteIntRect(), true});
    painted_scroll_translations_.insert(
        properties.outer_scroll_translation,
        ScrollTranslationInfo{InfiniteIntRect(), true});
  }

  layer_tree_host->RegisterViewportPropertyIds(ids);
}

void PaintArtifactCompositor::Update(
    const PaintArtifact& artifact,
    const ViewportProperties& viewport_properties,
    const StackScrollTranslationVector& scroll_translation_nodes,
    Vector<std::unique_ptr<cc::ViewTransitionRequest>> transition_requests) {
  // See: |UpdateRepaintedLayers| for repaint updates.
  DCHECK(needs_update_);
  DCHECK(root_layer_);

  TRACE_EVENT0("blink", "PaintArtifactCompositor::Update");

  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  cc::LayerTreeHost* host = root_layer_->layer_tree_host();
  if (!host)
    return;

  for (auto& request : transition_requests)
    host->AddViewTransitionRequest(std::move(request));

  host->property_trees()->scroll_tree_mutable().SetScrollCallbacks(
      scroll_callbacks_);
  root_layer_->set_property_tree_sequence_number(
      g_s_property_tree_sequence_number);

  wtf_size_t old_size = pending_layers_.size();
  OldPendingLayerMatcher old_pending_layer_matcher(std::move(pending_layers_));
  CHECK(painted_scroll_translations_.empty());

  // Make compositing decisions, storing the result in |pending_layers_|.
  pending_layers_ = Layerizer(*this, artifact, old_size).Layerize();
  PendingLayer::DecompositeTransforms(pending_layers_);

  LayerListBuilder layer_list_builder;
  PropertyTreeManager property_tree_manager(*this, *host->property_trees(),
                                            *root_layer_, layer_list_builder,
                                            g_s_property_tree_sequence_number);

  UpdateCompositorViewportProperties(viewport_properties, property_tree_manager,
                                     host);

  should_always_update_on_scroll_ = false;
  for (auto& entry : synthesized_clip_cache_)
    entry.in_use = false;

  // Ensure scroll and scroll translation nodes which may be referenced by
  // AnchorPositionScrollTranslation nodes, to reduce chance of inefficient
  // stale_forward_dependencies in cc::TransformTree::AnchorPositionOffset().
  // We want to create a cc::TransformNode only if the scroller is painted.
  // This avoids violating an assumption in CompositorAnimations that an
  // element has property nodes for either all or none of its animating
  // properties (see crbug.com/1385575).
  // However, we want to create a cc::ScrollNode regardless of whether the
  // scroller is painted. This ensures that scroll offset animations aren't
  // affected by becoming unpainted.
  for (auto& node : scroll_translation_nodes) {
    property_tree_manager.EnsureCompositorScrollNode(*node);
  }
  for (auto& [node, info] : painted_scroll_translations_) {
    property_tree_manager.EnsureCompositorScrollAndTransformNode(
        *node, info.scrolling_contents_cull_rect);
  }

  cc::LayerSelection layer_selection;
  for (auto& pending_layer : pending_layers_) {
    pending_layer.UpdateCompositedLayer(
        old_pending_layer_matcher.Find(pending_layer), layer_selection,
        tracks_raster_invalidations_, root_layer_->layer_tree_host());

    cc::Layer& layer = pending_layer.CcLayer();
    const auto& property_state = pending_layer.GetPropertyTreeState();
    const auto& transform = property_state.Transform();
    const auto& clip = property_state.Clip();
    const auto& effect = property_state.Effect();
    int transform_id =
        property_tree_manager.EnsureCompositorTransformNode(transform);
    int effect_id = property_tree_manager.SwitchToEffectNodeWithSynthesizedClip(
        effect, clip, layer.draws_content());
    int clip_id = property_tree_manager.EnsureCompositorClipNode(clip);

    // We need additional bookkeeping for backdrop-filter mask.
    if (effect.RequiresCompositingForBackdropFilterMask() &&
        effect.CcNodeId(g_s_property_tree_sequence_number) == effect_id) {
      CHECK(pending_layer.GetContentLayerClient());
      static_cast<cc::PictureLayer&>(layer).SetIsBackdropFilterMask(true);
      layer.SetElementId(effect.GetCompositorElementId());
      auto& effect_tree = host->property_trees()->effect_tree_mutable();
      auto* cc_node = effect_tree.Node(effect_id);
      effect_tree.Node(cc_node->parent_id)->backdrop_mask_element_id =
          effect.GetCompositorElementId();
    }

    int scroll_id =
        property_tree_manager.EnsureCompositorScrollAndTransformNode(
            ScrollTranslationStateForLayer(pending_layer), InfiniteIntRect());

    layer_list_builder.Add(&layer);

    layer.set_property_tree_sequence_number(
        root_layer_->property_tree_sequence_number());
    layer.SetTransformTreeIndex(transform_id);
    layer.SetScrollTreeIndex(scroll_id);
    layer.SetClipTreeIndex(clip_id);
    layer.SetEffectTreeIndex(effect_id);
    bool backface_hidden = transform.IsBackfaceHidden();
    layer.SetShouldCheckBackfaceVisibility(backface_hidden);

    if (layer.subtree_property_changed())
      root_layer_->SetNeedsCommit();
  }

  root_layer_->layer_tree_host()->RegisterSelection(layer_selection);

  property_tree_manager.Finalize();

  auto new_end = std::remove_if(
      synthesized_clip_cache_.begin(), synthesized_clip_cache_.end(),
      [](const auto& entry) { return !entry.in_use; });
  synthesized_clip_cache_.Shrink(
      static_cast<wtf_size_t>(new_end - synthesized_clip_cache_.begin()));

  // This should be done before
  // property_tree_manager.UpdateConditionalRenderSurfaceReasons() for which to
  // get property tree node ids from the layers.
  host->property_trees()->set_sequence_number(
      g_s_property_tree_sequence_number);

  auto layers = layer_list_builder.Finalize();
  property_tree_manager.UpdateConditionalRenderSurfaceReasons(layers);
  root_layer_->SetChildLayerList(std::move(layers));

  // Mark the property trees as having been rebuilt.
  host->property_trees()->set_needs_rebuild(false);
  host->property_trees()->ResetCachedData();
  previous_update_for_testing_ = PreviousUpdateType::kFull;

  UpdateDebugInfo();
  painted_scroll_translations_.clear();
  needs_update_ = false;

  g_s_property_tree_sequence_number++;

  // For information about |sequence_number|, see:
  // PaintPropertyNode::changed_sequence_number_|;
  for (auto& chunk : artifact.GetPaintChunks()) {
    chunk.properties.ClearChangedToRoot(g_s_property_tree_sequence_number);
    if (chunk.hit_test_data && chunk.hit_test_data->scroll_translation) {
      chunk.hit_test_data->scroll_translation->ClearChangedToRoot(
          g_s_property_tree_sequence_number);
    }
  }

  DVLOG(2) << "PaintArtifactCompositor::Update() done\n"
           << "Composited layers:\n"
           << GetLayersAsJSON(VLOG_IS_ON(3) ? 0xffffffff : 0)
                  ->ToPrettyJSONString()
                  .Utf8();
}

void PaintArtifactCompositor::UpdateRepaintedLayers(
    const PaintArtifact& repainted_artifact) {
  // |Update| should be used for full updates.
  DCHECK(!needs_update_);

#if DCHECK_IS_ON()
  // Any property tree state change should have caused a full update.
  for (const auto& chunk : repainted_artifact.GetPaintChunks()) {
    // If this fires, a property tree value has changed but we are missing a
    // call to |PaintArtifactCompositor::SetNeedsUpdate|.
    DCHECK(!chunk.properties.Unalias().ChangedToRoot(
        PaintPropertyChangeType::kChangedOnlyNonRerasterValues));
  }
#endif

  cc::LayerSelection layer_selection;
  for (auto& pending_layer : pending_layers_) {
    pending_layer.UpdateCompositedLayerForRepaint(repainted_artifact,
                                                  layer_selection);
  }
  root_layer_->layer_tree_host()->RegisterSelection(layer_selection);
  UpdateDebugInfo();

  previous_update_for_testing_ = PreviousUpdateType::kRepaint;
  needs_update_ = false;

  DVLOG(3) << "PaintArtifactCompositor::UpdateRepaintedLayers() done\n"
           << "Composited layers:\n"
           << GetLayersAsJSON(VLOG_IS_ON(3) ? 0xffffffff : 0)
                  ->ToPrettyJSONString()
                  .Utf8();
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
  if (!property_trees->scroll_tree().FindNodeFromElementId(element_id))
    return false;
  PropertyTreeManager::DirectlySetScrollOffset(*root_layer_->layer_tree_host(),
                                               element_id, scroll_offset);
  return true;
}

void PaintArtifactCompositor::DropCompositorScrollDeltaNextCommit(
    CompositorElementId element_id) {
  if (!root_layer_ || !root_layer_->layer_tree_host()) {
    return;
  }
  auto* property_trees = root_layer_->layer_tree_host()->property_trees();
  if (!property_trees->scroll_tree().FindNodeFromElementId(element_id)) {
    return;
  }
  PropertyTreeManager::DropCompositorScrollDeltaNextCommit(
      *root_layer_->layer_tree_host(), element_id);
}

uint32_t PaintArtifactCompositor::GetMainThreadScrollingReasons(
    const ScrollPaintPropertyNode& scroll) const {
  CHECK(root_layer_);
  if (!root_layer_->layer_tree_host()) {
    return 0;
  }
  return PropertyTreeManager::GetMainThreadScrollingReasons(
      *root_layer_->layer_tree_host(), scroll);
}

bool PaintArtifactCompositor::UsesCompositedScrolling(
    const ScrollPaintPropertyNode& scroll) const {
  CHECK(root_layer_);
  if (!root_layer_->layer_tree_host()) {
    return false;
  }
  return PropertyTreeManager::UsesCompositedScrolling(
      *root_layer_->layer_tree_host(), scroll);
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

void PaintArtifactCompositor::UpdateDebugInfo() const {
  if (!layer_debug_info_enabled_)
    return;

  PropertyTreeState previous_layer_state = PropertyTreeState::Root();
  for (const auto& pending_layer : pending_layers_) {
    cc::Layer& layer = pending_layer.CcLayer();
    RasterInvalidationTracking* tracking = nullptr;
    if (auto* client = pending_layer.GetContentLayerClient()) {
      tracking = client->GetRasterInvalidator().GetTracking();
    }
    cc::LayerDebugInfo& debug_info = layer.EnsureDebugInfo();
    debug_info.name = pending_layer.DebugName().Utf8();
    // GetCompositingReasons calls NeedsCompositedScrolling which is only
    // available during full update. In repaint-only update, the original
    // compositing reasons in debug_info will be kept.
    if (needs_update_) {
      auto compositing_reasons =
          GetCompositingReasons(pending_layer, previous_layer_state);
      debug_info.compositing_reasons =
          CompositingReason::Descriptions(compositing_reasons);
      debug_info.compositing_reason_ids =
          CompositingReason::ShortNames(compositing_reasons);
    }
    debug_info.owner_node_id = pending_layer.OwnerNodeId();

    if (RasterInvalidationTracking::IsTracingRasterInvalidations() &&
        tracking) {
      tracking->AddToLayerDebugInfo(debug_info);
      tracking->ClearInvalidations();
    }
    previous_layer_state = pending_layer.GetPropertyTreeState();
  }
}

// The returned compositing reasons are informative for tracing/debugging.
// Some are based on heuristics so are not fully accurate.
CompositingReasons PaintArtifactCompositor::GetCompositingReasons(
    const PendingLayer& layer,
    const PropertyTreeState& previous_layer_state) const {
  DCHECK(layer_debug_info_enabled_);
  DCHECK(needs_update_);

  if (layer.GetCompositingType() == PendingLayer::kScrollHitTestLayer) {
    return CompositingReason::kOverflowScrolling;
  }
  if (layer.Chunks().size() == 1 && layer.FirstPaintChunk().size() == 1) {
    switch (layer.FirstDisplayItem().GetType()) {
      case DisplayItem::kFixedAttachmentBackground:
        return CompositingReason::kFixedAttachmentBackground;
      case DisplayItem::kCaret:
        return CompositingReason::kCaret;
      case DisplayItem::kScrollbarHorizontal:
      case DisplayItem::kScrollbarVertical:
        return CompositingReason::kScrollbar;
      case DisplayItem::kForeignLayerCanvas:
        return CompositingReason::kCanvas;
      case DisplayItem::kForeignLayerDevToolsOverlay:
        return CompositingReason::kDevToolsOverlay;
      case DisplayItem::kForeignLayerPlugin:
        return CompositingReason::kPlugin;
      case DisplayItem::kForeignLayerVideo:
        return CompositingReason::kVideo;
      case DisplayItem::kForeignLayerRemoteFrame:
        return CompositingReason::kIFrame;
      case DisplayItem::kForeignLayerLinkHighlight:
        return CompositingReason::kLinkHighlight;
      case DisplayItem::kForeignLayerViewportScroll:
        return CompositingReason::kViewport;
      case DisplayItem::kForeignLayerViewportScrollbar:
        return CompositingReason::kScrollbar;
      case DisplayItem::kForeignLayerViewTransitionContent:
        return CompositingReason::kViewTransitionContent;
      default:
        // Will determine compositing reasons based on paint properties.
        break;
    }
  }

  CompositingReasons reasons = CompositingReason::kNone;
  const auto& transform = layer.GetPropertyTreeState().Transform();
  if (transform.IsBackfaceHidden() &&
      !previous_layer_state.Transform().IsBackfaceHidden()) {
    reasons = CompositingReason::kBackfaceVisibilityHidden;
  }
  if (layer.GetCompositingType() == PendingLayer::kOverlap) {
    return reasons == CompositingReason::kNone ? CompositingReason::kOverlap
                                               : reasons;
  }

  auto composited_ancestor = [this](const TransformPaintPropertyNode& transform)
      -> const TransformPaintPropertyNode* {
    const auto* ancestor = transform.NearestDirectlyCompositedAncestor();
    const auto& scroll_translation = transform.NearestScrollTranslationNode();
    if (NeedsCompositedScrolling(scroll_translation) &&
        (!ancestor || ancestor->IsAncestorOf(scroll_translation))) {
      return &scroll_translation;
    }
    return ancestor;
  };

  auto transform_compositing_reasons =
      [composited_ancestor](
          const TransformPaintPropertyNode& transform,
          const TransformPaintPropertyNode& previous) -> CompositingReasons {
    CompositingReasons reasons = CompositingReason::kNone;
    const auto* ancestor = composited_ancestor(transform);
    if (ancestor && ancestor != composited_ancestor(previous)) {
      reasons = ancestor->DirectCompositingReasonsForDebugging();
      if (ancestor->ScrollNode()) {
        reasons |= CompositingReason::kOverflowScrolling;
      }
    }
    return reasons;
  };

  auto clip_compositing_reasons =
      [transform_compositing_reasons](
          const ClipPaintPropertyNode& clip,
          const ClipPaintPropertyNode& previous) -> CompositingReasons {
    return transform_compositing_reasons(
        clip.LocalTransformSpace().Unalias(),
        previous.LocalTransformSpace().Unalias());
  };

  reasons |= transform_compositing_reasons(transform,
                                           previous_layer_state.Transform());
  const auto& effect = layer.GetPropertyTreeState().Effect();
  if (&effect != &previous_layer_state.Effect()) {
    reasons |= effect.DirectCompositingReasonsForDebugging();
    if (reasons == CompositingReason::kNone) {
      reasons = transform_compositing_reasons(
          effect.LocalTransformSpace().Unalias(),
          previous_layer_state.Effect().LocalTransformSpace().Unalias());
      if (reasons == CompositingReason::kNone && effect.OutputClip() &&
          previous_layer_state.Effect().OutputClip()) {
        reasons = clip_compositing_reasons(
            effect.OutputClip()->Unalias(),
            previous_layer_state.Effect().OutputClip()->Unalias());
      }
    }
  }
  if (reasons == CompositingReason::kNone) {
    reasons = clip_compositing_reasons(layer.GetPropertyTreeState().Clip(),
                                       previous_layer_state.Clip());
  }

  return reasons;
}

Vector<cc::Layer*> PaintArtifactCompositor::SynthesizedClipLayersForTesting()
    const {
  Vector<cc::Layer*> synthesized_clip_layers;
  for (const auto& entry : synthesized_clip_cache_) {
    synthesized_clip_layers.push_back(entry.synthesized_clip->Layer());
  }
  return synthesized_clip_layers;
}

size_t PaintArtifactCompositor::ApproximateUnsharedMemoryUsage() const {
  size_t result = sizeof(*this) + synthesized_clip_cache_.CapacityInBytes() +
                  pending_layers_.CapacityInBytes();

  for (auto& layer : pending_layers_) {
    if (auto* client = layer.GetContentLayerClient())
      result += client->ApproximateUnsharedMemoryUsage();
    size_t chunks_size = layer.Chunks().ApproximateUnsharedMemoryUsage();
    DCHECK_GE(chunks_size, sizeof(layer.Chunks()));
    result += chunks_size - sizeof(layer.Chunks());
  }

  return result;
}

bool PaintArtifactCompositor::SetScrollbarNeedsDisplay(
    CompositorElementId element_id) {
  DCHECK(root_layer_);
  CHECK(ScrollbarDisplayItem::IsScrollbarElementId(element_id));
  if (cc::LayerTreeHost* host = root_layer_->layer_tree_host()) {
    if (cc::Layer* layer = host->LayerByElementId(element_id)) {
      layer->SetNeedsDisplay();
      return true;
    }
  }
  // The scrollbar isn't currently composited.
  return false;
}

bool PaintArtifactCompositor::SetScrollbarSolidColor(
    CompositorElementId element_id,
    SkColor4f color) {
  DCHECK(root_layer_);
  CHECK(ScrollbarDisplayItem::IsScrollbarElementId(element_id));
  if (cc::LayerTreeHost* host = root_layer_->layer_tree_host()) {
    if (cc::Layer* layer = host->LayerByElementId(element_id)) {
      if (static_cast<cc::ScrollbarLayerBase*>(layer)
              ->GetScrollbarLayerType() ==
          cc::ScrollbarLayerBase::kSolidColor) {
        static_cast<cc::SolidColorScrollbarLayer*>(layer)->SetColor(color);
        return true;
      }
    }
  }
  // The scrollbar isn't currently composited.
  return false;
}

void LayerListBuilder::Add(scoped_refptr<cc::Layer> layer) {
  DCHECK(list_valid_);
  // Duplicated layers may happen when a foreign layer is fragmented.
  // TODO(wangxianzhu): Change this to DCHECK when we ensure all foreign layers
  // are monolithic (i.e. LayoutNGBlockFragmentation is fully launched).
  if (layer_ids_.insert(layer->id()).is_new_entry)
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

ContentLayerClientImpl* PaintArtifactCompositor::ContentLayerClientForTesting(
    wtf_size_t i) const {
  for (auto& pending_layer : pending_layers_) {
    if (auto* client = pending_layer.GetContentLayerClient()) {
      if (i == 0)
        return client;
      --i;
    }
  }
  return nullptr;
}

}  // namespace blink

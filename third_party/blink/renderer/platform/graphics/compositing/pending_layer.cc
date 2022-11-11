// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/pending_layer.h"

#include "base/containers/adapters.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

namespace {

// When possible, provides a clip rect that limits the visibility.
absl::optional<gfx::RectF> VisibilityLimit(const PropertyTreeState& state) {
  if (&state.Clip().LocalTransformSpace() == &state.Transform())
    return state.Clip().PaintClipRect().Rect();
  if (const auto* scroll = state.Transform().ScrollNode())
    return gfx::RectF(scroll->ContentsRect());
  return absl::nullopt;
}

bool IsCompositedScrollHitTest(const PaintChunk& chunk) {
  if (!chunk.hit_test_data)
    return false;
  const auto scroll_translation = chunk.hit_test_data->scroll_translation;
  return scroll_translation &&
         scroll_translation->HasDirectCompositingReasons();
}

bool IsCompositedScrollbar(const DisplayItem& item) {
  if (const auto* scrollbar = DynamicTo<ScrollbarDisplayItem>(item)) {
    const auto* scroll_translation = scrollbar->ScrollTranslation();
    return scroll_translation &&
           scroll_translation->HasDirectCompositingReasons();
  }
  return false;
}

// Snap |bounds| if within floating-point numeric limits of an integral rect.
void PreserveNearIntegralBounds(gfx::RectF& bounds) {
  constexpr float kTolerance = 1e-5f;
  if (std::abs(std::round(bounds.x()) - bounds.x()) <= kTolerance &&
      std::abs(std::round(bounds.y()) - bounds.y()) <= kTolerance &&
      std::abs(std::round(bounds.right()) - bounds.right()) <= kTolerance &&
      std::abs(std::round(bounds.bottom()) - bounds.bottom()) <= kTolerance) {
    bounds = gfx::RectF(gfx::ToRoundedRect(bounds));
  }
}

}  // anonymous namespace

PendingLayer::PendingLayer(const PaintChunkSubset& chunks,
                           const PaintChunkIterator& first_chunk)
    : PendingLayer(chunks, *first_chunk, first_chunk.IndexInPaintArtifact()) {}

PendingLayer::PendingLayer(const PaintChunkSubset& chunks,
                           const PaintChunk& first_chunk,
                           wtf_size_t first_chunk_index_in_paint_artifact)
    : bounds_(first_chunk.bounds),
      rect_known_to_be_opaque_(first_chunk.rect_known_to_be_opaque),
      has_text_(first_chunk.has_text),
      draws_content_(first_chunk.DrawsContent()),
      text_known_to_be_on_opaque_background_(
          first_chunk.text_known_to_be_on_opaque_background),
      chunks_(&chunks.GetPaintArtifact(), first_chunk_index_in_paint_artifact),
      property_tree_state_(
          first_chunk.properties.GetPropertyTreeState().Unalias()),
      compositing_type_(kOther) {
  DCHECK(!ChunkRequiresOwnLayer() || first_chunk.size() <= 1u);
  // Though text_known_to_be_on_opaque_background is only meaningful when
  // has_text is true, we expect text_known_to_be_on_opaque_background to be
  // true when !has_text to simplify code.
  DCHECK(has_text_ || text_known_to_be_on_opaque_background_);
  if (const absl::optional<gfx::RectF>& visibility_limit =
          VisibilityLimit(GetPropertyTreeState())) {
    bounds_.Intersect(*visibility_limit);
    if (bounds_.IsEmpty())
      draws_content_ = false;
  }

  if (IsCompositedScrollHitTest(first_chunk)) {
    compositing_type_ = kScrollHitTestLayer;
  } else if (first_chunk.size()) {
    const auto& first_display_item = FirstDisplayItem();
    if (first_display_item.IsForeignLayer())
      compositing_type_ = kForeignLayer;
    else if (IsCompositedScrollbar(first_display_item))
      compositing_type_ = kScrollbarLayer;
  }
}

gfx::Vector2dF PendingLayer::LayerOffset() const {
  // The solid color layer optimization is important for performance. Snapping
  // the location could make the solid color drawings not cover the entire
  // cc::Layer which would make the layer non-solid-color.
  if (IsSolidColor())
    return bounds_.OffsetFromOrigin();
  // Otherwise return integral offset to reduce chance of additional blurriness.
  return gfx::Vector2dF(gfx::ToFlooredVector2d(bounds_.OffsetFromOrigin()));
}

gfx::Size PendingLayer::LayerBounds() const {
  // Because solid color layers do not adjust their location (see:
  // |PendingLayer::LayerOffset()|), we only expand their size here.
  if (IsSolidColor())
    return gfx::ToCeiledSize(bounds_.size());
  return gfx::ToEnclosingRect(bounds_).size();
}

gfx::RectF PendingLayer::MapRectKnownToBeOpaque(
    const PropertyTreeState& new_state) const {
  if (rect_known_to_be_opaque_.IsEmpty())
    return gfx::RectF();

  FloatClipRect float_clip_rect(rect_known_to_be_opaque_);
  GeometryMapper::LocalToAncestorVisualRect(GetPropertyTreeState(), new_state,
                                            float_clip_rect);
  return float_clip_rect.IsTight() ? float_clip_rect.Rect() : gfx::RectF();
}

std::unique_ptr<JSONObject> PendingLayer::ToJSON() const {
  std::unique_ptr<JSONObject> result = std::make_unique<JSONObject>();
  result->SetArray("bounds", RectAsJSONArray(bounds_));
  result->SetArray("rect_known_to_be_opaque",
                   RectAsJSONArray(rect_known_to_be_opaque_));
  result->SetObject("property_tree_state", GetPropertyTreeState().ToJSON());
  result->SetArray("offset_of_decomposited_transforms",
                   VectorAsJSONArray(offset_of_decomposited_transforms_));
  result->SetArray("paint_chunks", chunks_.ToJSON());
  result->SetBoolean("draws_content", DrawsContent());
  return result;
}

std::ostream& operator<<(std::ostream& os, const PendingLayer& layer) {
  return os << layer.ToJSON()->ToPrettyJSONString().Utf8();
}

void PendingLayer::Upcast(const PropertyTreeState& new_state) {
  DCHECK(!ChunkRequiresOwnLayer());
  DCHECK_EQ(&new_state.Effect(),
            property_tree_state_.Effect().UnaliasedParent());

  if (property_tree_state_.Effect().BlendMode() != SkBlendMode::kSrcOver)
    has_decomposited_blend_mode_ = true;

  FloatClipRect float_clip_rect(bounds_);
  GeometryMapper::LocalToAncestorVisualRect(GetPropertyTreeState(), new_state,
                                            float_clip_rect);
  bounds_ = float_clip_rect.Rect();

  rect_known_to_be_opaque_ = MapRectKnownToBeOpaque(new_state);
  property_tree_state_ = new_state;
}

const PaintChunk& PendingLayer::FirstPaintChunk() const {
  return *chunks_.begin();
}

const DisplayItem& PendingLayer::FirstDisplayItem() const {
  return *chunks_.begin().DisplayItems().begin();
}

bool PendingLayer::Matches(const PendingLayer& old_pending_layer) const {
  if (ChunkRequiresOwnLayer() != old_pending_layer.ChunkRequiresOwnLayer())
    return false;
  if (ChunkRequiresOwnLayer() &&
      compositing_type_ != old_pending_layer.compositing_type_)
    return false;
  return FirstPaintChunk().Matches(old_pending_layer.FirstPaintChunk());
}

// We will only allow merging if
// merged_area - (home_area + guest_area) <= kMergeSparsityAreaTolerance
static constexpr float kMergeSparsityAreaTolerance = 10000;

bool PendingLayer::MergeInternal(const PendingLayer& guest,
                                 const PropertyTreeState& guest_state,
                                 bool prefers_lcd_text,
                                 bool dry_run) {
  DCHECK_EQ(&Chunks().GetPaintArtifact(), &guest.Chunks().GetPaintArtifact());
  if (ChunkRequiresOwnLayer() || guest.ChunkRequiresOwnLayer())
    return false;
  if (&GetPropertyTreeState().Effect() != &guest_state.Effect())
    return false;

  const absl::optional<PropertyTreeState>& merged_state =
      GetPropertyTreeState().CanUpcastWith(guest_state);
  if (!merged_state)
    return false;

  const absl::optional<gfx::RectF>& merged_visibility_limit =
      VisibilityLimit(*merged_state);

  // If the current bounds and known-to-be-opaque area already cover the entire
  // visible area of the merged state, and the current state is already equal
  // to the merged state, we can merge the guest immediately without needing to
  // update any bounds at all. This simple merge fast-path avoids the cost of
  // mapping the visual rects, below.
  if (!guest.has_decomposited_blend_mode_ && merged_visibility_limit &&
      *merged_visibility_limit == bounds_ &&
      merged_state == property_tree_state_ &&
      rect_known_to_be_opaque_.Contains(bounds_)) {
    if (!dry_run) {
      chunks_.Merge(guest.Chunks());
      draws_content_ |= guest.draws_content_;
      text_known_to_be_on_opaque_background_ = true;
      has_text_ |= guest.has_text_;
      change_of_decomposited_transforms_ =
          std::max(ChangeOfDecompositedTransforms(),
                   guest.ChangeOfDecompositedTransforms());
    }
    return true;
  }

  FloatClipRect new_home_bounds(bounds_);
  GeometryMapper::LocalToAncestorVisualRect(GetPropertyTreeState(),
                                            *merged_state, new_home_bounds);
  if (merged_visibility_limit)
    new_home_bounds.Rect().Intersect(*merged_visibility_limit);

  FloatClipRect new_guest_bounds(guest.bounds_);
  GeometryMapper::LocalToAncestorVisualRect(guest_state, *merged_state,
                                            new_guest_bounds);
  if (merged_visibility_limit)
    new_guest_bounds.Rect().Intersect(*merged_visibility_limit);

  gfx::RectF merged_bounds =
      gfx::UnionRects(new_home_bounds.Rect(), new_guest_bounds.Rect());
  float sum_area = new_home_bounds.Rect().size().GetArea() +
                   new_guest_bounds.Rect().size().GetArea();
  if (merged_bounds.size().GetArea() - sum_area > kMergeSparsityAreaTolerance)
    return false;

  // The guest's blend mode may make the merged layer not opaque.
  gfx::RectF merged_rect_known_to_be_opaque;
  bool merged_text_known_to_be_on_opaque_background = false;
  if (!guest.has_decomposited_blend_mode_) {
    merged_rect_known_to_be_opaque =
        gfx::MaximumCoveredRect(MapRectKnownToBeOpaque(*merged_state),
                                guest.MapRectKnownToBeOpaque(*merged_state));
    merged_text_known_to_be_on_opaque_background =
        text_known_to_be_on_opaque_background_;
    if (text_known_to_be_on_opaque_background_ !=
        guest.text_known_to_be_on_opaque_background_) {
      if (!text_known_to_be_on_opaque_background_) {
        if (merged_rect_known_to_be_opaque.Contains(new_home_bounds.Rect()))
          merged_text_known_to_be_on_opaque_background = true;
      } else if (!guest.text_known_to_be_on_opaque_background_) {
        if (!merged_rect_known_to_be_opaque.Contains(new_guest_bounds.Rect()))
          merged_text_known_to_be_on_opaque_background = false;
      }
    }
    // This is in the 'if' block because if guest.has_decomposited_blend_mode_
    // is true, we'll lose LCD text anyway due to the exotic blend mode
    // regardless of whether it's decomposited.
    if (prefers_lcd_text && !merged_text_known_to_be_on_opaque_background) {
      if (has_text_ && text_known_to_be_on_opaque_background_)
        return false;
      if (guest.has_text_ && guest.text_known_to_be_on_opaque_background_)
        return false;
    }
  }

  if (!dry_run) {
    chunks_.Merge(guest.Chunks());
    bounds_ = merged_bounds;
    property_tree_state_ = *merged_state;
    draws_content_ |= guest.draws_content_;
    rect_known_to_be_opaque_ = merged_rect_known_to_be_opaque;
    text_known_to_be_on_opaque_background_ =
        merged_text_known_to_be_on_opaque_background;
    has_text_ |= guest.has_text_;
    change_of_decomposited_transforms_ =
        std::max(ChangeOfDecompositedTransforms(),
                 guest.ChangeOfDecompositedTransforms());
    // GeometryMapper::LocalToAncestorVisualRect can introduce floating-point
    // error to the bounds. Integral bounds are important for reducing
    // blurriness (see: PendingLayer::LayerOffset) so preserve that here.
    PreserveNearIntegralBounds(bounds_);
    PreserveNearIntegralBounds(rect_known_to_be_opaque_);
  }
  return true;
}

const TransformPaintPropertyNode&
PendingLayer::ScrollTranslationForScrollHitTestLayer() const {
  DCHECK_EQ(GetCompositingType(), kScrollHitTestLayer);
  DCHECK_EQ(1u, Chunks().size());
  const auto& paint_chunk = FirstPaintChunk();
  DCHECK(paint_chunk.hit_test_data);
  DCHECK(paint_chunk.hit_test_data->scroll_translation);
  DCHECK(paint_chunk.hit_test_data->scroll_translation->ScrollNode());
  return *paint_chunk.hit_test_data->scroll_translation;
}

bool PendingLayer::PropertyTreeStateChanged(
    const PendingLayer* old_pending_layer) const {
  if (!old_pending_layer ||
      old_pending_layer->property_tree_state_ != property_tree_state_)
    return true;

  auto change = PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  if (change_of_decomposited_transforms_ >= change)
    return true;

  return GetPropertyTreeState().ChangedToRoot(change);
}

bool PendingLayer::MightOverlap(const PendingLayer& other) const {
  return GeometryMapper::MightOverlapForCompositing(
      bounds_, property_tree_state_.GetPropertyTreeState(), other.bounds_,
      other.property_tree_state_.GetPropertyTreeState());
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
            const TransformPaintPropertyNode& transform_node) {
          for (const auto* node = &transform_node; node && !node->IsRoot();
               node = node->UnaliasedParent()) {
            auto result = can_be_decomposited.insert(node, false);
            if (!result.is_new_entry) {
              if (!result.stored_value->value)
                break;
              result.stored_value->value = false;
            }
          }
        };

    // Add the transform and all transform parents to the map.
    for (const auto* node = &property_state.Transform();
         !node->IsRoot() && !can_be_decomposited.Contains(node);
         node = &node->Parent()->Unalias()) {
      if (!node->IsIdentityOr2dTranslation() || node->ScrollNode() ||
          node->HasDirectCompositingReasonsOtherThan3dTransform() ||
          !node->FlattensInheritedTransformSameAsParent() ||
          !node->BackfaceVisibilitySameAsParent()) {
        mark_not_decompositable(*node);
        break;
      }
      DCHECK(!node->GetStickyConstraint());
      DCHECK(!node->GetAnchorScrollContainersData());
      DCHECK(!node->IsAffectedByOuterViewportBoundsDelta());
      can_be_decomposited.insert(node, true);
    }

    // Add clips and effects, and their parents, that we haven't already seen.
    for (const auto* node = &property_state.Clip();
         !node->IsRoot() && !clips_and_effects_seen.Contains(node);
         node = &node->Parent()->Unalias()) {
      clips_and_effects_seen.insert(node);
      mark_not_decompositable(node->LocalTransformSpace().Unalias());
    }
    for (const auto* node = &property_state.Effect();
         !node->IsRoot() && !clips_and_effects_seen.Contains(node);
         node = &node->Parent()->Unalias()) {
      clips_and_effects_seen.insert(node);
      mark_not_decompositable(node->LocalTransformSpace().Unalias());
    }

    if (pending_layer.GetCompositingType() == kScrollHitTestLayer) {
      // The scroll translation node of a scroll hit test layer may not be
      // referenced by any pending layer's property tree state. Disallow
      // decomposition of it (and its ancestors).
      mark_not_decompositable(
          pending_layer.ScrollTranslationForScrollHitTestLayer());
    }
  }

  // Now, for any transform nodes that can be de-composited, re-map their
  // transform to point to the correct parent, and set the
  // offset_to_transform_parent.
  for (PendingLayer& pending_layer : pending_layers) {
    const auto* transform = &pending_layer.GetPropertyTreeState().Transform();
    while (!transform->IsRoot() && can_be_decomposited.at(transform)) {
      pending_layer.offset_of_decomposited_transforms_ +=
          transform->Get2dTranslation();
      pending_layer.change_of_decomposited_transforms_ =
          std::max(pending_layer.ChangeOfDecompositedTransforms(),
                   transform->NodeChanged());
      transform = &transform->Parent()->Unalias();
    }
    pending_layer.property_tree_state_.SetTransform(*transform);
    pending_layer.bounds_.Offset(
        pending_layer.OffsetOfDecompositedTransforms());
    pending_layer.rect_known_to_be_opaque_.Offset(
        pending_layer.OffsetOfDecompositedTransforms());
  }
}

void PendingLayer::UpdateForeignLayer() {
  DCHECK_EQ(compositing_type_, PendingLayer::kForeignLayer);

  // UpdateTouchActionRects() depends on the layer's offset, but when the
  // layer's offset changes, we do not call SetNeedsUpdate() (this is an
  // optimization because the update would only cause an extra commit) This is
  // only OK if the ForeignLayer doesn't have hit test data.
  DCHECK(!FirstPaintChunk().hit_test_data);
  const auto& foreign_layer_display_item =
      To<ForeignLayerDisplayItem>(FirstDisplayItem());

  gfx::Vector2dF layer_offset(
      foreign_layer_display_item.VisualRect().OffsetFromOrigin());
  cc_layer_ = foreign_layer_display_item.GetLayer();
  cc_layer_->SetOffsetToTransformParent(layer_offset +
                                        offset_of_decomposited_transforms_);
}

void PendingLayer::UpdateScrollHitTestLayer(PendingLayer* old_pending_layer) {
  DCHECK_EQ(compositing_type_, kScrollHitTestLayer);

  // We shouldn't decomposite scroll transform nodes.
  DCHECK_EQ(gfx::Vector2dF(), offset_of_decomposited_transforms_);

  const auto& scroll_node =
      *ScrollTranslationForScrollHitTestLayer().ScrollNode();

  DCHECK(!cc_layer_);
  if (old_pending_layer)
    cc_layer_ = std::move(old_pending_layer->cc_layer_);

  if (cc_layer_) {
    DCHECK_EQ(cc_layer_->element_id(), scroll_node.GetCompositorElementId());
  } else {
    cc_layer_ = cc::Layer::Create();
    cc_layer_->SetElementId(scroll_node.GetCompositorElementId());
    cc_layer_->SetHitTestable(true);
  }

  cc_layer_->SetOffsetToTransformParent(
      gfx::Vector2dF(scroll_node.ContainerRect().OffsetFromOrigin()));
  // TODO(pdr): The scroll layer's bounds are currently set to the clipped
  // container bounds but this does not include the border. We may want to
  // change this behavior to make non-composited and composited hit testing
  // match (see: crbug.com/753124). To do this, use
  // |scroll_hit_test->scroll_container_bounds|. Set the layer's bounds equal
  // to the container because the scroll layer does not scroll.
  cc_layer_->SetBounds(scroll_node.ContainerRect().size());

  if (scroll_node.NodeChanged() != PaintPropertyChangeType::kUnchanged) {
    cc_layer_->SetNeedsPushProperties();
    cc_layer_->SetNeedsCommit();
  }
}

void PendingLayer::UpdateScrollbarLayer(PendingLayer* old_pending_layer) {
  DCHECK_EQ(compositing_type_, kScrollbarLayer);

  const auto& item = FirstDisplayItem();
  DCHECK(item.IsScrollbar());

  const auto& scrollbar_item = To<ScrollbarDisplayItem>(item);
  scoped_refptr<cc::ScrollbarLayerBase> scrollbar_layer;
  if (old_pending_layer) {
    scrollbar_layer = static_cast<cc::ScrollbarLayerBase*>(
        std::move(old_pending_layer->cc_layer_).get());
  }

  scrollbar_layer = scrollbar_item.CreateOrReuseLayer(scrollbar_layer.get());
  scrollbar_layer->SetOffsetToTransformParent(
      scrollbar_layer->offset_to_transform_parent() +
      gfx::Vector2dF(offset_of_decomposited_transforms_));
  DCHECK(!cc_layer_);
  cc_layer_ = std::move(scrollbar_layer);
}

void PendingLayer::UpdateContentLayer(PendingLayer* old_pending_layer,
                                      bool tracks_raster_invalidations) {
  DCHECK(!ChunkRequiresOwnLayer());
  DCHECK(!content_layer_client_);
  if (old_pending_layer)
    content_layer_client_ = std::move(old_pending_layer->content_layer_client_);
  if (!content_layer_client_) {
    content_layer_client_ = std::make_unique<ContentLayerClientImpl>();
    content_layer_client_->GetRasterInvalidator().SetTracksRasterInvalidations(
        tracks_raster_invalidations);
  }
  content_layer_client_->UpdateCcPictureLayer(*this);
}

void PendingLayer::UpdateCompositedLayer(PendingLayer* old_pending_layer,
                                         cc::LayerSelection& layer_selection,
                                         bool tracks_raster_invalidations,
                                         cc::LayerTreeHost* layer_tree_host) {
  switch (compositing_type_) {
    case PendingLayer::kForeignLayer:
      UpdateForeignLayer();
      break;
    case PendingLayer::kScrollHitTestLayer:
      UpdateScrollHitTestLayer(old_pending_layer);
      break;
    case PendingLayer::kScrollbarLayer:
      UpdateScrollbarLayer(old_pending_layer);
      break;
    default:
      DCHECK(!ChunkRequiresOwnLayer());
      UpdateContentLayer(old_pending_layer, tracks_raster_invalidations);
      break;
  }

  UpdateLayerProperties();
  UpdateLayerSelection(layer_selection);

  cc::Layer& layer = CcLayer();
  layer.SetLayerTreeHost(layer_tree_host);
  if (!layer.subtree_property_changed() &&
      PropertyTreeStateChanged(old_pending_layer)) {
    layer.SetSubtreePropertyChanged();
  }
}

void PendingLayer::UpdateCompositedLayerForRepaint(
    scoped_refptr<const PaintArtifact> repainted_artifact,
    cc::LayerSelection& layer_selection) {
  // Essentially replace the paint chunks of the pending layer with the
  // repainted chunks in |repainted_artifact|. The pending layer's paint
  // chunks (a |PaintChunkSubset|) actually store indices to |PaintChunk|s
  // in a |PaintArtifact|. In repaint updates, chunks are not added,
  // removed, or re-ordered, so we can simply swap in a repainted
  // |PaintArtifact| instead of copying |PaintChunk|s individually.
  const PaintArtifact& old_artifact = Chunks().GetPaintArtifact();
  DCHECK_EQ(old_artifact.PaintChunks().size(),
            repainted_artifact->PaintChunks().size());
  SetPaintArtifact(std::move(repainted_artifact));

  bool chunks_unchanged = true;
  for (const auto& chunk : Chunks()) {
    if (!chunk.is_moved_from_cached_subsequence) {
      chunks_unchanged = false;
      break;
    }
  }

  if (!ChunkRequiresOwnLayer()) {
    DCHECK(content_layer_client_);
    // Checking |pending_layer_chunks_unchanged| is an optimization to avoid
    // the expensive call to |UpdateCcPictureLayer| when no repainting occurs
    // for this PendingLayer.
    if (chunks_unchanged) {
      // See RasterInvalidator::SetOldPaintArtifact() for the reason for this.
      content_layer_client_->GetRasterInvalidator().SetOldPaintArtifact(
          &Chunks().GetPaintArtifact());
    } else {
      content_layer_client_->UpdateCcPictureLayer(*this);
    }
  }

  if (!chunks_unchanged)
    UpdateLayerProperties();
  UpdateLayerSelection(layer_selection);
}

void PendingLayer::UpdateLayerProperties() {
  // Properties of foreign layers are managed by their owners.
  if (compositing_type_ == PendingLayer::kForeignLayer)
    return;
  PaintChunksToCcLayer::UpdateLayerProperties(CcLayer(), GetPropertyTreeState(),
                                              Chunks());
}

void PendingLayer::UpdateLayerSelection(cc::LayerSelection& layer_selection) {
  // Foreign layers cannot contain selection.
  if (compositing_type_ == PendingLayer::kForeignLayer)
    return;
  bool any_selection_was_painted = PaintChunksToCcLayer::UpdateLayerSelection(
      CcLayer(), GetPropertyTreeState(), Chunks(), layer_selection);
  if (any_selection_was_painted) {
    // If any selection was painted, but we didn't see the start or end bound
    // recorded, it could have been outside of the painting cull rect thus
    // invisible. Mark the bound as such if this is the case.
    if (layer_selection.start.type == gfx::SelectionBound::EMPTY) {
      layer_selection.start.type = gfx::SelectionBound::LEFT;
      layer_selection.start.hidden = true;
    }

    if (layer_selection.end.type == gfx::SelectionBound::EMPTY) {
      layer_selection.end.type = gfx::SelectionBound::RIGHT;
      layer_selection.end.hidden = true;
    }
  }
}

bool PendingLayer::IsSolidColor() const {
  if (Chunks().size() != 1)
    return false;
  const auto& items = chunks_.begin().DisplayItems();
  if (items.size() != 1)
    return false;
  auto* drawing = DynamicTo<DrawingDisplayItem>(*items.begin());
  return drawing && drawing->IsSolidColor();
}

// The heuristic for picking a checkerboarding color works as follows:
// - During paint, PaintChunker will look for background color display items,
//   and record the blending of background colors if the background is larger
//   than a ratio of the chunk bounds.
// - After layer allocation, the paint chunks assigned to a layer are examined
//   for a background color annotation.
// - The blending of background colors of chunks having background larger than
//   a ratio of the layer is set as the layer's background color.
SkColor4f PendingLayer::ComputeBackgroundColor() const {
  Vector<Color, 4> background_colors;
  float min_background_area =
      kMinBackgroundColorCoverageRatio * bounds_.width() * bounds_.height();
  for (auto it = chunks_.end(); it != chunks_.begin();) {
    const auto& chunk = *(--it);
    if (chunk.background_color == Color::kTransparent)
      continue;
    if (chunk.background_color_area >= min_background_area) {
      Color chunk_background_color = chunk.background_color;
      const auto& chunk_effect = chunk.properties.Effect().Unalias();
      if (&chunk_effect != &property_tree_state_.Effect()) {
        if (chunk_effect.UnaliasedParent() != &property_tree_state_.Effect() ||
            !chunk_effect.IsOpacityOnly()) {
          continue;
        }
        chunk_background_color =
            chunk_background_color.CombineWithAlpha(chunk_effect.Opacity());
      }
      background_colors.push_back(chunk_background_color);
      if (!chunk_background_color.HasAlpha()) {
        // If this color is opaque, blending it with subsequent colors will have
        // no effect.
        break;
      }
    }
  }

  Color background_color;
  for (Color color : base::Reversed(background_colors))
    background_color = background_color.Blend(color);
  return SkColor4f::FromColor(background_color.Rgb());
}

}  // namespace blink

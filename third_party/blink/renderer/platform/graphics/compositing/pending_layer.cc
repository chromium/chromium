// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/pending_layer.h"

#include "base/containers/adapters.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/layers/solid_color_layer.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

namespace {

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

PendingLayer::PendingLayer(scoped_refptr<const PaintArtifact> artifact,
                           const PaintChunk& first_chunk)
    : bounds_(first_chunk.bounds),
      rect_known_to_be_opaque_(first_chunk.rect_known_to_be_opaque),
      has_text_(first_chunk.has_text),
      draws_content_(first_chunk.DrawsContent()),
      text_known_to_be_on_opaque_background_(
          first_chunk.text_known_to_be_on_opaque_background),
      is_solid_color_(first_chunk.background_color.is_solid_color),
      chunks_(std::move(artifact), first_chunk),
      property_tree_state_(
          first_chunk.properties.GetPropertyTreeState().Unalias()) {
  DCHECK(!ChunkRequiresOwnLayer() || first_chunk.size() <= 1u);
  // Though text_known_to_be_on_opaque_background is only meaningful when
  // has_text is true, we expect text_known_to_be_on_opaque_background to be
  // true when !has_text to simplify code.
  DCHECK(has_text_ || text_known_to_be_on_opaque_background_);
  if (const absl::optional<gfx::RectF>& visibility_limit =
          GeometryMapper::VisibilityLimit(GetPropertyTreeState())) {
    bounds_.Intersect(*visibility_limit);
    if (bounds_.IsEmpty()) {
      draws_content_ = false;
    }
  }
  rect_known_to_be_opaque_.Intersect(bounds_);
}

gfx::Vector2dF PendingLayer::LayerOffset() const {
  // The solid color layer optimization is important for performance. Snapping
  // the location could make the solid color drawings not cover the entire
  // cc::Layer which would make the layer non-solid-color.
  if (IsSolidColor()) {
    return bounds_.OffsetFromOrigin();
  }
  // Otherwise return integral offset to reduce chance of additional blurriness.
  // TODO(crbug.com/1414915): This expansion may harm performance because
  // opaque layers becomes non-opaque. We can avoid this when we support
  // subpixel raster translation for render surfaces. We have already supported
  // that for cc::PictureLayerImpls.
  return gfx::Vector2dF(gfx::ToFlooredVector2d(bounds_.OffsetFromOrigin()));
}

gfx::Size PendingLayer::LayerBounds() const {
  // Because solid color layers do not adjust their location (see:
  // |PendingLayer::LayerOffset()|), we only expand their size here.
  if (IsSolidColor()) {
    return gfx::ToCeiledSize(bounds_.size());
  }
  return gfx::ToEnclosingRect(bounds_).size();
}

gfx::RectF PendingLayer::MapRectKnownToBeOpaque(
    const PropertyTreeState& new_state,
    const FloatClipRect& mapped_layer_bounds) const {
  if (!mapped_layer_bounds.IsTight()) {
    return gfx::RectF();
  }
  if (rect_known_to_be_opaque_.IsEmpty()) {
    return gfx::RectF();
  }
  if (rect_known_to_be_opaque_ == bounds_) {
    return mapped_layer_bounds.Rect();
  }
  FloatClipRect float_clip_rect(rect_known_to_be_opaque_);
  GeometryMapper::LocalToAncestorVisualRect(GetPropertyTreeState(), new_state,
                                            float_clip_rect);
  float_clip_rect.Rect().Intersect(mapped_layer_bounds.Rect());
  DCHECK(float_clip_rect.IsTight());
  return float_clip_rect.Rect();
}

std::unique_ptr<JSONObject> PendingLayer::ToJSON() const {
  std::unique_ptr<JSONObject> result = std::make_unique<JSONObject>();
  result->SetString("debug_name", DebugName());
  result->SetArray("bounds", RectAsJSONArray(bounds_));
  result->SetArray("rect_known_to_be_opaque",
                   RectAsJSONArray(rect_known_to_be_opaque_));
  result->SetBoolean("text_known_to_be_on_opaque_background",
                     text_known_to_be_on_opaque_background_);
  result->SetString("property_tree_state", GetPropertyTreeState().ToString());
  result->SetArray("offset_of_decomposited_transforms",
                   VectorAsJSONArray(offset_of_decomposited_transforms_));
  result->SetArray("paint_chunks", chunks_.ToJSON());
  result->SetBoolean("draws_content", DrawsContent());
  result->SetBoolean("is_solid_color", is_solid_color_);
  return result;
}

String PendingLayer::DebugName() const {
  return Chunks().GetPaintArtifact().ClientDebugName(
      FirstPaintChunk().id.client_id);
}

DOMNodeId PendingLayer::OwnerNodeId() const {
  return Chunks().GetPaintArtifact().ClientOwnerNodeId(
      FirstPaintChunk().id.client_id);
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
  // The order of the following two statements is important because
  // MapRectKnownToBeOpaque() needs to know the original bounds_.
  rect_known_to_be_opaque_ = MapRectKnownToBeOpaque(new_state, float_clip_rect);
  bounds_ = float_clip_rect.Rect();

  property_tree_state_ = new_state;
  is_solid_color_ = false;
}

const PaintChunk& PendingLayer::FirstPaintChunk() const {
  return chunks_[0];
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

bool PendingLayer::CanMerge(
    const PendingLayer& guest,
    LCDTextPreference lcd_text_preference,
    IsCompositedScrollFunction is_composited_scroll,
    gfx::RectF& merged_bounds,
    PropertyTreeState& merged_state,
    gfx::RectF& merged_rect_known_to_be_opaque,
    bool& merged_text_known_to_be_on_opaque_background) const {
  absl::optional<PropertyTreeState> optional_merged_state =
      CanUpcastWith(guest, guest.GetPropertyTreeState(), is_composited_scroll);
  if (!optional_merged_state) {
    return false;
  }

  merged_state = *optional_merged_state;
  const absl::optional<gfx::RectF>& merged_visibility_limit =
      GeometryMapper::VisibilityLimit(merged_state);

  // If the current bounds and known-to-be-opaque area already cover the entire
  // visible area of the merged state, and the current state is already equal
  // to the merged state, we can merge the guest immediately without needing to
  // update any bounds at all. This simple merge fast-path avoids the cost of
  // mapping the visual rects, below.
  if (!guest.has_decomposited_blend_mode_ && merged_visibility_limit &&
      *merged_visibility_limit == bounds_ &&
      merged_state == property_tree_state_ &&
      rect_known_to_be_opaque_ == bounds_) {
    merged_bounds = merged_rect_known_to_be_opaque = bounds_;
    merged_text_known_to_be_on_opaque_background = true;
    return true;
  }

  FloatClipRect new_home_bounds(bounds_);
  GeometryMapper::LocalToAncestorVisualRect(GetPropertyTreeState(),
                                            merged_state, new_home_bounds);
  if (merged_visibility_limit) {
    new_home_bounds.Rect().Intersect(*merged_visibility_limit);
  }
  FloatClipRect new_guest_bounds(guest.bounds_);
  GeometryMapper::LocalToAncestorVisualRect(guest.GetPropertyTreeState(),
                                            merged_state, new_guest_bounds);
  if (merged_visibility_limit) {
    new_guest_bounds.Rect().Intersect(*merged_visibility_limit);
  }

  merged_bounds =
      gfx::UnionRects(new_home_bounds.Rect(), new_guest_bounds.Rect());

  // If guest.has_decomposited_blend_mode_ is true, this function must merge
  // unconditionally and return because the decomposited blend mode requires
  // the merge. See PaintArtifactCompositor::DecompositeEffect().
  // Also in the case, the conditions returning false below are unlikely to
  // apply because
  // - the src and dest layers are unlikely to be far away (sparse),
  // - the blend mode may make the merged layer not opaque,
  // - LCD text will be disabled with exotic blend mode.
  if (!guest.has_decomposited_blend_mode_) {
    float sum_area = new_home_bounds.Rect().size().GetArea() +
                     new_guest_bounds.Rect().size().GetArea();
    if (merged_bounds.size().GetArea() - sum_area >
        kMergeSparsityAreaTolerance) {
      return false;
    }

    gfx::RectF home_rect_known_to_be_opaque =
        MapRectKnownToBeOpaque(merged_state, new_home_bounds);
    gfx::RectF guest_rect_known_to_be_opaque =
        guest.MapRectKnownToBeOpaque(merged_state, new_guest_bounds);
    merged_rect_known_to_be_opaque = gfx::MaximumCoveredRect(
        home_rect_known_to_be_opaque, guest_rect_known_to_be_opaque);
    merged_text_known_to_be_on_opaque_background =
        text_known_to_be_on_opaque_background_;
    if (text_known_to_be_on_opaque_background_ !=
        guest.text_known_to_be_on_opaque_background_) {
      if (!text_known_to_be_on_opaque_background_) {
        if (merged_rect_known_to_be_opaque.Contains(new_home_bounds.Rect())) {
          merged_text_known_to_be_on_opaque_background = true;
        }
      } else if (!guest.text_known_to_be_on_opaque_background_) {
        if (!merged_rect_known_to_be_opaque.Contains(new_guest_bounds.Rect())) {
          merged_text_known_to_be_on_opaque_background = false;
        }
      }
    }
    if (lcd_text_preference == LCDTextPreference::kStronglyPreferred &&
        !merged_text_known_to_be_on_opaque_background) {
      if (has_text_ && text_known_to_be_on_opaque_background_) {
        return false;
      }
      if (guest.has_text_ && guest.text_known_to_be_on_opaque_background_) {
        return false;
      }
    }
  }

  // GeometryMapper::LocalToAncestorVisualRect can introduce floating-point
  // error to the bounds. Integral bounds are important for reducing
  // blurriness (see: PendingLayer::LayerOffset) so preserve that here.
  PreserveNearIntegralBounds(merged_bounds);
  PreserveNearIntegralBounds(merged_rect_known_to_be_opaque);
  return true;
}

bool PendingLayer::Merge(const PendingLayer& guest,
                         LCDTextPreference lcd_text_preference,
                         IsCompositedScrollFunction is_composited_scroll) {
  gfx::RectF merged_bounds;
  PropertyTreeState merged_state = PropertyTreeState::Uninitialized();
  gfx::RectF merged_rect_known_to_be_opaque;
  bool merged_text_known_to_be_on_opaque_background = false;

  if (!CanMerge(guest, lcd_text_preference, is_composited_scroll, merged_bounds,
                merged_state, merged_rect_known_to_be_opaque,
                merged_text_known_to_be_on_opaque_background)) {
    return false;
  }

  chunks_.Merge(guest.Chunks());
  bounds_ = merged_bounds;
  property_tree_state_ = merged_state;
  draws_content_ |= guest.draws_content_;
  rect_known_to_be_opaque_ = merged_rect_known_to_be_opaque;
  text_known_to_be_on_opaque_background_ =
      merged_text_known_to_be_on_opaque_background;
  has_text_ |= guest.has_text_;
  is_solid_color_ = false;
  change_of_decomposited_transforms_ = std::max(
      ChangeOfDecompositedTransforms(), guest.ChangeOfDecompositedTransforms());
  return true;
}

absl::optional<PropertyTreeState> PendingLayer::CanUpcastWith(
    const PendingLayer& guest,
    const PropertyTreeState& guest_state,
    IsCompositedScrollFunction is_composited_scroll) const {
  DCHECK_EQ(&Chunks().GetPaintArtifact(), &guest.Chunks().GetPaintArtifact());
  if (ChunkRequiresOwnLayer() || guest.ChunkRequiresOwnLayer()) {
    return absl::nullopt;
  }
  if (&GetPropertyTreeState().Effect() != &guest_state.Effect()) {
    return absl::nullopt;
  }
  return GetPropertyTreeState().CanUpcastWith(guest_state,
                                              is_composited_scroll);
}

bool PendingLayer::CanMergeWithDecompositedBlendMode(
    const PendingLayer& guest,
    const PropertyTreeState& upcast_state,
    IsCompositedScrollFunction is_composited_scroll) const {
  return CanUpcastWith(guest, upcast_state, is_composited_scroll).has_value();
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
      DCHECK(!node->GetAnchorPositionScrollersData());
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
  DCHECK(!cc_layer_);
  DCHECK(!content_layer_client_);
  DCHECK(!UsesSolidColorLayer());
  if (old_pending_layer) {
    content_layer_client_ = std::move(old_pending_layer->content_layer_client_);
  }
  if (!content_layer_client_) {
    content_layer_client_ = std::make_unique<ContentLayerClientImpl>();
    content_layer_client_->GetRasterInvalidator().SetTracksRasterInvalidations(
        tracks_raster_invalidations);
  }
  content_layer_client_->UpdateCcPictureLayer(*this);
}

void PendingLayer::UpdateSolidColorLayer(PendingLayer* old_pending_layer) {
  DCHECK(!ChunkRequiresOwnLayer());
  DCHECK(!cc_layer_);
  DCHECK(!content_layer_client_);
  DCHECK(UsesSolidColorLayer());
  if (old_pending_layer) {
    cc_layer_ = std::move(old_pending_layer->cc_layer_);
  }
  if (!cc_layer_) {
    cc_layer_ = cc::SolidColorLayer::Create();
  }
  cc_layer_->SetOffsetToTransformParent(LayerOffset());
  cc_layer_->SetBounds(LayerBounds());
  cc_layer_->SetHitTestable(true);
  DCHECK(FirstPaintChunk().background_color.is_solid_color);
  cc_layer_->SetBackgroundColor(FirstPaintChunk().background_color.color);
  cc_layer_->SetIsDrawable(draws_content_);
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
      if (UsesSolidColorLayer()) {
        UpdateSolidColorLayer(old_pending_layer);
      } else {
        UpdateContentLayer(old_pending_layer, tracks_raster_invalidations);
      }
      break;
  }

  UpdateLayerProperties(layer_selection, /*selection_only=*/false);

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
    if (UsesSolidColorLayer()) {
      DCHECK(cc_layer_);
      if (!chunks_unchanged) {
        DCHECK(FirstPaintChunk().background_color.is_solid_color);
        cc_layer_->SetBackgroundColor(FirstPaintChunk().background_color.color);
      }
    } else {
      DCHECK(content_layer_client_);
      // Checking |chunks_unchanged| is an optimization to avoid the expensive
      // call to |UpdateCcPictureLayer| when no repainting occurs for this
      // PendingLayer.
      if (chunks_unchanged) {
        // See RasterInvalidator::SetOldPaintArtifact() for the reason for this.
        content_layer_client_->GetRasterInvalidator().SetOldPaintArtifact(
            &Chunks().GetPaintArtifact());
      } else {
        content_layer_client_->UpdateCcPictureLayer(*this);
      }
    }
  }

  UpdateLayerProperties(layer_selection, chunks_unchanged);
}

void PendingLayer::UpdateLayerProperties(cc::LayerSelection& layer_selection,
                                         bool selection_only) {
  // Properties of foreign layers are managed by their owners.
  if (compositing_type_ == PendingLayer::kForeignLayer) {
    return;
  }
  PaintChunksToCcLayer::UpdateLayerProperties(CcLayer(), GetPropertyTreeState(),
                                              Chunks(), layer_selection,
                                              selection_only);
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
  Vector<SkColor4f, 4> background_colors;
  float min_background_area =
      kMinBackgroundColorCoverageRatio * bounds_.width() * bounds_.height();
  for (auto it = chunks_.end(); it != chunks_.begin();) {
    const auto& chunk = *(--it);
    if (chunk.background_color.color.fA == 0.0f) {
      continue;
    }
    if (chunk.background_color.area >= min_background_area) {
      SkColor4f chunk_background_color = chunk.background_color.color;
      const auto& chunk_effect = chunk.properties.Effect().Unalias();
      if (&chunk_effect != &property_tree_state_.Effect()) {
        if (chunk_effect.UnaliasedParent() != &property_tree_state_.Effect() ||
            !chunk_effect.IsOpacityOnly()) {
          continue;
        }
        chunk_background_color.fA *= chunk_effect.Opacity();
      }
      background_colors.push_back(chunk_background_color);
      if (chunk_background_color.isOpaque()) {
        // If this color is opaque, blending it with subsequent colors will have
        // no effect.
        break;
      }
    }
  }

  if (background_colors.empty()) {
    return SkColors::kTransparent;
  }
  SkColor4f background_color = background_colors.back();
  background_colors.pop_back();

  for (const SkColor4f& color : base::Reversed(background_colors)) {
    background_color = SkColor4f::FromColor(color_utils::GetResultingPaintColor(
        color.toSkColor(), background_color.toSkColor()));
  }
  return background_color;
}

}  // namespace blink

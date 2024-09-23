// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

namespace {

bool Keeps2dAxisAlignmentStatus(const gfx::Transform& a,
                                const gfx::Transform& b) {
  if (a.Preserves2dAxisAlignment() && b.Preserves2dAxisAlignment())
    return true;

  return (a.InverseOrIdentity() * b).Preserves2dAxisAlignment();
}

}  // anonymous namespace

PaintPropertyChangeType
TransformPaintPropertyNode::State::ComputeTransformChange(
    const TransformAndOrigin& other,
    const AnimationState& animation_state) const {
  bool matrix_changed = transform_and_origin.matrix != other.matrix;
  bool origin_changed = transform_and_origin.origin != other.origin;
  bool transform_changed = matrix_changed || origin_changed;

  if (!transform_changed)
    return PaintPropertyChangeType::kUnchanged;

  if (animation_state.is_running_animation_on_compositor) {
    // The compositor handles transform change automatically during composited
    // transform animation, but it doesn't handle origin changes (which can
    // still be treated as simple, and can skip the 2d-axis-alignment check
    // because PropertyTreeManager knows if the whole animation is 2d-axis
    // aligned when the animation starts).
    return origin_changed
               ? PaintPropertyChangeType::kChangedOnlySimpleValues
               : PaintPropertyChangeType::kChangedOnlyCompositedValues;
  }

  if ((direct_compositing_reasons & CompositingReason::kStickyPosition) ||
      (direct_compositing_reasons & CompositingReason::kAnchorPosition)) {
    // The compositor handles sticky offset changes and anchor position
    // translation offset changes automatically.
    DCHECK(transform_and_origin.matrix.Preserves2dAxisAlignment());
    DCHECK(other.matrix.Preserves2dAxisAlignment());
    return PaintPropertyChangeType::kChangedOnlyCompositedValues;
  }

  if (matrix_changed &&
      !Keeps2dAxisAlignmentStatus(transform_and_origin.matrix, other.matrix)) {
    // An additional cc::EffectNode may be required if
    // blink::TransformPaintPropertyNode is not axis-aligned (see:
    // PropertyTreeManager::SyntheticEffectType). Changes to axis alignment
    // are therefore treated as non-simple. We do not need to check origin
    // because axis alignment is not affected by transform origin.
    return PaintPropertyChangeType::kChangedOnlyValues;
  }

  return PaintPropertyChangeType::kChangedOnlySimpleValues;
}

PaintPropertyChangeType TransformPaintPropertyNode::State::ComputeChange(
    const State& other,
    const AnimationState& animation_state) const {
  // Whether or not a node is considered a frame root should be invariant.
  DCHECK_EQ(is_frame_paint_offset_translation,
            other.is_frame_paint_offset_translation);

  // Changes other than compositing reason and the transform are not simple.
  if (flattens_inherited_transform != other.flattens_inherited_transform ||
      in_subtree_of_page_scale != other.in_subtree_of_page_scale ||
      animation_is_axis_aligned != other.animation_is_axis_aligned ||
      is_frame_paint_offset_translation !=
          other.is_frame_paint_offset_translation ||
      is_for_svg_child != other.is_for_svg_child ||
      backface_visibility != other.backface_visibility ||
      rendering_context_id != other.rendering_context_id ||
      compositor_element_id != other.compositor_element_id ||
      // This change affects cull rect expansion for scrolling contents.
      UsesCompositedScrolling() != other.UsesCompositedScrolling() ||
      // This change affects cull rect expansion for the element itself.
      RequiresCullRectExpansion() != other.RequiresCullRectExpansion() ||
      scroll != other.scroll ||
      scroll_translation_for_fixed != other.scroll_translation_for_fixed ||
      !base::ValuesEquivalent(sticky_constraint, other.sticky_constraint) ||
      !base::ValuesEquivalent(anchor_position_scroll_data,
                              other.anchor_position_scroll_data) ||
      visible_frame_element_id != other.visible_frame_element_id) {
    return PaintPropertyChangeType::kChangedOnlyValues;
  }

  auto change =
      ComputeTransformChange(other.transform_and_origin, animation_state);

  bool non_reraster_values_changed =
      direct_compositing_reasons != other.direct_compositing_reasons;
  if (non_reraster_values_changed) {
    // Both transform change and non-reraster change is upgraded to value
    // change to avoid loss of non-reraster change when PaintPropertyTreeBuilder
    // downgrades kChangedOnlySimpleValues to kChangedOnlyCompositedValues
    // after a successful direct update.
    return change != PaintPropertyChangeType::kUnchanged
               ? PaintPropertyChangeType::kChangedOnlyValues
               : PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  }

  return change;
}

void TransformPaintPropertyNode::State::Trace(Visitor* visitor) const {
  visitor->Trace(scroll);
  visitor->Trace(scroll_translation_for_fixed);
}

TransformPaintPropertyNode::TransformPaintPropertyNode(RootTag)
    : TransformPaintPropertyNodeOrAlias(kRoot),
      state_{.scroll = &ScrollPaintPropertyNode::Root(),
             .in_subtree_of_page_scale = false} {}

const TransformPaintPropertyNode& TransformPaintPropertyNode::Root() {
  DEFINE_STATIC_LOCAL(
      Persistent<TransformPaintPropertyNode>, root,
      (MakeGarbageCollected<TransformPaintPropertyNode>(kRoot)));
  return *root;
}

PaintPropertyChangeType
TransformPaintPropertyNode::DirectlyUpdateTransformAndOrigin(
    TransformAndOrigin&& transform_and_origin,
    const AnimationState& animation_state) {
  auto change =
      state_.ComputeTransformChange(transform_and_origin, animation_state);
  state_.transform_and_origin = std::move(transform_and_origin);
  if (change != PaintPropertyChangeType::kUnchanged)
    AddChanged(change);
  return change;
}

bool TransformPaintPropertyNodeOrAlias::Changed(
    PaintPropertyChangeType change,
    const TransformPaintPropertyNodeOrAlias& relative_to_node) const {
  for (const auto* node = this; node; node = node->Parent()) {
    if (node == &relative_to_node)
      return false;
    if (node->NodeChanged() >= change)
      return true;
  }

  // |this| is not a descendant of |relative_to_node|. We have seen no changed
  // flag from |this| to the root. Now check |relative_to_node| to the root.
  return relative_to_node.Changed(change, TransformPaintPropertyNode::Root());
}

void TransformPaintPropertyNodeOrAlias::ClearChangedToRoot(
    int sequence_number) const {
  for (auto* n = this; n && n->ChangedSequenceNumber() != sequence_number;
       n = n->Parent()) {
    n->ClearChanged(sequence_number);
    if (n->IsParentAlias()) {
      continue;
    }
    if (const auto* scroll =
            static_cast<const TransformPaintPropertyNode*>(n)->ScrollNode()) {
      scroll->ClearChangedToRoot(sequence_number);
    }
  }
}

std::unique_ptr<JSONObject> TransformPaintPropertyNode::ToJSON() const {
  auto json = TransformPaintPropertyNodeOrAlias::ToJSON();
  if (IsIdentityOr2dTranslation()) {
    if (!Get2dTranslation().IsZero())
      json->SetString("translation2d", String(Get2dTranslation().ToString()));
  } else {
    String matrix(Matrix().ToDecomposedString());
    if (matrix.EndsWith("\n"))
      matrix = matrix.Left(matrix.length() - 1);
    json->SetString("matrix", matrix.Replace("\n", ", "));
    json->SetString("origin", String(Origin().ToString()));
  }
  if (!state_.flattens_inherited_transform) {
    json->SetBoolean("flattensInheritedTransform", false);
  }
  if (!state_.in_subtree_of_page_scale) {
    json->SetBoolean("in_subtree_of_page_scale", false);
  }
  if (state_.backface_visibility != BackfaceVisibility::kInherited) {
    json->SetString("backface",
                    state_.backface_visibility == BackfaceVisibility::kVisible
                        ? "visible"
                        : "hidden");
  }
  if (state_.rendering_context_id) {
    json->SetString("renderingContextId",
                    String::Format("%x", state_.rendering_context_id));
  }
  if (state_.direct_compositing_reasons != CompositingReason::kNone) {
    json->SetString(
        "directCompositingReasons",
        CompositingReason::ToString(state_.direct_compositing_reasons));
  }
  if (state_.compositor_element_id) {
    json->SetString("compositorElementId",
                    String(state_.compositor_element_id.ToString()));
  }
  if (state_.scroll)
    json->SetString("scroll", String::Format("%p", state_.scroll.Get()));

  if (state_.scroll_translation_for_fixed) {
    json->SetString(
        "scroll_translation_for_fixed",
        String::Format("%p", state_.scroll_translation_for_fixed.Get()));
  }
  return json;
}

}  // namespace blink

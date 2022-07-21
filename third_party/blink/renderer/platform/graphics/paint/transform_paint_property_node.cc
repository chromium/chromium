// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

PaintPropertyChangeType TransformPaintPropertyNode::State::ComputeChange(
    const State& other,
    const AnimationState& animation_state) const {
  // Whether or not a node is considered a frame root should be invariant.
  DCHECK_EQ(flags.is_frame_paint_offset_translation,
            other.flags.is_frame_paint_offset_translation);

  // Changes other than compositing reason and the transform are not simple.
  if (flags.flattens_inherited_transform !=
          other.flags.flattens_inherited_transform ||
      flags.in_subtree_of_page_scale != other.flags.in_subtree_of_page_scale ||
      flags.animation_is_axis_aligned !=
          other.flags.animation_is_axis_aligned ||
      flags.delegates_to_parent_for_backface !=
          other.flags.delegates_to_parent_for_backface ||
      flags.is_frame_paint_offset_translation !=
          other.flags.is_frame_paint_offset_translation ||
      flags.is_for_svg_child != other.flags.is_for_svg_child ||
      backface_visibility != other.backface_visibility ||
      rendering_context_id != other.rendering_context_id ||
      compositor_element_id != other.compositor_element_id ||
      scroll != other.scroll ||
      scroll_translation_for_fixed != other.scroll_translation_for_fixed ||
      !StickyConstraintEquals(other) ||
      anchor_scroll_container != other.anchor_scroll_container ||
      visible_frame_element_id != other.visible_frame_element_id) {
    return PaintPropertyChangeType::kChangedOnlyValues;
  }

  bool matrix_changed =
      !transform_and_origin.TransformEquals(other.transform_and_origin);
  bool origin_changed =
      transform_and_origin.Origin() != other.transform_and_origin.Origin();
  bool transform_changed = matrix_changed || origin_changed;

  bool non_reraster_values_changed =
      direct_compositing_reasons != other.direct_compositing_reasons;
  if (non_reraster_values_changed) {
    // Both transform change and non-reraster change is upgraded to value
    // change to avoid loss of non-reraster change when PaintPropertyTreeBuilder
    // downgrades kChangedOnlySimpleValues to kChangedOnlyCompositedValues
    // after a successful direct update.
    return transform_changed
               ? PaintPropertyChangeType::kChangedOnlyValues
               : PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  }

  if (!transform_changed)
    return PaintPropertyChangeType::kUnchanged;

  // Now we have transform change only.
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

  if (RuntimeEnabledFeatures::ScrollUpdateOptimizationsEnabled() &&
      direct_compositing_reasons & CompositingReason::kStickyPosition) {
    // The compositor handles sticky offset changes automatically.
    DCHECK(transform_and_origin.ChangePreserves2dAxisAlignment(
        other.transform_and_origin));
    return PaintPropertyChangeType::kChangedOnlyCompositedValues;
  }

  if (matrix_changed && !transform_and_origin.ChangePreserves2dAxisAlignment(
                            other.transform_and_origin)) {
    // An additional cc::EffectNode may be required if
    // blink::TransformPaintPropertyNode is not axis-aligned (see:
    // PropertyTreeManager::SyntheticEffectType). Changes to axis alignment
    // are therefore treated as non-simple. We do not need to check origin
    // because axis alignment is not affected by transform origin.
    return PaintPropertyChangeType::kChangedOnlyValues;
  }

  return PaintPropertyChangeType::kChangedOnlySimpleValues;
}

// The root of the transform tree. The root transform node references the root
// scroll node.
const TransformPaintPropertyNode& TransformPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(
      TransformPaintPropertyNode, root,
      base::AdoptRef(new TransformPaintPropertyNode(
          nullptr,
          State{gfx::Vector2dF(), &ScrollPaintPropertyNode::Root(), nullptr,
                State::Flags{false /* flattens_inherited_transform */,
                             false /* in_subtree_of_page_scale */}})));
  return *root;
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

const TransformPaintPropertyNode&
TransformPaintPropertyNode::NearestScrollTranslationNode() const {
  const auto* transform = this;
  while (!transform->ScrollNode()) {
    transform = transform->UnaliasedParent();
    // The transform should never be null because the root transform has an
    // associated scroll node (see: TransformPaintPropertyNode::Root()).
    DCHECK(transform);
  }
  return *transform;
}

std::unique_ptr<JSONObject> TransformPaintPropertyNode::ToJSON() const {
  auto json = ToJSONBase();
  if (IsIdentityOr2DTranslation()) {
    if (!Translation2D().IsZero())
      json->SetString("translation2d", String(Translation2D().ToString()));
  } else {
    json->SetString("matrix", Matrix().ToString());
    json->SetString("origin", String(Origin().ToString()));
  }
  if (!state_.flags.flattens_inherited_transform)
    json->SetBoolean("flattensInheritedTransform", false);
  if (!state_.flags.in_subtree_of_page_scale)
    json->SetBoolean("in_subtree_of_page_scale", false);
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
                    state_.compositor_element_id.ToString().c_str());
  }
  if (state_.scroll)
    json->SetString("scroll", String::Format("%p", state_.scroll.get()));

  if (state_.scroll_translation_for_fixed) {
    json->SetString(
        "scroll_translation_for_fixed",
        String::Format("%p", state_.scroll_translation_for_fixed.get()));
  }
  return json;
}

}  // namespace blink

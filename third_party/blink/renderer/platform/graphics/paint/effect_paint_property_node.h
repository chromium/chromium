// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_

#include <algorithm>
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/rrect_f.h"

namespace blink {

class PropertyTreeState;

// Effect nodes are abstraction of isolated groups, along with optional effects
// that can be applied to the composited output of the group.
//
// The effect tree is rooted at a node with no parent. This root node should
// not be modified.
class PLATFORM_EXPORT EffectPaintPropertyNode
    : public PaintPropertyNode<EffectPaintPropertyNode> {
 public:
  struct AnimationState {
    AnimationState() {}
    bool is_running_opacity_animation_on_compositor = false;
    bool is_running_filter_animation_on_compositor = false;
    bool is_running_backdrop_filter_animation_on_compositor = false;
  };

  // To make it less verbose and more readable to construct and update a node,
  // a struct with default values is used to represent the state.
  struct State {
    // The local transform space serves two purposes:
    // 1. Assign a depth mapping for 3D depth sorting against other paint chunks
    //    and effects under the same parent.
    // 2. Some effects are spatial (namely blur filter and reflection), the
    //    effect parameters will be specified in the local space.
    scoped_refptr<const TransformPaintPropertyNode> local_transform_space;
    // The output of the effect can be optionally clipped when composited onto
    // the current backdrop.
    scoped_refptr<const ClipPaintPropertyNode> output_clip;
    // Optionally a number of effects can be applied to the composited output.
    // The chain of effects will be applied in the following order:
    // === Begin of effects ===
    ColorFilter color_filter = kColorFilterNone;
    CompositorFilterOperations filter;
    float opacity = 1;
    CompositorFilterOperations backdrop_filter;
    base::Optional<gfx::RRectF> backdrop_filter_bounds;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;
    // === End of effects ===
    CompositingReasons direct_compositing_reasons = CompositingReason::kNone;
    CompositorElementId compositor_element_id;
    // The compositor element id for any masks that are applied to elements that
    // also have backdrop-filters applied.
    CompositorElementId backdrop_mask_element_id;
    // TODO(crbug.com/900241): Use direct_compositing_reasons to check for
    // active animations when we can track animations for each property type.
    bool has_active_opacity_animation = false;
    bool has_active_filter_animation = false;
    bool has_active_backdrop_filter_animation = false;
    // The offset of the origin of filters in local_transform_space.
    FloatPoint filters_origin;

    PaintPropertyChangeType ComputeChange(
        const State& other,
        const AnimationState& animation_state) {
      if (local_transform_space != other.local_transform_space ||
          output_clip != other.output_clip ||
          color_filter != other.color_filter ||
          backdrop_filter_bounds != other.backdrop_filter_bounds ||
          blend_mode != other.blend_mode ||
          filters_origin != other.filters_origin) {
        return PaintPropertyChangeType::kChangedOnlyValues;
      }
      bool opacity_changed = opacity != other.opacity;
      bool opacity_change_is_simple =
          opacity_changed && opacity != 1.f && other.opacity != 1.f;
      if (opacity_changed && !opacity_change_is_simple &&
          !animation_state.is_running_opacity_animation_on_compositor) {
        return PaintPropertyChangeType::kChangedOnlyValues;
      }
      bool filter_changed = filter != other.filter;
      if (filter_changed &&
          !animation_state.is_running_filter_animation_on_compositor) {
        return PaintPropertyChangeType::kChangedOnlyValues;
      }
      bool backdrop_filter_changed = backdrop_filter != other.backdrop_filter;
      if (backdrop_filter_changed &&
          !animation_state.is_running_backdrop_filter_animation_on_compositor) {
        return PaintPropertyChangeType::kChangedOnlyValues;
      }
      bool non_reraster_values_changed =
          direct_compositing_reasons != other.direct_compositing_reasons ||
          compositor_element_id != other.compositor_element_id;
      bool simple_values_changed =
          opacity_change_is_simple &&
          !animation_state.is_running_opacity_animation_on_compositor;
      if (non_reraster_values_changed && simple_values_changed)
        return PaintPropertyChangeType::kChangedOnlyValues;
      if (non_reraster_values_changed)
        return PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
      if (simple_values_changed)
        return PaintPropertyChangeType::kChangedOnlySimpleValues;

      if (opacity_changed || filter_changed || backdrop_filter_changed) {
        return PaintPropertyChangeType::kChangedOnlyCompositedValues;
      }
      return PaintPropertyChangeType::kUnchanged;
    }
  };

  // This node is really a sentinel, and does not represent a real effect.
  static const EffectPaintPropertyNode& Root();

  static scoped_refptr<EffectPaintPropertyNode> Create(
      const EffectPaintPropertyNode& parent,
      State&& state) {
    return base::AdoptRef(new EffectPaintPropertyNode(
        &parent, std::move(state), false /* is_parent_alias */));
  }
  static scoped_refptr<EffectPaintPropertyNode> CreateAlias(
      const EffectPaintPropertyNode& parent) {
    return base::AdoptRef(new EffectPaintPropertyNode(
        &parent, State{}, true /* is_parent_alias */));
  }

  PaintPropertyChangeType Update(
      const EffectPaintPropertyNode& parent,
      State&& state,
      const AnimationState& animation_state = AnimationState()) {
    auto parent_changed = SetParent(&parent);
    auto state_changed = state_.ComputeChange(state, animation_state);
    if (state_changed != PaintPropertyChangeType::kUnchanged) {
      DCHECK(!IsParentAlias()) << "Changed the state of an alias node.";
      state_ = std::move(state);
      AddChanged(state_changed);
    }
    return std::max(parent_changed, state_changed);
  }

  // Checks if the accumulated effect from |this| to |relative_to_state
  // .Effect()| has changed, at least significance of |change|, in the space of
  // |relative_to_state.Transform()|. We check for changes of not only effect
  // nodes, but also LocalTransformSpace relative to |relative_to_state
  // .Transform()| of the effect nodes having filters that move pixels. Change
  // of OutputClip is not checked and the caller should check in other ways.
  // |transform_not_to_check| specifies the transform node that the caller has
  // checked or will check its change in other ways and this function should
  // treat it as unchanged.
  bool Changed(PaintPropertyChangeType change,
               const PropertyTreeState& relative_to_state,
               const TransformPaintPropertyNode* transform_not_to_check) const;

  const TransformPaintPropertyNode& LocalTransformSpace() const {
    DCHECK(!Parent() || !IsParentAlias());
    return *state_.local_transform_space;
  }
  const ClipPaintPropertyNode* OutputClip() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.output_clip.get();
  }

  SkBlendMode BlendMode() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.blend_mode;
  }
  float Opacity() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.opacity;
  }
  const CompositorFilterOperations& Filter() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.filter;
  }
  ColorFilter GetColorFilter() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.color_filter;
  }

  const CompositorFilterOperations& BackdropFilter() const {
    return state_.backdrop_filter;
  }

  const base::Optional<gfx::RRectF>& BackdropFilterBounds() const {
    return state_.backdrop_filter_bounds;
  }

  const CompositorElementId& BackdropMaskElementId() const {
    return state_.backdrop_mask_element_id;
  }

  bool HasFilterThatMovesPixels() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.filter.HasFilterThatMovesPixels();
  }

  FloatPoint FiltersOrigin() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.filters_origin;
  }

  bool HasRealEffects() const {
    return Opacity() != 1.0f || GetColorFilter() != kColorFilterNone ||
           BlendMode() != SkBlendMode::kSrcOver || !Filter().IsEmpty() ||
           !BackdropFilter().IsEmpty();
  }

  // Returns a rect covering the pixels that can be affected by pixels in
  // |inputRect|. The rects are in the space of localTransformSpace.
  FloatRect MapRect(const FloatRect& input_rect) const;

  bool HasDirectCompositingReasons() const {
    return DirectCompositingReasons() != CompositingReason::kNone;
  }

  // TODO(crbug.com/900241): Use HaveActiveXXXAnimation() instead of this
  // function when we can track animations for each property type.
  bool RequiresCompositingForAnimation() const {
    return DirectCompositingReasons() &
           CompositingReason::kComboActiveAnimation;
  }
  bool HasActiveOpacityAnimation() const {
    return state_.has_active_opacity_animation;
    // TODO(crbug.com/900241): Use the following code when we can track
    // animations for each property type.
    // return DirectCompositingReasons() &
    //        CompositingReason::kActiveOpacityAnimation;
  }
  bool HasActiveFilterAnimation() const {
    return state_.has_active_filter_animation;
    // TODO(crbug.com/900241): Use the following code when we can track
    // animations for each property type.
    // return DirectCompositingReasons() &
    //        CompositingReason::kActiveFilterAnimation;
  }
  bool HasActiveBackdropFilterAnimation() const {
    return state_.has_active_backdrop_filter_animation;
    // TODO(crbug.com/900241): Use the following code when we can track
    // animations for each property type.
    // return DirectCompositingReasons() &
    //        CompositingReason::kActiveBackdropFilterAnimation;
  }

  const CompositorElementId& GetCompositorElementId() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.compositor_element_id;
  }

  std::unique_ptr<JSONObject> ToJSON() const;

  // Returns memory usage of this node plus ancestors.
  size_t TreeMemoryUsageInBytes() const;

 private:
  EffectPaintPropertyNode(const EffectPaintPropertyNode* parent,
                          State&& state,
                          bool is_parent_alias)
      : PaintPropertyNode(parent, is_parent_alias), state_(std::move(state)) {}

  CompositingReasons DirectCompositingReasons() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.direct_compositing_reasons;
  }

  State state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_

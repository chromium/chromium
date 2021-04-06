// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_

#include <algorithm>
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/document_transition_shared_element_id.h"
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
class EffectPaintPropertyNode;

class PLATFORM_EXPORT EffectPaintPropertyNodeOrAlias
    : public PaintPropertyNode<EffectPaintPropertyNodeOrAlias,
                               EffectPaintPropertyNode> {
 public:
  // Checks if the accumulated effect from |this| to |relative_to_state
  // .Effect()| has changed, at least significance of |change|, in the space of
  // |relative_to_state.Transform()|. We check for changes of not only effect
  // nodes, but also LocalTransformSpace relative to |relative_to_state
  // .Transform()| of the effect nodes having filters that move pixels. Change
  // of OutputClip is not checked and the caller should check in other ways.
  // |transform_not_to_check| specifies the transform node that the caller has
  // checked or will check its change in other ways and this function should
  // treat it as unchanged.
  bool Changed(
      PaintPropertyChangeType change,
      const PropertyTreeState& relative_to_state,
      const TransformPaintPropertyNodeOrAlias* transform_not_to_check) const;

 protected:
  using PaintPropertyNode::PaintPropertyNode;
};

class EffectPaintPropertyNodeAlias : public EffectPaintPropertyNodeOrAlias {
 public:
  static scoped_refptr<EffectPaintPropertyNodeAlias> Create(
      const EffectPaintPropertyNodeOrAlias& parent) {
    return base::AdoptRef(new EffectPaintPropertyNodeAlias(parent));
  }

  PaintPropertyChangeType SetParent(
      const EffectPaintPropertyNodeOrAlias& parent) {
    DCHECK(IsParentAlias());
    return PaintPropertyNode::SetParent(parent);
  }

 private:
  explicit EffectPaintPropertyNodeAlias(
      const EffectPaintPropertyNodeOrAlias& parent)
      : EffectPaintPropertyNodeOrAlias(parent, kParentAlias) {}
};

class PLATFORM_EXPORT EffectPaintPropertyNode
    : public EffectPaintPropertyNodeOrAlias {
 public:
  struct AnimationState {
    AnimationState() {}
    bool is_running_opacity_animation_on_compositor = false;
    bool is_running_filter_animation_on_compositor = false;
    bool is_running_backdrop_filter_animation_on_compositor = false;
  };

  struct BackdropFilterInfo {
    CompositorFilterOperations operations;
    gfx::RRectF bounds;
    // The compositor element id for any masks that are applied to elements that
    // also have backdrop-filters applied.
    CompositorElementId mask_element_id;

    static PaintPropertyChangeType ComputeChange(
        const BackdropFilterInfo* a,
        const BackdropFilterInfo* b,
        bool is_running_backdrop_filter_animation_on_compositor) {
      if (!a && !b)
        return PaintPropertyChangeType::kUnchanged;
      if (!a || !b || a->bounds != b->bounds ||
          a->mask_element_id != b->mask_element_id)
        return PaintPropertyChangeType::kChangedOnlyValues;
      if (a->operations != b->operations) {
        return is_running_backdrop_filter_animation_on_compositor
                   ? PaintPropertyChangeType::kChangedOnlyCompositedValues
                   : PaintPropertyChangeType::kChangedOnlyValues;
      }
      return PaintPropertyChangeType::kUnchanged;
    }
  };

  // To make it less verbose and more readable to construct and update a node,
  // a struct with default values is used to represent the state.
  struct State {
    // The local transform space serves two purposes:
    // 1. Assign a depth mapping for 3D depth sorting against other paint chunks
    //    and effects under the same parent.
    // 2. Some effects are spatial (namely blur filter and reflection), the
    //    effect parameters will be specified in the local space.
    scoped_refptr<const TransformPaintPropertyNodeOrAlias>
        local_transform_space;
    // The output of the effect can be optionally clipped when composited onto
    // the current backdrop.
    scoped_refptr<const ClipPaintPropertyNodeOrAlias> output_clip;
    // Optionally a number of effects can be applied to the composited output.
    // The chain of effects will be applied in the following order:
    // === Begin of effects ===
    CompositorFilterOperations filter;
    std::unique_ptr<BackdropFilterInfo> backdrop_filter_info;
    float opacity = 1;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;
    // === End of effects ===
    CompositingReasons direct_compositing_reasons = CompositingReason::kNone;
    CompositorElementId compositor_element_id;

    // An identifier for a document transition shared element. `id.valid()`
    // returns true if this has been set, and false otherwise.
    DocumentTransitionSharedElementId document_transition_shared_element_id;

    // TODO(crbug.com/900241): Use direct_compositing_reasons to check for
    // active animations when we can track animations for each property type.
    bool has_active_opacity_animation = false;
    bool has_active_filter_animation = false;
    bool has_active_backdrop_filter_animation = false;

    PaintPropertyChangeType ComputeChange(
        const State& other,
        const AnimationState& animation_state) {
      if (local_transform_space != other.local_transform_space ||
          output_clip != other.output_clip ||
          blend_mode != other.blend_mode ||
          document_transition_shared_element_id !=
              other.document_transition_shared_element_id) {
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
      auto backdrop_filter_changed = BackdropFilterInfo::ComputeChange(
          backdrop_filter_info.get(), other.backdrop_filter_info.get(),
          animation_state.is_running_backdrop_filter_animation_on_compositor);
      if (backdrop_filter_changed ==
          PaintPropertyChangeType::kChangedOnlyValues) {
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

      if (opacity_changed || filter_changed ||
          backdrop_filter_changed != PaintPropertyChangeType::kUnchanged) {
        return PaintPropertyChangeType::kChangedOnlyCompositedValues;
      }
      return PaintPropertyChangeType::kUnchanged;
    }
  };

  // This node is really a sentinel, and does not represent a real effect.
  static const EffectPaintPropertyNode& Root();

  static scoped_refptr<EffectPaintPropertyNode> Create(
      const EffectPaintPropertyNodeOrAlias& parent,
      State&& state) {
    return base::AdoptRef(
        new EffectPaintPropertyNode(&parent, std::move(state)));
  }

  PaintPropertyChangeType Update(
      const EffectPaintPropertyNodeOrAlias& parent,
      State&& state,
      const AnimationState& animation_state = AnimationState()) {
    auto parent_changed = SetParent(parent);
    auto state_changed = state_.ComputeChange(state, animation_state);
    if (state_changed != PaintPropertyChangeType::kUnchanged) {
      state_ = std::move(state);
      AddChanged(state_changed);
    }
    return std::max(parent_changed, state_changed);
  }

  const EffectPaintPropertyNode& Unalias() const = delete;
  bool IsParentAlias() const = delete;

  const TransformPaintPropertyNodeOrAlias& LocalTransformSpace() const {
    return *state_.local_transform_space;
  }
  const ClipPaintPropertyNodeOrAlias* OutputClip() const {
    return state_.output_clip.get();
  }

  SkBlendMode BlendMode() const {
    return state_.blend_mode;
  }
  float Opacity() const {
    return state_.opacity;
  }
  const CompositorFilterOperations& Filter() const {
    return state_.filter;
  }

  const CompositorFilterOperations* BackdropFilter() const {
    if (!state_.backdrop_filter_info)
      return nullptr;
    DCHECK(!state_.backdrop_filter_info->operations.IsEmpty());
    return &state_.backdrop_filter_info->operations;
  }

  const gfx::RRectF& BackdropFilterBounds() const {
    DCHECK(state_.backdrop_filter_info);
    return state_.backdrop_filter_info->bounds;
  }

  CompositorElementId BackdropMaskElementId() const {
    DCHECK(state_.backdrop_filter_info);
    return state_.backdrop_filter_info->mask_element_id;
  }

  bool HasFilterThatMovesPixels() const {
    return state_.filter.HasFilterThatMovesPixels();
  }

  bool HasRealEffects() const {
    return Opacity() != 1.0f || BlendMode() != SkBlendMode::kSrcOver ||
           !Filter().IsEmpty() || BackdropFilter();
  }

  bool IsOpacityOnly() const {
    return BlendMode() == SkBlendMode::kSrcOver && Filter().IsEmpty() &&
           !BackdropFilter();
  }

  // Returns a rect covering the pixels that can be affected by pixels in
  // |inputRect|. The rects are in the space of localTransformSpace.
  FloatRect MapRect(const FloatRect& input_rect) const;

  bool HasDirectCompositingReasons() const {
    return state_.direct_compositing_reasons != CompositingReason::kNone;
  }
  bool RequiresCompositingForBackdropFilterMask() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kBackdropFilterMask;
  }

  // TODO(crbug.com/900241): Use HaveActiveXXXAnimation() instead of this
  // function when we can track animations for each property type.
  bool RequiresCompositingForAnimation() const {
    return state_.direct_compositing_reasons &
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

  // Whether the effect node uses the backdrop as an input. This includes
  // exotic blending modes and backdrop filters.
  bool HasBackdropEffect() const {
    return BlendMode() != SkBlendMode::kSrcOver || BackdropFilter() ||
           HasActiveBackdropFilterAnimation();
  }

  CompositingReasons DirectCompositingReasonsForDebugging() const {
    return state_.direct_compositing_reasons;
  }

  const CompositorElementId& GetCompositorElementId() const {
    return state_.compositor_element_id;
  }

  const blink::DocumentTransitionSharedElementId&
  DocumentTransitionSharedElementId() const {
    return state_.document_transition_shared_element_id;
  }

  std::unique_ptr<JSONObject> ToJSON() const;

 private:
  EffectPaintPropertyNode(const EffectPaintPropertyNodeOrAlias* parent,
                          State&& state)
      : EffectPaintPropertyNodeOrAlias(parent), state_(std::move(state)) {}

  using EffectPaintPropertyNodeOrAlias::SetParent;

  State state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_

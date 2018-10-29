// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_

#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/platform_export.h"

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
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;
    // === End of effects ===
    CompositingReasons direct_compositing_reasons = CompositingReason::kNone;
    CompositorElementId compositor_element_id;
    // The offset of the origin of filters in local_transform_space.
    FloatPoint filters_origin;

    bool operator==(const State& o) const {
      return local_transform_space == o.local_transform_space &&
             output_clip == o.output_clip && color_filter == o.color_filter &&
             filter == o.filter && opacity == o.opacity &&
             blend_mode == o.blend_mode &&
             direct_compositing_reasons == o.direct_compositing_reasons &&
             compositor_element_id == o.compositor_element_id &&
             filters_origin == o.filters_origin;
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

  bool Update(const EffectPaintPropertyNode& parent, State&& state) {
    bool parent_changed = SetParent(&parent);
    if (state == state_)
      return parent_changed;

    DCHECK(!IsParentAlias()) << "Changed the state of an alias node.";
    state_ = std::move(state);
    SetChanged();
    return true;
  }

  // Checks if the accumulated effect from |this| to |relative_to_state
  // .Effect()| has changed in the space of |relative_to_state.Transform()|.
  // We check for changes of not only effect nodes, but also LocalTransformSpace
  // relative to |relative_to_state.Transform()| of the effect nodes having
  // filters that move pixels. Change of OutputClip is not checked and the
  // caller should check in other ways. |transform_not_to_check| specifies the
  // transform node that the caller has checked or will check its change in
  // other ways and this function should treat it as unchanged.
  bool Changed(const PropertyTreeState& relative_to_state,
               const TransformPaintPropertyNode* transform_not_to_check) const;

  const TransformPaintPropertyNode* LocalTransformSpace() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.local_transform_space.get();
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

  bool HasFilterThatMovesPixels() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.filter.HasFilterThatMovesPixels();
  }

  FloatPoint FiltersOrigin() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.filters_origin;
  }

  // Returns a rect covering the pixels that can be affected by pixels in
  // |inputRect|. The rects are in the space of localTransformSpace.
  FloatRect MapRect(const FloatRect& input_rect) const;

  bool HasDirectCompositingReasons() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.direct_compositing_reasons != CompositingReason::kNone;
  }

  bool RequiresCompositingForAnimation() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.direct_compositing_reasons &
           CompositingReason::kComboActiveAnimation;
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

  State state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_

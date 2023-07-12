// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_IMPL_H_

#include <array>
#include <memory>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This class is the implementation of the ObjectPaintProperties interface,
// and is used for storing the paint property nodes created by a LayoutObject.
class CORE_EXPORT ObjectPaintPropertiesImpl : public ObjectPaintProperties {
  USING_FAST_MALLOC(ObjectPaintPropertiesImpl);

 public:
  ObjectPaintPropertiesImpl() = default;
  ObjectPaintPropertiesImpl(ObjectPaintPropertiesImpl&&) = default;
  ObjectPaintPropertiesImpl(const ObjectPaintPropertiesImpl&) = delete;
  ObjectPaintPropertiesImpl& operator=(ObjectPaintPropertiesImpl&&) = default;
  ObjectPaintPropertiesImpl& operator=(const ObjectPaintPropertiesImpl&) =
      delete;

  ~ObjectPaintPropertiesImpl() override {
#if DCHECK_IS_ON()
    DCHECK(!is_immutable_);
#endif
  }

// Preprocessor macro implementations.
#define ADD_NODE(type, function, variable)                              \
 public:                                                                \
  const type##PaintPropertyNode* function() const override {            \
    return variable.get();                                              \
  }                                                                     \
  PaintPropertyChangeType Update##function(                             \
      const type##PaintPropertyNodeOrAlias& parent,                     \
      type##PaintPropertyNode::State&& state,                           \
      const type##PaintPropertyNode::AnimationState& animation_state =  \
          type##PaintPropertyNode::AnimationState()) override {         \
    return Update(variable, parent, std::move(state), animation_state); \
  }                                                                     \
  bool Clear##function() override { return Clear(variable); }           \
                                                                        \
 private:                                                               \
  scoped_refptr<type##PaintPropertyNode> variable
  // (End of ADD_NODE definition)

#define ADD_ALIAS_NODE(type, function, variable)                    \
 public:                                                            \
  const type##PaintPropertyNodeOrAlias* function() const override { \
    return variable.get();                                          \
  }                                                                 \
  PaintPropertyChangeType Update##function(                         \
      const type##PaintPropertyNodeOrAlias& parent) override {      \
    return UpdateAlias(variable, parent);                           \
  }                                                                 \
  bool Clear##function() override { return Clear(variable); }       \
                                                                    \
 private:                                                           \
  scoped_refptr<type##PaintPropertyNodeAlias> variable
  // (End of ADD_ALIAS_NODE definition)

#define ADD_TRANSFORM(function, variable) \
  ADD_NODE(Transform, function, variable)
#define ADD_EFFECT(function, variable) ADD_NODE(Effect, function, variable)
#define ADD_CLIP(function, variable) ADD_NODE(Clip, function, variable)

  // Transform node implementations.
  bool HasTransformNode() const override {
    return paint_offset_translation_ || sticky_translation_ ||
           anchor_position_scroll_translation_ || translate_ || rotate_ ||
           scale_ || offset_ || transform_ || perspective_ ||
           replaced_content_transform_ || scroll_translation_ ||
           transform_isolation_node_;
  }
  bool HasCSSTransformPropertyNode() const override {
    return translate_ || rotate_ || scale_ || offset_ || transform_;
  }
  std::array<const TransformPaintPropertyNode*, 5>
  AllCSSTransformPropertiesOutsideToInside() const override {
    return {Translate(), Rotate(), Scale(), Offset(), Transform()};
  }
  ADD_TRANSFORM(PaintOffsetTranslation, paint_offset_translation_);
  ADD_TRANSFORM(StickyTranslation, sticky_translation_);
  ADD_TRANSFORM(AnchorPositionScrollTranslation,
                anchor_position_scroll_translation_);
  ADD_TRANSFORM(Translate, translate_);
  ADD_TRANSFORM(Rotate, rotate_);
  ADD_TRANSFORM(Scale, scale_);
  ADD_TRANSFORM(Offset, offset_);
  ADD_TRANSFORM(Transform, transform_);
  ADD_TRANSFORM(Perspective, perspective_);
  ADD_TRANSFORM(ReplacedContentTransform, replaced_content_transform_);
  ADD_TRANSFORM(ScrollTranslation, scroll_translation_);
  using ScrollPaintPropertyNodeOrAlias = ScrollPaintPropertyNode;
  ADD_NODE(Scroll, Scroll, scroll_);
  ADD_ALIAS_NODE(Transform, TransformIsolationNode, transform_isolation_node_);

  // Effect node implementations.
  bool HasEffectNode() const override {
    return effect_ || filter_ || vertical_scrollbar_effect_ ||
           horizontal_scrollbar_effect_ || scroll_corner_effect_ || mask_ ||
           clip_path_mask_ || effect_isolation_node_;
  }
  ADD_EFFECT(Effect, effect_);
  ADD_EFFECT(Filter, filter_);
  ADD_EFFECT(VerticalScrollbarEffect, vertical_scrollbar_effect_);
  ADD_EFFECT(HorizontalScrollbarEffect, horizontal_scrollbar_effect_);
  ADD_EFFECT(ScrollCornerEffect, scroll_corner_effect_);
  ADD_EFFECT(Mask, mask_);
  ADD_EFFECT(ClipPathMask, clip_path_mask_);
  ADD_ALIAS_NODE(Effect, EffectIsolationNode, effect_isolation_node_);

  // Clip node implementations.
  bool HasClipNode() const override {
    return pixel_moving_filter_clip_expander_ || clip_path_clip_ ||
           mask_clip_ || css_clip_ || overflow_controls_clip_ ||
           inner_border_radius_clip_ || overflow_clip_ || clip_isolation_node_;
  }
  ADD_CLIP(PixelMovingFilterClipExpander, pixel_moving_filter_clip_expander_);
  ADD_CLIP(ClipPathClip, clip_path_clip_);
  ADD_CLIP(MaskClip, mask_clip_);
  ADD_CLIP(CssClip, css_clip_);
  ADD_CLIP(CssClipFixedPosition, css_clip_fixed_position_);
  ADD_CLIP(OverflowControlsClip, overflow_controls_clip_);
  ADD_CLIP(BackgroundClip, background_clip_);
  ADD_CLIP(InnerBorderRadiusClip, inner_border_radius_clip_);
  ADD_CLIP(OverflowClip, overflow_clip_);
  ADD_ALIAS_NODE(Clip, ClipIsolationNode, clip_isolation_node_);

#undef ADD_CLIP
#undef ADD_EFFECT
#undef ADD_TRANSFORM
#undef ADD_NODE
#undef ADD_ALIAS_NODE

// Debug-only state change validation method implementations.
#if DCHECK_IS_ON()
  void SetImmutable() const override { is_immutable_ = true; }
  bool IsImmutable() const override { return is_immutable_; }
  void SetMutable() const override { is_immutable_ = false; }

  void Validate() override {
    DCHECK(!ScrollTranslation() || !ReplacedContentTransform())
        << "Replaced elements don't scroll so there should never be both a "
           "scroll translation and a replaced content transform.";
    DCHECK(!ClipPathClip() || !ClipPathMask())
        << "ClipPathClip and ClipPathshould be mutually exclusive.";
    DCHECK((!TransformIsolationNode() && !ClipIsolationNode() &&
            !EffectIsolationNode()) ||
           (TransformIsolationNode() && ClipIsolationNode() &&
            EffectIsolationNode()))
        << "Isolation nodes have to be created for all of transform, clip, and "
           "effect trees.";
  }
#endif

  // Direct update method implementations.
  PaintPropertyChangeType DirectlyUpdateTransformAndOrigin(
      TransformPaintPropertyNode::TransformAndOrigin&& transform_and_origin,
      const TransformPaintPropertyNode::AnimationState& animation_state)
      override {
    return transform_->DirectlyUpdateTransformAndOrigin(
        std::move(transform_and_origin), animation_state);
  }

  PaintPropertyChangeType DirectlyUpdateOpacity(
      float opacity,
      const EffectPaintPropertyNode::AnimationState& animation_state) override {
    // TODO(yotha): Remove this check once we make sure crbug.com/1370268 is
    // fixed
    DCHECK(effect_ != nullptr);
    if (effect_ == nullptr) {
      return PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
    return effect_->DirectlyUpdateOpacity(opacity, animation_state);
  }

 private:
  // Return true if the property tree structure changes (an existing node was
  // deleted), and false otherwise. See the class-level comment on
  // ObjectPaintProperties ("update & clear implementation note") for details
  // about why this is needed for efficiency.
  template <typename PaintPropertyNode>
  bool Clear(scoped_refptr<PaintPropertyNode>& field) {
    if (field) {
      field = nullptr;
      return true;
    }
    return false;
  }

  // Return true if the property tree structure changes (a new node was
  // created), and false otherwise. See the class-level comment on
  // ObjectPaintProperties ("update & clear implementation note") for details
  // about why this is needed for efficiency.
  template <typename PaintPropertyNode, typename PaintPropertyNodeOrAlias>
  PaintPropertyChangeType Update(
      scoped_refptr<PaintPropertyNode>& field,
      const PaintPropertyNodeOrAlias& parent,
      typename PaintPropertyNode::State&& state,
      const typename PaintPropertyNode::AnimationState& animation_state) {
    if (field) {
      auto changed = field->Update(parent, std::move(state), animation_state);
#if DCHECK_IS_ON()
      DCHECK(!is_immutable_ || changed == PaintPropertyChangeType::kUnchanged)
          << "Value changed while immutable. New state:\n"
          << *field;
#endif
      return changed;
    }
    field = PaintPropertyNode::Create(parent, std::move(state));
#if DCHECK_IS_ON()
    DCHECK(!is_immutable_) << "Node added while immutable. New state:\n"
                           << *field;
#endif
    return PaintPropertyChangeType::kNodeAddedOrRemoved;
  }

  template <typename PaintPropertyNodeAlias, typename PaintPropertyNodeOrAlias>
  PaintPropertyChangeType UpdateAlias(
      scoped_refptr<PaintPropertyNodeAlias>& field,
      const PaintPropertyNodeOrAlias& parent) {
    if (field) {
      DCHECK(field->IsParentAlias());
      auto changed = field->SetParent(parent);
#if DCHECK_IS_ON()
      DCHECK(!is_immutable_ || changed == PaintPropertyChangeType::kUnchanged)
          << "Parent changed while immutable. New state:\n"
          << *field;
#endif
      return changed;
    }
    field = PaintPropertyNodeAlias::Create(parent);
#if DCHECK_IS_ON()
    DCHECK(!is_immutable_) << "Node added while immutable. New state:\n"
                           << *field;
#endif
    return PaintPropertyChangeType::kNodeAddedOrRemoved;
  }

#if DCHECK_IS_ON()
  mutable bool is_immutable_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_IMPL_H_

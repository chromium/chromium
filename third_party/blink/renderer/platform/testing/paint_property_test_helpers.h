// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_PROPERTY_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_PROPERTY_TEST_HELPERS_H_

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {

// Convenient shorthands.
inline const TransformPaintPropertyNode& t0() {
  return TransformPaintPropertyNode::Root();
}
inline const ClipPaintPropertyNode& c0() {
  return ClipPaintPropertyNode::Root();
}
inline const EffectPaintPropertyNode& e0() {
  return EffectPaintPropertyNode::Root();
}

inline scoped_refptr<EffectPaintPropertyNode> CreateOpacityEffect(
    const EffectPaintPropertyNode& parent,
    const TransformPaintPropertyNode& local_transform_space,
    const ClipPaintPropertyNode* output_clip,
    float opacity,
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  EffectPaintPropertyNode::State state;
  state.local_transform_space = &local_transform_space;
  state.output_clip = output_clip;
  state.opacity = opacity;
  state.direct_compositing_reasons = compositing_reasons;
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kPrimary);
  return EffectPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<EffectPaintPropertyNode> CreateOpacityEffect(
    const EffectPaintPropertyNode& parent,
    float opacity,
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  return CreateOpacityEffect(parent, parent.Unalias().LocalTransformSpace(),
                             parent.Unalias().OutputClip(), opacity,
                             compositing_reasons);
}

inline scoped_refptr<EffectPaintPropertyNode> CreateAnimatingOpacityEffect(
    const EffectPaintPropertyNode& parent,
    float opacity = 1.f,
    const ClipPaintPropertyNode* output_clip = nullptr) {
  EffectPaintPropertyNode::State state;
  state.local_transform_space = &parent.Unalias().LocalTransformSpace();
  state.output_clip = output_clip;
  state.opacity = opacity;
  state.direct_compositing_reasons = CompositingReason::kActiveOpacityAnimation;
  state.has_active_opacity_animation = true;
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kPrimaryEffect);
  return EffectPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<EffectPaintPropertyNode> CreateFilterEffect(
    const EffectPaintPropertyNode& parent,
    const TransformPaintPropertyNode& local_transform_space,
    const ClipPaintPropertyNode* output_clip,
    CompositorFilterOperations filter,
    const FloatPoint& filters_origin = FloatPoint(),
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  EffectPaintPropertyNode::State state;
  state.local_transform_space = &local_transform_space;
  state.output_clip = output_clip;
  state.filter = std::move(filter);
  state.filters_origin = filters_origin;
  state.direct_compositing_reasons = compositing_reasons;
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kEffectFilter);
  return EffectPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<EffectPaintPropertyNode> CreateFilterEffect(
    const EffectPaintPropertyNode& parent,
    CompositorFilterOperations filter,
    const FloatPoint& paint_offset = FloatPoint(),
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  return CreateFilterEffect(parent, parent.Unalias().LocalTransformSpace(),
                            parent.Unalias().OutputClip(), filter, paint_offset,
                            compositing_reasons);
}

inline scoped_refptr<EffectPaintPropertyNode> CreateAnimatingFilterEffect(
    const EffectPaintPropertyNode& parent,
    CompositorFilterOperations filter = CompositorFilterOperations(),
    const ClipPaintPropertyNode* output_clip = nullptr) {
  EffectPaintPropertyNode::State state;
  state.local_transform_space = &parent.Unalias().LocalTransformSpace();
  state.output_clip = output_clip;
  state.filter = std::move(filter);
  state.direct_compositing_reasons = CompositingReason::kActiveFilterAnimation;
  state.has_active_filter_animation = true;
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kEffectFilter);
  return EffectPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<EffectPaintPropertyNode> CreateBackdropFilterEffect(
    const EffectPaintPropertyNode& parent,
    const TransformPaintPropertyNode& local_transform_space,
    const ClipPaintPropertyNode* output_clip,
    CompositorFilterOperations backdrop_filter,
    const FloatPoint& filters_origin = FloatPoint(),
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  EffectPaintPropertyNode::State state;
  state.local_transform_space = &local_transform_space;
  state.output_clip = output_clip;
  state.backdrop_filter = std::move(backdrop_filter);
  state.filters_origin = filters_origin;
  state.direct_compositing_reasons = compositing_reasons;
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kPrimary);
  return EffectPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<EffectPaintPropertyNode> CreateBackdropFilterEffect(
    const EffectPaintPropertyNode& parent,
    CompositorFilterOperations backdrop_filter,
    const FloatPoint& paint_offset = FloatPoint(),
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  return CreateBackdropFilterEffect(
      parent, parent.Unalias().LocalTransformSpace(),
      parent.Unalias().OutputClip(), backdrop_filter, paint_offset,
      compositing_reasons);
}

inline scoped_refptr<EffectPaintPropertyNode>
CreateAnimatingBackdropFilterEffect(
    const EffectPaintPropertyNode& parent,
    CompositorFilterOperations backdrop_filter = CompositorFilterOperations(),
    const ClipPaintPropertyNode* output_clip = nullptr) {
  EffectPaintPropertyNode::State state;
  state.local_transform_space = &parent.Unalias().LocalTransformSpace();
  state.output_clip = output_clip;
  state.backdrop_filter = std::move(backdrop_filter);
  state.direct_compositing_reasons =
      CompositingReason::kActiveBackdropFilterAnimation;
  state.has_active_backdrop_filter_animation = true;
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kPrimaryEffect);
  return EffectPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<ClipPaintPropertyNode> CreateClip(
    const ClipPaintPropertyNode& parent,
    const TransformPaintPropertyNode& local_transform_space,
    const FloatRoundedRect& clip_rect) {
  ClipPaintPropertyNode::State state;
  state.local_transform_space = &local_transform_space;
  state.clip_rect = clip_rect;
  return ClipPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<ClipPaintPropertyNode> CreateClipPathClip(
    const ClipPaintPropertyNode& parent,
    const TransformPaintPropertyNode& local_transform_space,
    const FloatRoundedRect& clip_rect) {
  ClipPaintPropertyNode::State state;
  state.local_transform_space = &local_transform_space;
  state.clip_rect = clip_rect;
  state.clip_path = base::AdoptRef(new RefCountedPath);
  return ClipPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<TransformPaintPropertyNode> Create2DTranslation(
    const TransformPaintPropertyNode& parent,
    float x,
    float y) {
  return TransformPaintPropertyNode::Create(
      parent, TransformPaintPropertyNode::State{FloatSize(x, y)});
}

inline scoped_refptr<TransformPaintPropertyNode> CreateTransform(
    const TransformPaintPropertyNode& parent,
    const TransformationMatrix& matrix,
    const FloatPoint3D& origin = FloatPoint3D(),
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  TransformPaintPropertyNode::State state{
      TransformPaintPropertyNode::TransformAndOrigin(matrix, origin)};
  state.direct_compositing_reasons = compositing_reasons;
  return TransformPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<TransformPaintPropertyNode> CreateAnimatingTransform(
    const TransformPaintPropertyNode& parent,
    const TransformationMatrix& matrix = TransformationMatrix(),
    const FloatPoint3D& origin = FloatPoint3D()) {
  TransformPaintPropertyNode::State state{
      TransformPaintPropertyNode::TransformAndOrigin(matrix, origin)};
  state.direct_compositing_reasons =
      CompositingReason::kActiveTransformAnimation;
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kPrimaryTransform);
  return TransformPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<TransformPaintPropertyNode> CreateScrollTranslation(
    const TransformPaintPropertyNode& parent,
    float offset_x,
    float offset_y,
    const ScrollPaintPropertyNode& scroll,
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  TransformPaintPropertyNode::State state{FloatSize(offset_x, offset_y)};
  state.direct_compositing_reasons = compositing_reasons;
  state.scroll = &scroll;
  return TransformPaintPropertyNode::Create(parent, std::move(state));
}

inline scoped_refptr<TransformPaintPropertyNode>
CreateCompositedScrollTranslation(const TransformPaintPropertyNode& parent,
                                  float offset_x,
                                  float offset_y,
                                  const ScrollPaintPropertyNode& scroll) {
  return CreateScrollTranslation(parent, offset_x, offset_y, scroll,
                                 CompositingReason::kOverflowScrolling);
}

inline PropertyTreeState DefaultPaintChunkProperties() {
  return PropertyTreeState::Root();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_PROPERTY_TEST_HELPERS_H_

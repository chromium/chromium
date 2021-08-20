// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_definition.h"

#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"

namespace blink {

namespace {

// This class includes information that is required by the compositor thread
// when painting clip path.
class ClipPathPaintWorkletInput : public PaintWorkletInput {
 public:
  ClipPathPaintWorkletInput(
      const FloatRect& container_rect,
      int worklet_id,
      float zoom,
      const Vector<scoped_refptr<ShapeClipPathOperation>>& animated_shapes,
      cc::PaintWorkletInput::PropertyKeys property_keys)
      : PaintWorkletInput(container_rect.Size(),
                          worklet_id,
                          std::move(property_keys)),
        zoom_(zoom),
        animated_shapes_(animated_shapes) {}

  ~ClipPathPaintWorkletInput() override = default;
  const Vector<scoped_refptr<ShapeClipPathOperation>>& AnimatedShapes() const {
    return animated_shapes_;
  }
  float Zoom() const { return zoom_; }

  PaintWorkletInputType GetType() const override {
    return PaintWorkletInputType::kClipPath;
  }

 private:
  float zoom_;
  // TODO(crbug.com/1223975): This structure should support values for
  // StylePath.
  Vector<scoped_refptr<ShapeClipPathOperation>> animated_shapes_;
};

void GetAnimatedShapesFromKeyframes(
    const PropertySpecificKeyframe* frame,
    Vector<scoped_refptr<ShapeClipPathOperation>>* animated_shapes,
    const Element* element) {
  DCHECK(frame->IsCSSPropertySpecificKeyframe());
  const CSSValue* value =
      static_cast<const CSSPropertySpecificKeyframe*>(frame)->Value();
  const CSSPropertyName property_name =
      CSSPropertyName(CSSPropertyID::kClipPath);
  const CSSValue* computed_value = StyleResolver::ComputeValue(
      const_cast<Element*>(element), property_name, *value);

  StyleResolverState state(element->GetDocument(),
                           *const_cast<Element*>(element));
  scoped_refptr<ShapeClipPathOperation> basic_shape =
      ShapeClipPathOperation::Create(
          BasicShapeForValue(state, *computed_value));

  animated_shapes->push_back(basic_shape);
}
}  // namespace

template <>
struct DowncastTraits<ClipPathPaintWorkletInput> {
  static bool AllowFrom(const cc::PaintWorkletInput& worklet_input) {
    auto* input = DynamicTo<PaintWorkletInput>(worklet_input);
    return input && AllowFrom(*input);
  }

  static bool AllowFrom(const PaintWorkletInput& worklet_input) {
    return worklet_input.GetType() ==
           PaintWorkletInput::PaintWorkletInputType::kClipPath;
  }
};

// static
ClipPathPaintDefinition* ClipPathPaintDefinition::Create(
    LocalFrame& local_root) {
  return MakeGarbageCollected<ClipPathPaintDefinition>(local_root);
}

ClipPathPaintDefinition::ClipPathPaintDefinition(LocalFrame& local_root)
    : NativePaintDefinition(
          &local_root,
          PaintWorkletInput::PaintWorkletInputType::kClipPath) {}

sk_sp<PaintRecord> ClipPathPaintDefinition::Paint(
    const CompositorPaintWorkletInput* compositor_input,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  const ClipPathPaintWorkletInput* input =
      To<ClipPathPaintWorkletInput>(compositor_input);
  FloatSize container_size = input->ContainerSize();

  Vector<scoped_refptr<ShapeClipPathOperation>> animated_shapes =
      input->AnimatedShapes();
  DCHECK_GT(animated_shapes.size(), 1u);

  DCHECK_EQ(animated_property_values.size(), 1u);
  const auto& entry = animated_property_values.begin();
  float progress = entry->second.float_value.value();
  // TODO(crbug.com/1223975): implement interpolation here, instead of hard
  // coding.
  scoped_refptr<ShapeClipPathOperation> current_shape =
      progress < 0.5 ? animated_shapes[0] : animated_shapes[1];
  Path path = current_shape->GetPath(
      FloatRect(FloatPoint(0.0, 0.0), container_size), input->Zoom());
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  auto* rendering_context = MakeGarbageCollected<PaintRenderingContext2D>(
      RoundedIntSize(container_size), context_settings, 1, 1);

  PaintFlags flags;
  flags.setAntiAlias(true);
  rendering_context->GetPaintCanvas()->drawPath(path.GetSkPath(), flags);

  return rendering_context->GetRecord();
}

scoped_refptr<Image> ClipPathPaintDefinition::Paint(
    float zoom,
    const FloatRect& reference_box,
    const Node& node) {
  DCHECK(node.IsElementNode());
  const Element* element = static_cast<Element*>(const_cast<Node*>(&node));
  ElementAnimations* element_animations = element->GetElementAnimations();

  Vector<scoped_refptr<ShapeClipPathOperation>> animated_shapes;
  // TODO(crbug.com/1223975): implement main-thread fall back logic for
  // animations that we cannot handle.
  for (const auto& animation : element_animations->Animations()) {
    const AnimationEffect* effect = animation.key->effect();
    if (!effect->IsKeyframeEffect())
      continue;
    const KeyframeEffectModelBase* model =
        static_cast<const KeyframeEffect*>(effect)->Model();
    // TODO(crbug.com/1223975): handle transition keyframes here.
    if (!model->IsStringKeyframeEffectModel())
      continue;
    const PropertySpecificKeyframeVector* frames =
        model->GetPropertySpecificKeyframes(
            PropertyHandle(GetCSSPropertyClipPath()));
    DCHECK_GE(frames->size(), 2u);
    // TODO(crbug.com/1223975): right now we keep the first and last keyframe
    // values only, we need to keep all keyframe values.
    GetAnimatedShapesFromKeyframes(frames->front(), &animated_shapes, element);
    GetAnimatedShapesFromKeyframes(frames->back(), &animated_shapes, element);
  }

  node.GetLayoutObject()->GetMutableForPainting().EnsureId();
  CompositorElementId element_id = CompositorElementIdFromUniqueObjectId(
      node.GetLayoutObject()->UniqueId(),
      CompositorAnimations::CompositorElementNamespaceForProperty(
          CSSPropertyID::kClipPath));
  CompositorPaintWorkletInput::PropertyKeys input_property_keys;
  input_property_keys.emplace_back(
      CompositorPaintWorkletInput::NativePropertyType::kClipPath, element_id);
  scoped_refptr<ClipPathPaintWorkletInput> input =
      base::MakeRefCounted<ClipPathPaintWorkletInput>(
          reference_box, worklet_id_, zoom, animated_shapes,
          std::move(input_property_keys));

  return PaintWorkletDeferredImage::Create(std::move(input),
                                           reference_box.Size());
}

void ClipPathPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink

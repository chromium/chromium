// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/background_color_paint_worklet.h"

#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"

namespace blink {

namespace {

// This class includes information that is required by the compositor thread
// when painting background color.
class BackgroundColorPaintWorkletInput : public PaintWorkletInput {
 public:
  BackgroundColorPaintWorkletInput(
      const FloatSize& container_size,
      int worklet_id,
      const Vector<Color>& animated_colors,
      const Vector<double>& offsets,
      cc::PaintWorkletInput::PropertyKeys property_keys)
      : PaintWorkletInput(container_size, worklet_id, std::move(property_keys)),
        animated_colors_(animated_colors),
        offsets_(offsets) {}

  ~BackgroundColorPaintWorkletInput() override = default;

  const Vector<Color>& AnimatedColors() const { return animated_colors_; }
  const Vector<double>& Offsets() const { return offsets_; }

 private:
  Vector<Color> animated_colors_;
  Vector<double> offsets_;
};

class BackgroundColorPaintWorkletProxyClient
    : public NativePaintWorkletProxyClient {
  DISALLOW_COPY_AND_ASSIGN(BackgroundColorPaintWorkletProxyClient);

 public:
  static BackgroundColorPaintWorkletProxyClient* Create(int worklet_id) {
    return MakeGarbageCollected<BackgroundColorPaintWorkletProxyClient>(
        worklet_id);
  }

  explicit BackgroundColorPaintWorkletProxyClient(int worklet_id)
      : NativePaintWorkletProxyClient(worklet_id) {}
  ~BackgroundColorPaintWorkletProxyClient() override = default;

  // PaintWorkletPainter implementation.
  sk_sp<PaintRecord> Paint(
      const CompositorPaintWorkletInput* compositor_input,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&
          animated_property_values) override {
    if (animated_property_values.empty())
      return nullptr;
    const BackgroundColorPaintWorkletInput* input =
        static_cast<const BackgroundColorPaintWorkletInput*>(compositor_input);
    FloatSize container_size = input->ContainerSize();
    Vector<Color> animated_colors = input->AnimatedColors();
    Vector<double> offsets = input->Offsets();
    DCHECK_GT(animated_colors.size(), 1u);
    DCHECK_EQ(animated_colors.size(), offsets.size());

    DCHECK_EQ(animated_property_values.size(), 1u);
    const auto& entry = animated_property_values.begin();
    float progress = entry->second.float_value.value();

    // Get the start and end color based on the progress and offsets.
    DCHECK_EQ(offsets.front(), 0);
    DCHECK_EQ(offsets.back(), 1);
    unsigned result_index = -1;
    for (unsigned i = 0; i < offsets.size() - 1; i++) {
      if (progress <= offsets[i + 1]) {
        result_index = i;
        break;
      }
    }
    DCHECK_GE(result_index, 0u);
    // Because the progress is a global one, we need to adjust it with offsets.
    float adjusted_progress =
        (progress - offsets[result_index]) /
        (offsets[result_index + 1] - offsets[result_index]);
    std::unique_ptr<InterpolableValue> from =
        CSSColorInterpolationType::CreateInterpolableColor(
            animated_colors[result_index]);
    std::unique_ptr<InterpolableValue> to =
        CSSColorInterpolationType::CreateInterpolableColor(
            animated_colors[result_index + 1]);
    std::unique_ptr<InterpolableValue> result =
        CSSColorInterpolationType::CreateInterpolableColor(
            animated_colors[result_index + 1]);
    from->Interpolate(*to, adjusted_progress, *result);
    Color rgba = CSSColorInterpolationType::GetRGBA(*(result.get()));
    SkColor current_color = static_cast<SkColor>(rgba);

    PaintRenderingContext2DSettings* context_settings =
        PaintRenderingContext2DSettings::Create();
    auto* rendering_context = MakeGarbageCollected<PaintRenderingContext2D>(
        RoundedIntSize(container_size), context_settings, 1, 1);
    rendering_context->GetPaintCanvas()->drawColor(current_color);
    return rendering_context->GetRecord();
  }
};

// TODO(crbug.com/1163949): Support animation keyframes without 0% or 100%.
// Returns false if we cannot successfully get the animated color.
bool GetColorsFromStringKeyframe(const PropertySpecificKeyframe* frame,
                                 Vector<Color>* animated_colors,
                                 const Element* element) {
  DCHECK(frame->IsCSSPropertySpecificKeyframe());
  const CSSValue* value = To<CSSPropertySpecificKeyframe>(frame)->Value();
  if (!value)
    return false;
  const CSSPropertyName property_name =
      CSSPropertyName(CSSPropertyID::kBackgroundColor);
  const CSSValue* computed_value = StyleResolver::ComputeValue(
      const_cast<Element*>(element), property_name, *value);
  DCHECK(computed_value->IsColorValue());
  const cssvalue::CSSColorValue* color_value =
      static_cast<const cssvalue::CSSColorValue*>(computed_value);
  animated_colors->push_back(color_value->Value());
  return true;
}

// Returns false if we cannot successfully get the animated color.
bool GetColorsFromTransitionKeyframe(const PropertySpecificKeyframe* frame,
                                     Vector<Color>* animated_colors,
                                     const Element* element) {
  DCHECK(frame->IsTransitionPropertySpecificKeyframe());
  const TransitionKeyframe::PropertySpecificKeyframe* keyframe =
      To<TransitionKeyframe::PropertySpecificKeyframe>(frame);
  InterpolableValue* value =
      keyframe->GetValue()->Value().interpolable_value.get();
  if (!value)
    return false;
  const InterpolableList& list = To<InterpolableList>(*value);
  // Only the first one has the real value.
  Color rgba = CSSColorInterpolationType::GetRGBA(*(list.Get(0)));
  animated_colors->push_back(rgba);
  return true;
}

void GetCompositorKeyframeOffset(const PropertySpecificKeyframe* frame,
                                 Vector<double>* offsets) {
  const CompositorKeyframeDouble& value =
      To<CompositorKeyframeDouble>(*(frame->GetCompositorKeyframeValue()));
  offsets->push_back(value.ToDouble());
}

bool GetBGColorPaintWorkletParamsInternal(Element* element,
                                          Vector<Color>* animated_colors,
                                          Vector<double>* offsets) {
  if (!element->GetElementAnimations())
    return false;
  // We composite the background color animation that has the highest composite
  // order. Or if we have only one animation on background color and other
  // animation(s) are on other properties, then we can also composite the
  // background color animation.
  Animation* composited_animation = nullptr;
  for (const auto& animation : element->GetElementAnimations()->Animations()) {
    if (animation.key->CalculateAnimationPlayState() == Animation::kIdle ||
        !animation.key->Affects(*element, GetCSSPropertyBackgroundColor()))
      continue;
    animation.key->SetDidBGColorAnimFallBack();
    if (!composited_animation ||
        Animation::HasLowerCompositeOrdering(
            composited_animation, animation.key,
            Animation::CompareAnimationsOrdering::kPointerOrder))
      composited_animation = animation.key;
  }
  DCHECK(composited_animation);

  // If we are here, then this element must have one background color animation
  // only. Fall back to the main thread if it is not composite:replace.
  const AnimationEffect* effect = composited_animation->effect();
  DCHECK(effect->IsKeyframeEffect());
  const KeyframeEffectModelBase* model =
      static_cast<const KeyframeEffect*>(effect)->Model();
  if (model->Composite() != EffectModel::kCompositeReplace)
    return false;
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyBackgroundColor()));
  DCHECK_GE(frames->size(), 2u);
  for (const auto& frame : *frames) {
    if (model->IsStringKeyframeEffectModel()) {
      if (!GetColorsFromStringKeyframe(frame, animated_colors, element))
        return false;
    } else {
      if (!GetColorsFromTransitionKeyframe(frame, animated_colors, element))
        return false;
    }
    GetCompositorKeyframeOffset(frame, offsets);
  }
  composited_animation->ResetDidBGColorAnimFallBack();
  return true;
}

}  // namespace

// static
BackgroundColorPaintWorklet* BackgroundColorPaintWorklet::Create(
    LocalFrame& local_root) {
  return MakeGarbageCollected<BackgroundColorPaintWorklet>(local_root);
}

BackgroundColorPaintWorklet::BackgroundColorPaintWorklet(LocalFrame& local_root)
    : NativePaintWorklet(local_root) {
  // This is called only once per document.
  BackgroundColorPaintWorkletProxyClient* client =
      BackgroundColorPaintWorkletProxyClient::Create(worklet_id_);
  RegisterProxyClient(client);
}

BackgroundColorPaintWorklet::~BackgroundColorPaintWorklet() = default;

scoped_refptr<Image> BackgroundColorPaintWorklet::Paint(
    const FloatSize& container_size,
    const Node* node,
    const Vector<Color>& animated_colors,
    const Vector<double>& offsets) {
  node->GetLayoutObject()->GetMutableForPainting().EnsureId();
  CompositorElementId element_id = CompositorElementIdFromUniqueObjectId(
      node->GetLayoutObject()->UniqueId(),
      CompositorAnimations::CompositorElementNamespaceForProperty(
          CSSPropertyID::kBackgroundColor));
  CompositorPaintWorkletInput::PropertyKeys input_property_keys;
  input_property_keys.emplace_back(
      CompositorPaintWorkletInput::NativePropertyType::kBackgroundColor,
      element_id);
  scoped_refptr<BackgroundColorPaintWorkletInput> input =
      base::MakeRefCounted<BackgroundColorPaintWorkletInput>(
          container_size, worklet_id_, animated_colors, offsets,
          std::move(input_property_keys));
  return PaintWorkletDeferredImage::Create(std::move(input), container_size);
}

bool BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
    Node* node,
    Vector<Color>* animated_colors,
    Vector<double>* offsets) {
  DCHECK(node->IsElementNode());
  Element* element = static_cast<Element*>(node);
  return GetBGColorPaintWorkletParamsInternal(element, animated_colors,
                                              offsets);
}

sk_sp<PaintRecord> BackgroundColorPaintWorklet::ProxyClientPaintForTest() {
  FloatSize container_size(100, 100);
  Vector<Color> animated_colors = {Color(0, 255, 0), Color(255, 0, 0)};
  Vector<double> offsets = {0, 1};
  CompositorPaintWorkletInput::PropertyKeys property_keys;
  scoped_refptr<BackgroundColorPaintWorkletInput> input =
      base::MakeRefCounted<BackgroundColorPaintWorkletInput>(
          container_size, 1u, animated_colors, offsets, property_keys);
  CompositorPaintWorkletJob::AnimatedPropertyValues property_values;
  BackgroundColorPaintWorkletProxyClient* client =
      BackgroundColorPaintWorkletProxyClient::Create(1u);
  return client->Paint(input.get(), property_values);
}

}  // namespace blink

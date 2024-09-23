// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/background_color_paint_definition.h"

#include "cc/paint/paint_recorder.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"
#include "third_party/blink/renderer/core/css/cssom/style_property_map_read_only.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {

using ColorKeyframe = NativeCssPaintDefinition::TypedKeyframe<Color>;
using ColorKeyframeVector = Vector<ColorKeyframe>;

Color InterpolateColor(unsigned index,
                       double progress,
                       const ColorKeyframeVector& keyframes) {
  Color first = keyframes[index].value;
  Color second = keyframes[index + 1].value;

  // Interpolation is in legacy srgb if and only if both endpoints are legacy
  // srgb. Otherwise, use OkLab for interpolation.
  if (first.GetColorSpace() != Color::ColorSpace::kSRGBLegacy ||
      second.GetColorSpace() != Color::ColorSpace::kSRGBLegacy) {
    first.ConvertToColorSpace(Color::ColorSpace::kOklab);
    second.ConvertToColorSpace(Color::ColorSpace::kOklab);
  }

  return Color::InterpolateColors(first.GetColorSpace(), std::nullopt, first,
                                  second, progress);
}

// Check for ancestor node with filter that moves pixels. The compositor cannot
// easily track the filters applied within a layer (i.e. composited filters) and
// is unable to expand the damage rect. To workaround this, we want to disallow
// composited background animations if there are decomposited filters, but we do
// not know that at this stage of the pipeline.  Therefore, we simple disallow
// any pixel moving filters between this object and the nearest ancestor known
// to be composited.
bool CompositorMayHaveIncorrectDamageRect(const Element* element) {
  LayoutObject* layout_object = element->GetLayoutObject();
  DCHECK(layout_object);
  auto& first_fragment =
      layout_object->EnclosingLayer()->GetLayoutObject().FirstFragment();
  if (!first_fragment.HasLocalBorderBoxProperties())
    return true;

  auto paint_properties = first_fragment.LocalBorderBoxProperties();
  for (const auto* effect = &paint_properties.Effect().Unalias(); effect;
       effect = effect->UnaliasedParent()) {
    if (effect->HasDirectCompositingReasons())
      break;
    if (effect->HasFilterThatMovesPixels())
      return true;
  }

  return false;
}

// This class includes information that is required by the compositor thread
// when painting background color.
class BackgroundColorPaintWorkletInput : public PaintWorkletInput {
 public:
  BackgroundColorPaintWorkletInput(
      const gfx::SizeF& container_size,
      int worklet_id,
      ColorKeyframeVector keyframes,
      const std::optional<double>& main_thread_progress,
      cc::PaintWorkletInput::PropertyKeys property_keys)
      : PaintWorkletInput(container_size, worklet_id, std::move(property_keys)),
        keyframes_(std::move(keyframes)),
        main_thread_progress_(main_thread_progress) {
    for (const auto& item : keyframes_) {
      if (!item.value.IsOpaque()) {
        is_opaque_ = false;
        break;
      }
    }
  }

  ~BackgroundColorPaintWorkletInput() override = default;

  const ColorKeyframeVector& keyframes() const { return keyframes_; }
  const std::optional<double>& MainThreadProgress() const {
    return main_thread_progress_;
  }
  bool KnownToBeOpaque() const override { return is_opaque_; }

  PaintWorkletInputType GetType() const override {
    return PaintWorkletInputType::kBackgroundColor;
  }

 private:
  ColorKeyframeVector keyframes_;
  std::optional<double> main_thread_progress_;
  bool is_opaque_ = true;
};

Color GetColorFromKeyframe(const PropertySpecificKeyframe* frame,
                           const KeyframeEffectModelBase* model,
                           const Element* element) {
  if (model->IsStringKeyframeEffectModel()) {
    const CSSValue* value = To<CSSPropertySpecificKeyframe>(frame)->Value();
    const CSSPropertyName property_name =
        CSSPropertyName(CSSPropertyID::kBackgroundColor);
    const CSSValue* computed_value = StyleResolver::ComputeValue(
        const_cast<Element*>(element), property_name, *value);
    auto& color_value = To<cssvalue::CSSColor>(*computed_value);
    return color_value.Value();
  }

  const auto* keyframe =
      To<TransitionKeyframe::PropertySpecificKeyframe>(frame);
  InterpolableValue* value =
      keyframe->GetValue()->Value().interpolable_value.Get();

  const auto& list = To<InterpolableList>(*value);
  DCHECK(CSSColorInterpolationType::IsNonKeywordColor(*(list.Get(0))));

  return CSSColorInterpolationType::GetColor(*(list.Get(0)));
}

void ExtractKeyframes(const Element* element,
                      const Animation* compositable_animation,
                      ColorKeyframeVector& color_keyframes) {
  element->GetLayoutObject()->GetMutableForPainting().EnsureId();
  const AnimationEffect* effect = compositable_animation->effect();
  const KeyframeEffectModelBase* model = To<KeyframeEffect>(effect)->Model();
  DCHECK_EQ(model->Composite(), EffectModel::kCompositeReplace);
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyBackgroundColor()));
  for (const auto& frame : *frames) {
    Color color = GetColorFromKeyframe(frame, model, element);
    double offset = frame->Offset();
    std::unique_ptr<gfx::TimingFunction> timing_function_copy;
    const TimingFunction& timing_function = frame->Easing();
    // LinearTimingFunction::CloneToCC() returns nullptr as it is shared.
    timing_function_copy = timing_function.CloneToCC();
    color_keyframes.push_back(
        ColorKeyframe(offset, timing_function_copy, color));
  }
}

bool ValidateColorValue(const Element* element,
                        const CSSValue* value,
                        const InterpolableValue* interpolable_value) {
  if (value) {
    if (value->IsIdentifierValue()) {
      CSSValueID value_id = To<CSSIdentifierValue>(value)->GetValueID();
      if (StyleColor::IsSystemColorIncludingDeprecated(value_id)) {
        // The color depends on the color-scheme. Though we can resolve the
        // color values, we presently lack a method to update the colors should
        // the color-scheme change during the course of the animation.
        // TODO(crbug.com/40795239): handle system color.
        return false;
      }
      if (value_id == CSSValueID::kCurrentcolor) {
        // Do not composite a background color animation that depends on
        // currentcolor until we have a mechanism to update the compositor
        // keyframes when currentcolor changes.
        return false;
      }
    } else if (value->IsColorMixValue()) {
      const cssvalue::CSSColorMixValue* color_mix =
          To<cssvalue::CSSColorMixValue>(value);
      if (!ValidateColorValue(element, &color_mix->Color1(), nullptr) ||
          !ValidateColorValue(element, &color_mix->Color2(), nullptr)) {
        // Unresolved color mix or a color mix with a system color dependency.
        // Either way, fall back to main.
        return false;
      }
    }

    const CSSPropertyName property_name =
        CSSPropertyName(CSSPropertyID::kBackgroundColor);
    const CSSValue* computed_value = StyleResolver::ComputeValue(
        const_cast<Element*>(element), property_name, *value);
    return computed_value->IsColorValue();
  } else if (interpolable_value) {
    // Transition keyframes store a pair of color values: one for the actual
    // color and one for the reported color (conditionally resolved). This is to
    // prevent JavaScript code from snooping the visited status of links. The
    // color to use for the animation is stored first in the list.
    // We need to further check that the color is a simple RGBA color and does
    // not require blending with other colors (e.g. currentcolor).
    if (!interpolable_value->IsList())
      return false;

    const InterpolableList& list = To<InterpolableList>(*interpolable_value);
    return CSSColorInterpolationType::IsNonKeywordColor(*(list.Get(0)));
  }
  return false;
}

}  // namespace

template <>
struct DowncastTraits<BackgroundColorPaintWorkletInput> {
  static bool AllowFrom(const cc::PaintWorkletInput& worklet_input) {
    auto* input = DynamicTo<PaintWorkletInput>(worklet_input);
    return input && AllowFrom(*input);
  }

  static bool AllowFrom(const PaintWorkletInput& worklet_input) {
    return worklet_input.GetType() ==
           PaintWorkletInput::PaintWorkletInputType::kBackgroundColor;
  }
};

Animation* BackgroundColorPaintDefinition::GetAnimationIfCompositable(
    const Element* element) {
  if (CompositorMayHaveIncorrectDamageRect(element))
    return nullptr;

  return GetAnimationForProperty(element, GetCSSPropertyBackgroundColor(),
                                 ValidateColorValue);
}

// static
BackgroundColorPaintDefinition* BackgroundColorPaintDefinition::Create(
    LocalFrame& local_root) {
  if (!WebLocalFrameImpl::FromFrame(local_root))
    return nullptr;
  return MakeGarbageCollected<BackgroundColorPaintDefinition>(local_root);
}

BackgroundColorPaintDefinition::BackgroundColorPaintDefinition(
    LocalFrame& local_root)
    : NativeCssPaintDefinition(
          &local_root,
          PaintWorkletInput::PaintWorkletInputType::kBackgroundColor) {}

PaintRecord BackgroundColorPaintDefinition::Paint(
    const CompositorPaintWorkletInput* compositor_input,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  const auto* input = To<BackgroundColorPaintWorkletInput>(compositor_input);
  KeyframeIndexAndProgress keyframe_index_and_progress =
      ComputeKeyframeIndexAndProgress(input->MainThreadProgress(),
                                      animated_property_values,
                                      input->keyframes());

  Color color = InterpolateColor(keyframe_index_and_progress.index,
                                 keyframe_index_and_progress.progress,
                                 input->keyframes());

  // TODO(crbug/1308932): Remove toSkColor4f and make all SkColor4f.
  SkColor4f current_color = color.toSkColor4f();

  cc::InspectablePaintRecorder paint_recorder;
  // When render this element, we always do pixel snapping to its nearest pixel,
  // therefore we use rounded |container_size| to create the rendering context.
  const gfx::Size container_size(gfx::ToRoundedSize(input->ContainerSize()));
  cc::PaintCanvas* canvas = paint_recorder.beginRecording(container_size);
  canvas->drawColor(current_color);
  return paint_recorder.finishRecordingAsPicture();
}

scoped_refptr<Image> BackgroundColorPaintDefinition::Paint(
    const gfx::SizeF& container_size,
    const Node* node) {
  const Element* element = To<Element>(node);
  Animation* compositable_animation = GetAnimationIfCompositable(element);
  if (!compositable_animation) {
    return nullptr;
  }

  ColorKeyframeVector color_keyframes;
  ExtractKeyframes(element, compositable_animation, color_keyframes);

  CompositorElementId element_id = CompositorElementIdFromUniqueObjectId(
      node->GetLayoutObject()->UniqueId(),
      CompositorAnimations::CompositorElementNamespaceForProperty(
          CSSPropertyID::kBackgroundColor));
  CompositorPaintWorkletInput::PropertyKeys input_property_keys;
  input_property_keys.emplace_back(
      CompositorPaintWorkletInput::NativePropertyType::kBackgroundColor,
      element_id);

  std::optional<double> main_thread_progress =
      compositable_animation->effect()->Progress();

  scoped_refptr<BackgroundColorPaintWorkletInput> input =
      base::MakeRefCounted<BackgroundColorPaintWorkletInput>(
          container_size, worklet_id_, std::move(color_keyframes),
          main_thread_progress, std::move(input_property_keys));
  return PaintWorkletDeferredImage::Create(std::move(input), container_size);
}

PaintRecord BackgroundColorPaintDefinition::PaintForTest(
    const Vector<Color>& animated_colors,
    const Vector<double>& offsets,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  gfx::SizeF container_size(100, 100);
  std::optional<double> progress = 0;
  CompositorPaintWorkletInput::PropertyKeys property_keys;
  ColorKeyframeVector color_keyframes;
  for (unsigned i = 0; i < animated_colors.size(); i++) {
    std::unique_ptr<gfx::TimingFunction> tf;
    color_keyframes.push_back(
        TypedKeyframe<Color>(offsets[i], tf, animated_colors[i]));
  }

  scoped_refptr<BackgroundColorPaintWorkletInput> input =
      base::MakeRefCounted<BackgroundColorPaintWorkletInput>(
          container_size, 1u, std::move(color_keyframes), progress,
          std::move(property_keys));
  return Paint(input.get(), animated_property_values);
}

void BackgroundColorPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink

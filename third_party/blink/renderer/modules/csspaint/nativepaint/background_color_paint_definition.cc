// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/background_color_paint_definition.h"

#include "cc/paint/paint_recorder.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/compositor_animation_color_curve.h"
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
  if (element->GetDocument().Lifecycle().GetState() <=
      DocumentLifecycle::kInPrePaint) {
    return false;
  }

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
      scoped_refptr<CompositorAnimationColorCurve> color_curve,
      Color main_thread_value,
      cc::PaintWorkletInput::PropertyKeys property_keys)
      : PaintWorkletInput(container_size, worklet_id, std::move(property_keys)),
        color_curve_(std::move(color_curve)),
        main_thread_value_(main_thread_value) {}

  ~BackgroundColorPaintWorkletInput() override = default;

  const Color& MainThreadValue() const { return main_thread_value_; }
  bool KnownToBeOpaque() const override { return color_curve_->IsOpaque(); }

  Color Interpolate(double progress) const {
    return color_curve_->Interpolate(progress);
  }

  PaintWorkletInputType GetType() const override {
    return PaintWorkletInputType::kBackgroundColor;
  }

 private:
  scoped_refptr<CompositorAnimationColorCurve> color_curve_;
  Color main_thread_value_;
};

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
  ElementAnimations* element_animations = element->GetElementAnimations();
  if (!element_animations) {
    return nullptr;
  }

  NativePaintWorkletData* npw_data =
      element_animations->GetBackgroundColorNpwData();
  if (!npw_data) {
    return nullptr;
  }

  Animation* candidate = npw_data->GetAnimation();
  if (!candidate) {
    return nullptr;
  }

  // TODO(crbug.com/40901295): Support start delay.
  AnimationTimeDelta start_delay =
      candidate->effect()->SpecifiedTiming().start_delay.AsTimeValue();
  if (start_delay.InSecondsF() > 0.f) {
    return nullptr;
  }

  if (CompositorMayHaveIncorrectDamageRect(element)) {
    return nullptr;
  }

  if (npw_data->GetAnimationCurve() != nullptr) {
    // The keyframes have already been validated.
    return candidate;
  }

  scoped_refptr<CompositorAnimationColorCurve> color_curve =
      CompositorAnimationColorCurve::Create(
          candidate, CSSPropertyName(CSSPropertyID::kBackgroundColor));
  if (color_curve) {
    npw_data->SetAnimationCurve(std::move(color_curve));
    return candidate;
  }

  return nullptr;
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

  Color color = input->MainThreadValue();
  if (!animated_property_values.empty()) {
    DCHECK_EQ(animated_property_values.size(), 1u);
    const auto& entry = animated_property_values.begin();
    double progress = entry->second.float_value.value();
    color = input->Interpolate(progress);
  }

  // TODO(crbug/1308932): Remove toSkColor4f and make all SkColor4f.
  SkColor4f sk_color = color.toSkColor4f();

  cc::InspectablePaintRecorder paint_recorder;
  // When render this element, we always do pixel snapping to its nearest pixel,
  // therefore we use rounded |container_size| to create the rendering context.
  const gfx::Size container_size(gfx::ToRoundedSize(input->ContainerSize()));
  cc::PaintCanvas* canvas = paint_recorder.beginRecording(container_size);
  canvas->drawColor(sk_color);
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

  element->GetLayoutObject()->GetMutableForPainting().EnsureId();

  ElementAnimations* element_animations = element->GetElementAnimations();
  NativePaintWorkletData* npw_data =
      element_animations->GetBackgroundColorNpwData();
  scoped_refptr<CompositorAnimationColorCurve> color_curve =
      base::WrapRefCounted(static_cast<CompositorAnimationColorCurve*>(
          npw_data->GetAnimationCurve().get()));
  CompositorElementId element_id = CompositorElementIdFromUniqueObjectId(
      node->GetLayoutObject()->UniqueId(),
      CompositorAnimations::CompositorElementNamespaceForProperty(
          CSSPropertyID::kBackgroundColor));
  CompositorPaintWorkletInput::PropertyKeys input_property_keys;
  input_property_keys.emplace_back(
      CompositorPaintWorkletInput::NativePropertyType::kBackgroundColor,
      element_id);

  Color main_thread_value =
      element->GetLayoutObject()->ResolveColor(GetCSSPropertyBackgroundColor());

  scoped_refptr<BackgroundColorPaintWorkletInput> input =
      base::MakeRefCounted<BackgroundColorPaintWorkletInput>(
          container_size, worklet_id_, std::move(color_curve),
          main_thread_value, std::move(input_property_keys));
  return PaintWorkletDeferredImage::Create(std::move(input), container_size);
}

PaintRecord BackgroundColorPaintDefinition::PaintForTest(
    const Vector<Color>& animated_colors,
    const Vector<double>& offsets,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  gfx::SizeF container_size(100, 100);
  CompositorPaintWorkletInput::PropertyKeys property_keys;
  scoped_refptr color_curve = CompositorAnimationColorCurve::CreateForTesting(
      animated_colors, offsets,
      CSSPropertyName(CSSPropertyID::kBackgroundColor));
  scoped_refptr<BackgroundColorPaintWorkletInput> input =
      base::MakeRefCounted<BackgroundColorPaintWorkletInput>(
          container_size, 1u, std::move(color_curve), Color(),
          std::move(property_keys));
  return Paint(input.get(), animated_property_values);
}

void BackgroundColorPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink

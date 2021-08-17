// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/background_color_paint_definition.h"

#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"
#include "third_party/blink/renderer/core/css/cssom/style_property_map_read_only.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_id_generator.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

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
      const absl::optional<double>& progress,
      cc::PaintWorkletInput::PropertyKeys property_keys)
      : PaintWorkletInput(container_size, worklet_id, std::move(property_keys)),
        animated_colors_(animated_colors),
        offsets_(offsets),
        progress_(progress) {}

  ~BackgroundColorPaintWorkletInput() override = default;

  const Vector<Color>& AnimatedColors() const { return animated_colors_; }
  const Vector<double>& Offsets() const { return offsets_; }
  const absl::optional<double>& MainThreadProgress() const { return progress_; }

  PaintWorkletInputType GetType() const override {
    return PaintWorkletInputType::kBackgroundColor;
  }

 private:
  // TODO(xidachen): wrap these 3 into a structure.
  // animated_colors_: The colors extracted from animated keyframes.
  // offsets_: the offsets of the animated keyframes.
  // progress_: the progress obtained from the main thread animation.
  Vector<Color> animated_colors_;
  Vector<double> offsets_;
  absl::optional<double> progress_;
};

// TODO(crbug.com/1163949): Support animation keyframes without 0% or 100%.
// Returns false if we cannot successfully get the animated color.
void GetColorsFromKeyframe(const PropertySpecificKeyframe* frame,
                           const KeyframeEffectModelBase* model,
                           Vector<Color>* animated_colors,
                           const Element* element) {
  if (model->IsStringKeyframeEffectModel()) {
    DCHECK(frame->IsCSSPropertySpecificKeyframe());
    const CSSValue* value = To<CSSPropertySpecificKeyframe>(frame)->Value();
    const CSSPropertyName property_name =
        CSSPropertyName(CSSPropertyID::kBackgroundColor);
    const CSSValue* computed_value = StyleResolver::ComputeValue(
        const_cast<Element*>(element), property_name, *value);
    DCHECK(computed_value->IsColorValue());
    const cssvalue::CSSColor* color_value =
        static_cast<const cssvalue::CSSColor*>(computed_value);
    animated_colors->push_back(color_value->Value());
  } else {
    DCHECK(frame->IsTransitionPropertySpecificKeyframe());
    const TransitionKeyframe::PropertySpecificKeyframe* keyframe =
        To<TransitionKeyframe::PropertySpecificKeyframe>(frame);
    InterpolableValue* value =
        keyframe->GetValue()->Value().interpolable_value.get();
    const InterpolableList& list = To<InterpolableList>(*value);
    // Only the first one has the real value.
    Color rgba = CSSColorInterpolationType::GetRGBA(*(list.Get(0)));
    animated_colors->push_back(rgba);
  }
}

bool CanGetValueFromKeyframe(const PropertySpecificKeyframe* frame,
                             const KeyframeEffectModelBase* model) {
  if (model->IsStringKeyframeEffectModel()) {
    DCHECK(frame->IsCSSPropertySpecificKeyframe());
    const CSSValue* value = To<CSSPropertySpecificKeyframe>(frame)->Value();
    if (!value)
      return false;
  } else {
    DCHECK(frame->IsTransitionPropertySpecificKeyframe());
    const TransitionKeyframe::PropertySpecificKeyframe* keyframe =
        To<TransitionKeyframe::PropertySpecificKeyframe>(frame);
    InterpolableValue* value =
        keyframe->GetValue()->Value().interpolable_value.get();
    if (!value)
      return false;
  }
  return true;
}

void GetCompositorKeyframeOffset(const PropertySpecificKeyframe* frame,
                                 Vector<double>* offsets) {
  const CompositorKeyframeDouble& value =
      To<CompositorKeyframeDouble>(*(frame->GetCompositorKeyframeValue()));
  offsets->push_back(value.ToDouble());
}

bool GetBGColorPaintWorkletParamsInternal(
    Element* element,
    Vector<Color>* animated_colors,
    Vector<double>* offsets,
    absl::optional<double>* progress,
    const Animation* compositable_animation) {
  element->GetLayoutObject()->GetMutableForPainting().EnsureId();
  const AnimationEffect* effect = compositable_animation->effect();
  DCHECK(effect->IsKeyframeEffect());
  const KeyframeEffectModelBase* model =
      static_cast<const KeyframeEffect*>(effect)->Model();
  DCHECK_EQ(model->Composite(), EffectModel::kCompositeReplace);
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyBackgroundColor()));
  for (const auto& frame : *frames) {
    GetColorsFromKeyframe(frame, model, animated_colors, element);
    GetCompositorKeyframeOffset(frame, offsets);
  }
  *progress = compositable_animation->effect()->Progress();
  return true;
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
  if (!element->GetElementAnimations())
    return nullptr;
  Animation* compositable_animation = nullptr;
  // We'd composite the background-color only if it is the only background color
  // animation on this element.
  unsigned count = 0;
  for (const auto& animation : element->GetElementAnimations()->Animations()) {
    if (animation.key->CalculateAnimationPlayState() == Animation::kIdle ||
        !animation.key->Affects(*element, GetCSSPropertyBackgroundColor()))
      continue;
    count++;
    compositable_animation = animation.key;
  }
  if (!compositable_animation || count > 1)
    return nullptr;

  // If we are here, then this element must have one background color animation
  // only. Fall back to the main thread if it is not composite:replace.
  const AnimationEffect* effect = compositable_animation->effect();
  DCHECK(effect->IsKeyframeEffect());
  const KeyframeEffectModelBase* model =
      static_cast<const KeyframeEffect*>(effect)->Model();
  if (model->AffectedByUnderlyingAnimations())
    return nullptr;
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyBackgroundColor()));
  DCHECK_GE(frames->size(), 2u);
  for (const auto& frame : *frames) {
    if (!CanGetValueFromKeyframe(frame, model)) {
      return nullptr;
    }
  }
  return compositable_animation;
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
    : worklet_id_(PaintWorkletIdGenerator::NextId()) {
  DCHECK(local_root.IsLocalRoot());
  DCHECK(IsMainThread());
  ExecutionContext* context = local_root.DomWindow();
  FrameOrWorkerScheduler* scheduler =
      context ? context->GetScheduler() : nullptr;
  // TODO(crbug.com/1143407): We don't need this thread if we can make the
  // compositor thread support GC.
  ThreadCreationParams params(ThreadType::kAnimationAndPaintWorkletThread);
  worker_backing_thread_ = std::make_unique<WorkerBackingThread>(
      params.SetFrameOrWorkerScheduler(scheduler));
  auto startup_data = WorkerBackingThreadStartupData::CreateDefault();
  PostCrossThreadTask(
      *worker_backing_thread_->BackingThread().GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&WorkerBackingThread::InitializeOnBackingThread,
                          CrossThreadUnretained(worker_backing_thread_.get()),
                          startup_data));
  RegisterProxyClient(local_root);
}

void BackgroundColorPaintDefinition::RegisterProxyClient(
    LocalFrame& local_root) {
  proxy_client_ =
      PaintWorkletProxyClient::Create(local_root.DomWindow(), worklet_id_);
  proxy_client_->RegisterForNativePaintWorklet(
      worker_backing_thread_.get(), this,
      PaintWorkletInput::PaintWorkletInputType::kBackgroundColor);
}

void BackgroundColorPaintDefinition::UnregisterProxyClient() {
  proxy_client_->UnregisterForNativePaintWorklet();
}

sk_sp<PaintRecord> BackgroundColorPaintDefinition::Paint(
    const CompositorPaintWorkletInput* compositor_input,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  const BackgroundColorPaintWorkletInput* input =
      static_cast<const BackgroundColorPaintWorkletInput*>(compositor_input);
  FloatSize container_size = input->ContainerSize();
  Vector<Color> animated_colors = input->AnimatedColors();
  Vector<double> offsets = input->Offsets();
  DCHECK_GT(animated_colors.size(), 1u);
  DCHECK_EQ(animated_colors.size(), offsets.size());

  // TODO(crbug.com/1188760): We should handle the case when it is null, and
  // paint the original background-color retrieved from its style.
  float progress = input->MainThreadProgress().has_value()
                       ? input->MainThreadProgress().value()
                       : 0;
  // This would mean that the animation started on compositor, so we override
  // the progress that we obtained from the main thread.
  if (!animated_property_values.empty()) {
    DCHECK_EQ(animated_property_values.size(), 1u);
    const auto& entry = animated_property_values.begin();
    progress = entry->second.float_value.value();
  }

  // Get the start and end color based on the progress and offsets.
  unsigned result_index = offsets.size() - 1;
  if (progress <= 0) {
    result_index = 0;
  } else if (progress > 0 && progress < 1) {
    for (unsigned i = 0; i < offsets.size() - 1; i++) {
      if (progress <= offsets[i + 1]) {
        result_index = i;
        break;
      }
    }
  }
  if (result_index == offsets.size() - 1) {
    result_index = offsets.size() - 2;
  }
  // Because the progress is a global one, we need to adjust it with offsets.
  float adjusted_progress = (progress - offsets[result_index]) /
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

  // When render this element, we always do pixel snapping to its nearest pixel,
  // therefore we use rounded |container_size| to create the rendering context.
  IntSize rounded_size = RoundedIntSize(container_size);
  if (!context_ || context_->Width() != rounded_size.Width() ||
      context_->Height() != rounded_size.Height()) {
    PaintRenderingContext2DSettings* context_settings =
        PaintRenderingContext2DSettings::Create();
    context_ = MakeGarbageCollected<PaintRenderingContext2D>(
        rounded_size, context_settings, 1, 1);
  }
  context_->GetDrawingPaintCanvas()->drawColor(current_color);
  return context_->GetRecord();
}

scoped_refptr<Image> BackgroundColorPaintDefinition::Paint(
    const FloatSize& container_size,
    const Node* node,
    const Vector<Color>& animated_colors,
    const Vector<double>& offsets,
    const absl::optional<double>& progress) {
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
          container_size, worklet_id_, animated_colors, offsets, progress,
          std::move(input_property_keys));
  return PaintWorkletDeferredImage::Create(std::move(input), container_size);
}

bool BackgroundColorPaintDefinition::GetBGColorPaintWorkletParams(
    Node* node,
    Vector<Color>* animated_colors,
    Vector<double>* offsets,
    absl::optional<double>* progress) {
  DCHECK(node->IsElementNode());
  Element* element = static_cast<Element*>(node);
  Animation* compositable_animation = GetAnimationIfCompositable(element);
  if (!compositable_animation)
    return false;
  return GetBGColorPaintWorkletParamsInternal(element, animated_colors, offsets,
                                              progress, compositable_animation);
}

sk_sp<PaintRecord> BackgroundColorPaintDefinition::PaintForTest(
    const Vector<Color>& animated_colors,
    const Vector<double>& offsets,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  FloatSize container_size(100, 100);
  absl::optional<double> progress = 0;
  CompositorPaintWorkletInput::PropertyKeys property_keys;
  scoped_refptr<BackgroundColorPaintWorkletInput> input =
      base::MakeRefCounted<BackgroundColorPaintWorkletInput>(
          container_size, 1u, animated_colors, offsets, progress,
          property_keys);
  return Paint(input.get(), animated_property_values);
}

void BackgroundColorPaintDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(proxy_client_);
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink

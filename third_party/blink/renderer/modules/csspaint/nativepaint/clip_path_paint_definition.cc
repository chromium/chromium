// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_definition.h"

#include "cc/paint/paint_recorder.h"
#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/path_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

InterpolationValue CreateInterpolationValue(const BasicShape& basic_shape,
                                            float zoom);

// This class includes information that is required by the compositor thread
// when painting clip path.
class ClipPathPaintWorkletInput : public PaintWorkletInput {
 public:
  ClipPathPaintWorkletInput(
      const gfx::RectF& reference_box,
      const gfx::SizeF& clip_area_size,
      int worklet_id,
      float zoom,
      const Vector<scoped_refptr<BasicShape>>& animated_shapes,
      const Vector<double>& offsets,
      Vector<std::unique_ptr<gfx::TimingFunction>> timing_functions,
      const absl::optional<double>& progress,
      cc::PaintWorkletInput::PropertyKeys property_keys)
      : PaintWorkletInput(clip_area_size, worklet_id, std::move(property_keys)),
        zoom_(zoom),
        offsets_(offsets),
        timing_functions_(std::move(timing_functions)),
        progress_(progress),
        reference_box_(reference_box) {
    for (const auto& basic_shape : animated_shapes) {
      InterpolationValue interpolation_value =
          CreateInterpolationValue(*basic_shape.get(), zoom);
      DCHECK(interpolation_value);
      basic_shape_types_.push_back(basic_shape->GetType());
      interpolation_values_.push_back(std::move(interpolation_value));
    }
  }

  ~ClipPathPaintWorkletInput() override = default;

  const Vector<InterpolationValue>& InterpolationValues() const {
    return interpolation_values_;
  }
  const Vector<double>& Offsets() const { return offsets_; }
  double AdjustProgress(int keyframe, double progress) const {
    return timing_functions_[keyframe]->GetValue(progress);
  }
  const absl::optional<double>& MainThreadProgress() const { return progress_; }
  const Vector<BasicShape::ShapeType>& BasicShapeTypes() const {
    return basic_shape_types_;
  }

  float Zoom() const { return zoom_; }

  PaintWorkletInputType GetType() const override {
    return PaintWorkletInputType::kClipPath;
  }

  gfx::RectF GetReferenceBox() const { return reference_box_; }

 private:
  float zoom_;
  Vector<BasicShape::ShapeType> basic_shape_types_;
  Vector<InterpolationValue> interpolation_values_;
  Vector<double> offsets_;
  // TODO(crbug.com/1374390): Refactor composited animations so that
  // custom timing functions work for bgcolor animations as well
  // animations. This class should be refactored so that the necessary
  // properties exist in both this and Background Color paint worklet input
  Vector<std::unique_ptr<gfx::TimingFunction>> timing_functions_;
  absl::optional<double> progress_;
  gfx::RectF reference_box_;
};

InterpolationValue CreateInterpolationValue(const BasicShape& basic_shape,
                                            float zoom) {
  if (basic_shape.GetType() == BasicShape::kStylePathType) {
    return PathInterpolationFunctions::ConvertValue(
        To<StylePath>(&basic_shape),
        PathInterpolationFunctions::kForceAbsolute);
  }
  return basic_shape_interpolation_functions::MaybeConvertBasicShape(
      &basic_shape, zoom);
}

bool ShapesAreCompatible(const NonInterpolableValue& a,
                         BasicShape::ShapeType a_type,
                         const NonInterpolableValue& b,
                         BasicShape::ShapeType b_type) {
  if (a_type == BasicShape::kStylePathType &&
      b_type == BasicShape::kStylePathType)
    return PathInterpolationFunctions::PathsAreCompatible(a, b);

  if (a_type != BasicShape::kStylePathType &&
      b_type != BasicShape::kStylePathType)
    return basic_shape_interpolation_functions::ShapesAreCompatible(a, b);

  return false;
}

scoped_refptr<BasicShape> CreateBasicShape(
    BasicShape::ShapeType type,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue& untyped_non_interpolable_value) {
  if (type == BasicShape::kStylePathType) {
    return PathInterpolationFunctions::AppliedValue(
        interpolable_value, &untyped_non_interpolable_value);
  }

  CSSToLengthConversionData conversion_data;
  return basic_shape_interpolation_functions::CreateBasicShape(
      interpolable_value, untyped_non_interpolable_value, conversion_data);
}

scoped_refptr<BasicShape> GetAnimatedShapeFromKeyframe(
    const PropertySpecificKeyframe* frame,
    const KeyframeEffectModelBase* model,
    const Element* element) {
  scoped_refptr<BasicShape> basic_shape;
  if (model->IsStringKeyframeEffectModel()) {
    DCHECK(frame->IsCSSPropertySpecificKeyframe());
    const CSSValue* value =
        static_cast<const CSSPropertySpecificKeyframe*>(frame)->Value();
    const CSSPropertyName property_name =
        CSSPropertyName(CSSPropertyID::kClipPath);
    const CSSValue* computed_value = StyleResolver::ComputeValue(
        const_cast<Element*>(element), property_name, *value);
    StyleResolverState state(element->GetDocument(),
                             *const_cast<Element*>(element));
    basic_shape = BasicShapeForValue(state, *computed_value);
  } else {
    DCHECK(frame->IsTransitionPropertySpecificKeyframe());
    const TransitionKeyframe::PropertySpecificKeyframe* keyframe =
        To<TransitionKeyframe::PropertySpecificKeyframe>(frame);
    const NonInterpolableValue* non_interpolable_value =
        keyframe->GetValue()->Value().non_interpolable_value.get();
    BasicShape::ShapeType type =
        PathInterpolationFunctions::IsPathNonInterpolableValue(
            *non_interpolable_value)
            ? BasicShape::kStylePathType
            // This can be any shape but kStylePathType. This is needed to
            // distinguish between Path shape and other shapes in
            // CreateBasicShape function.
            : BasicShape::kBasicShapeCircleType;
    basic_shape = CreateBasicShape(
        type, *keyframe->GetValue()->Value().interpolable_value.get(),
        *non_interpolable_value);
  }
  DCHECK(basic_shape);
  return basic_shape;
}

bool ValidateClipPathValue(const Element* element,
                           const CSSValue* value,
                           const InterpolableValue* interpolable_value) {
  if (value) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    // Don't try to composite animations with clip-path: none, as this is not
    // compatible with the method used to paint composite clip path animations:
    // A mask image would potentially clip content unless if it was the size of
    // the entire viewport.
    if (identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kNone) {
      return false;
    }

    return true;
  } else if (interpolable_value) {
    // There is no need to check for clip-path: none here, as transitions are
    // not defined for this non-interpolable value. See
    // CSSAnimations::CalculateTransitionUpdateForPropertyHandle and
    // basic_shape_interpolation_functions::MaybeConvertBasicShape
    return true;
  }
  return false;
}

scoped_refptr<ShapeClipPathOperation> InterpolateShapes(
    const InterpolationValue& from,
    const BasicShape::ShapeType from_shape_type,
    const InterpolationValue& to,
    const BasicShape::ShapeType to_shape_type,
    const float progress) {
  scoped_refptr<BasicShape> result_shape;
  if (ShapesAreCompatible(*from.non_interpolable_value.get(), from_shape_type,
                          *to.non_interpolable_value.get(), to_shape_type)) {
    std::unique_ptr<InterpolableValue> result_interpolable_value =
        from.interpolable_value->Clone();
    from.interpolable_value->Interpolate(*to.interpolable_value, progress,
                                         *result_interpolable_value);
    result_shape = CreateBasicShape(from_shape_type, *result_interpolable_value,
                                    *from.non_interpolable_value);
  } else if (progress < 0.5) {
    result_shape = CreateBasicShape(from_shape_type, *from.interpolable_value,
                                    *from.non_interpolable_value);
  } else {
    result_shape = CreateBasicShape(to_shape_type, *to.interpolable_value,
                                    *to.non_interpolable_value);
  }
  return ShapeClipPathOperation::Create(result_shape);
}

double GetLocalProgress(double global_progress,
                        double first_offset,
                        double next_offset) {
  return (global_progress - first_offset) / (next_offset - first_offset);
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

// TODO(crbug.com/1248605): Introduce helper functions commonly used by
// background-color and clip-path animations.
Animation* ClipPathPaintDefinition::GetAnimationIfCompositable(
    const Element* element) {
  return GetAnimationForProperty(element, GetCSSPropertyClipPath(),
                                 ValidateClipPathValue);
}

// static
ClipPathPaintDefinition* ClipPathPaintDefinition::Create(
    LocalFrame& local_root) {
  return MakeGarbageCollected<ClipPathPaintDefinition>(local_root);
}

ClipPathPaintDefinition::ClipPathPaintDefinition(LocalFrame& local_root)
    : NativeCssPaintDefinition(
          &local_root,
          PaintWorkletInput::PaintWorkletInputType::kClipPath) {}

PaintRecord ClipPathPaintDefinition::Paint(
    const CompositorPaintWorkletInput* compositor_input,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  const auto* input = To<ClipPathPaintWorkletInput>(compositor_input);

  const Vector<InterpolationValue>& interpolation_values =
      input->InterpolationValues();
  Vector<BasicShape::ShapeType> basic_shape_types = input->BasicShapeTypes();
  // TODO(crbug.com/1374390): This section is a good candidate as a shared
  // helper function with bgcolor.
  Vector<double> offsets = input->Offsets();
  DCHECK_GT(interpolation_values.size(), 1u);
  DCHECK_EQ(interpolation_values.size(), offsets.size());

  // TODO(crbug.com/1188760): We should handle the case when it is null, and
  // paint the original clip-path retrieved from its style.
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

  // Get the start and end clip-path based on the progress and offsets.
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
  if (result_index == offsets.size() - 1)
    result_index = offsets.size() - 2;

  // Use offsets to calculate for intra-keyframe progress.
  float local_progress = GetLocalProgress(progress, offsets[result_index],
                                          offsets[result_index + 1]);
  // Adjust for that keyframe's timing function
  float adjusted_progress = input->AdjustProgress(result_index, local_progress);

  const InterpolationValue& from = interpolation_values[result_index];
  const InterpolationValue& to = interpolation_values[result_index + 1];

  scoped_refptr<ShapeClipPathOperation> current_shape =
      InterpolateShapes(from, basic_shape_types[result_index], to,
                        basic_shape_types[result_index + 1], adjusted_progress);

  const gfx::RectF reference_box = input->GetReferenceBox();
  Path path = current_shape->GetPath(reference_box, input->Zoom());

  cc::InspectablePaintRecorder paint_recorder;
  const gfx::Size clip_area_size(gfx::ToRoundedSize(input->ContainerSize()));
  cc::PaintCanvas* canvas = paint_recorder.beginRecording(clip_area_size);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  canvas->drawPath(path.GetSkPath(), flags);

  return paint_recorder.finishRecordingAsPicture();
}

// Creates a deferred image of size clip_area_size that will be painted via
// paint worklet. The clip paths will be scaled and translated according to
// reference_box.
scoped_refptr<Image> ClipPathPaintDefinition::Paint(
    float zoom,
    const gfx::RectF& reference_box,
    const gfx::SizeF& clip_area_size,
    const Node& node) {
  DCHECK(node.IsElementNode());
  const Element* element = To<Element>(&node);

  Vector<scoped_refptr<BasicShape>> animated_shapes;
  Vector<double> offsets;
  absl::optional<double> progress;

  Animation* animation = GetAnimationIfCompositable(element);
  // If we are here the animation must be compositable.
  DCHECK(animation);

  const AnimationEffect* effect = animation->effect();
  DCHECK(effect->IsKeyframeEffect());

  const KeyframeEffectModelBase* model =
      static_cast<const KeyframeEffect*>(effect)->Model();

  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyClipPath()));

  Vector<std::unique_ptr<gfx::TimingFunction>> timing_functions;

  for (const auto& frame : *frames) {
    animated_shapes.push_back(
        GetAnimatedShapeFromKeyframe(frame, model, element));
    offsets.push_back(frame->Offset());

    const TimingFunction& timing_function = frame->Easing();
    // LinearTimingFunction::CloneToCC() returns nullptr as it is shared.
    if (timing_function.GetType() == TimingFunction::Type::LINEAR) {
      timing_functions.push_back(gfx::LinearTimingFunction::Create());
    } else {
      timing_functions.push_back(timing_function.CloneToCC());
    }
  }
  progress = effect->Progress();

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
          reference_box, clip_area_size, worklet_id_, zoom, animated_shapes,
          offsets, std::move(timing_functions), progress,
          std::move(input_property_keys));

  return PaintWorkletDeferredImage::Create(std::move(input), clip_area_size);
}

gfx::RectF ClipPathPaintDefinition::ClipAreaRect(
    const Node& node,
    const gfx::RectF& reference_box,
    float zoom) {
  DCHECK(node.IsElementNode());
  const Element* element = To<Element>(&node);

  const Animation* animation = GetAnimationIfCompositable(element);
  DCHECK(animation);

  const AnimationEffect* effect = animation->effect();
  DCHECK(effect->IsKeyframeEffect());
  const KeyframeEffectModelBase* model =
      static_cast<const KeyframeEffect*>(effect)->Model();
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyClipPath()));

  Vector<scoped_refptr<BasicShape>> animated_shapes;

  for (const auto& frame : *frames) {
    animated_shapes.push_back(
        GetAnimatedShapeFromKeyframe(frame, model, element));
  }

  scoped_refptr<TimingFunction> effect_timing =
      effect->SpecifiedTiming().timing_function;

  double min_total_progress = 0.0;
  double max_total_progress = 1.0;
  effect_timing->Range(&min_total_progress, &max_total_progress);

  gfx::RectF clip_area;

  for (unsigned i = 0; i < frames->size(); i++) {
    scoped_refptr<BasicShape> cur_shape = animated_shapes[i];
    scoped_refptr<ShapeClipPathOperation> scpo =
        ShapeClipPathOperation::Create(cur_shape);
    Path path = scpo->GetPath(reference_box, zoom);
    clip_area.Union(path.BoundingRect());

    if (i + 1 == frames->size())
      break;

    double min_progress =
        i == 0 ? GetLocalProgress(min_total_progress, frames->at(0)->Offset(),
                                  frames->at(1)->Offset())
               : 0.0;
    double max_progress =
        (i + 2) == frames->size()
            ? GetLocalProgress(max_total_progress, frames->at(i)->Offset(),
                               frames->at(i + 1)->Offset())
            : 1.0;

    TimingFunction& timing = frames->at(i)->Easing();
    timing.Range(&min_progress, &max_progress);

    // If the timing function results in values outside [0,1], this will result
    // in extrapolated values that could potentially be larger than either
    // keyframe in the pair. Do the extrapolation ourselves for the maximal
    // value to find the clip area for this keyframe pair.

    if (min_progress < 0) {
      scoped_refptr<BasicShape> next_shape = animated_shapes[i + 1];
      scoped_refptr<ShapeClipPathOperation> extrapolated_shape =
          InterpolateShapes(CreateInterpolationValue(*cur_shape.get(), zoom),
                            cur_shape->GetType(),
                            CreateInterpolationValue(*next_shape.get(), zoom),
                            next_shape->GetType(), min_progress);
      path = extrapolated_shape->GetPath(reference_box, zoom);
      clip_area.Union(path.BoundingRect());
    }
    if (max_progress > 1) {
      scoped_refptr<BasicShape> next_shape = animated_shapes[i + 1];
      scoped_refptr<ShapeClipPathOperation> extrapolated_shape =
          InterpolateShapes(CreateInterpolationValue(*cur_shape.get(), zoom),
                            cur_shape->GetType(),
                            CreateInterpolationValue(*next_shape.get(), zoom),
                            next_shape->GetType(), max_progress);
      path = extrapolated_shape->GetPath(reference_box, zoom);
      clip_area.Union(path.BoundingRect());
    }
  }

  return clip_area;
}

void ClipPathPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink

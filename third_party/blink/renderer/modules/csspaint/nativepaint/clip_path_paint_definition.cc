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
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_revert_layer_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

// This struct contains the keyframe index and the intra-keyframe progress. It
// is calculated by GetAdjustedProgress.
struct AnimationProgress {
  int idx;
  float adjusted_progress;
  AnimationProgress(int idx, float adjusted_progress)
      : idx(idx), adjusted_progress(adjusted_progress) {}
  bool operator==(const AnimationProgress& other) const {
    return idx == other.idx && adjusted_progress == other.adjusted_progress;
  }
};

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
      const std::optional<double>& progress,
      const SkPath static_shape,
      cc::PaintWorkletInput::PropertyKeys property_keys)
      : PaintWorkletInput(clip_area_size, worklet_id, std::move(property_keys)),
        offsets_(offsets),
        timing_functions_(std::move(timing_functions)),
        progress_(progress),
        static_shape_(static_shape) {
    std::optional<BasicShape::ShapeType> prev_type = std::nullopt;
    for (const auto& basic_shape : animated_shapes) {
      Path path;
      basic_shape.get()->GetPath(path, reference_box, zoom);
      paths_.push_back(path.GetSkPath());
      if (prev_type.has_value()) {
        shape_compatibilities_.push_back(basic_shape->GetType() == *prev_type);
      }
      prev_type = basic_shape->GetType();
    }
  }

  ~ClipPathPaintWorkletInput() override = default;

  const std::optional<double>& MainThreadProgress() const { return progress_; }
  const Vector<SkPath>& Paths() const { return paths_; }
  const SkPath StaticPath() const { return static_shape_; }

  // Returns TRUE if the BasicShape::ShapeType of the keyframe and its following
  // keyframe are equal, FALSE otherwise. Not defined for the last keyframe.
  bool CanAttemptInterpolation(int keyframe) const {
    return shape_compatibilities_[keyframe];
  }

  PaintWorkletInputType GetType() const override {
    return PaintWorkletInputType::kClipPath;
  }

  AnimationProgress GetAdjustedProgress(float progress) const {
    // TODO(crbug.com/1374390): This function should be shared with composited
    // bgcolor animations Get the start and end clip-path based on the progress
    // and offsets.
    unsigned result_index = offsets_.size() - 1;
    if (progress <= 0) {
      result_index = 0;
    } else if (progress > 0 && progress < 1) {
      for (unsigned i = 0; i < offsets_.size() - 1; i++) {
        if (progress <= offsets_[i + 1]) {
          result_index = i;
          break;
        }
      }
    }
    if (result_index == offsets_.size() - 1) {
      result_index = offsets_.size() - 2;
    }

    // Use offsets to calculate for intra-keyframe progress.
    float local_progress =
        (progress - offsets_[result_index]) /
        (offsets_[result_index + 1] - offsets_[result_index]);
    // Adjust for that keyframe's timing function
    // TODO(crbug.com/347958668): Correct limit direction for phase and
    // direction in order to make the correct evaluation at the boundary of a
    // step-timing function.
    return AnimationProgress(
        result_index,
        timing_functions_[result_index]->GetValue(
            local_progress, TimingFunction::LimitDirection::RIGHT));
  }

  bool ValueChangeShouldCauseRepaint(const PropertyValue& val1,
                                     const PropertyValue& val2) const override {
    return !val1.float_value.has_value() || !val2.float_value.has_value() ||
           GetAdjustedProgress(*val1.float_value) !=
               GetAdjustedProgress(*val2.float_value);
  }

 private:
  Vector<SkPath> paths_;
  // Many shape types produce interpolable SkPaths, e.g. inset and a 4 point
  // polygon are both 4 point paths. By spec, we only interpolate if the the
  // BasicShape::ShapeType of each keyframe pair are equal. This tracks whether
  // the input ShapeTypes were equal. If equal, we should attempt to interpolate
  // between the resulting shapes.
  Vector<bool> shape_compatibilities_;
  Vector<double> offsets_;
  // TODO(crbug.com/1374390): Refactor composited animations so that
  // custom timing functions work for bgcolor animations as well
  // animations. This class should be refactored so that the necessary
  // properties exist in both this and Background Color paint worklet input
  Vector<std::unique_ptr<gfx::TimingFunction>> timing_functions_;
  std::optional<double> progress_;
  SkPath static_shape_;
};

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

bool CanExtractShapeOrPath(const CSSValue* computed_value) {
  // TODO(pdr): Support <geometry-box> (alone, or with a shape).
  if (const auto* list = DynamicTo<CSSValueList>(computed_value)) {
    return list->First().IsBasicShapeValue() || list->First().IsPathValue();
  }
  return false;
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

    // TODO(pdr): Support <geometry-box> (alone, or with a shape).
    if (CanExtractShapeOrPath(computed_value)) {
      basic_shape = BasicShapeForValue(
          state, DynamicTo<CSSValueList>(computed_value)->First());
    }
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
        type, *keyframe->GetValue()->Value().interpolable_value.Get(),
        *non_interpolable_value);
  }
  DCHECK(basic_shape);
  return basic_shape;
}

bool ValidateClipPathValue(const Element* element,
                           const CSSValue* value,
                           const InterpolableValue* interpolable_value) {
  if (value) {
    const CSSPropertyName property_name =
        CSSPropertyName(CSSPropertyID::kClipPath);
    const CSSValue* computed_value = StyleResolver::ComputeValue(
        const_cast<Element*>(element), property_name, *value);

    // Don't try to composite animations where we can't extract a shape or path
    if (computed_value && CanExtractShapeOrPath(computed_value)) {
      return true;
    }

    return false;
  } else if (interpolable_value) {
    // There is no need to check for clip-path: none here, as transitions are
    // not defined for this non-interpolable value. See
    // CSSAnimations::CalculateTransitionUpdateForPropertyHandle and
    // basic_shape_interpolation_functions::MaybeConvertBasicShape
    return true;
  }
  return false;
}

SkPath InterpolatePaths(const bool shapes_are_compatible,
                        const SkPath& from,
                        const SkPath& to,
                        const float progress) {
  if (shapes_are_compatible && to.isInterpolatable(from)) {
    SkPath out;
    to.interpolate(from, progress, &out);
    return out;
  } else if (progress < 0.5) {
    return from;
  } else {
    return to;
  }
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
// static
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

  const Vector<SkPath>& paths = input->Paths();

  SkPath cur_path = input->StaticPath();

  if (input->MainThreadProgress().has_value() ||
      !animated_property_values.empty()) {
    float progress = 0;

    if (!animated_property_values.empty()) {
      DCHECK_EQ(animated_property_values.size(), 1u);
      const auto& entry = animated_property_values.begin();
      progress = entry->second.float_value.value();
    } else {
      progress = input->MainThreadProgress().value();
    }

    auto [result_index, adjusted_progress] =
        input->GetAdjustedProgress(progress);
    cur_path = InterpolatePaths(input->CanAttemptInterpolation(result_index),
                                paths[result_index], paths[result_index + 1],
                                adjusted_progress);
  }

  cc::InspectablePaintRecorder paint_recorder;
  const gfx::Size clip_area_size(gfx::ToRoundedSize(input->ContainerSize()));
  cc::PaintCanvas* canvas = paint_recorder.beginRecording(clip_area_size);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  canvas->drawPath(cur_path, flags);

  return paint_recorder.finishRecordingAsPicture();
}

// Creates a deferred image of size clip_area_size that will be painted via
// paint worklet. The clip paths will be scaled and translated according to
// reference_box.
// static
scoped_refptr<Image> ClipPathPaintDefinition::Paint(
    float zoom,
    const gfx::RectF& reference_box,
    const gfx::SizeF& clip_area_size,
    const Node& node,
    int worklet_id) {
  DCHECK(node.IsElementNode());
  const Element* element = To<Element>(&node);

  Vector<scoped_refptr<BasicShape>> animated_shapes;
  Vector<double> offsets;
  std::optional<double> progress;

  Animation* animation = GetAnimationIfCompositable(element);
  // If we are here the animation must be compositable.
  CHECK(animation);

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

  SkPath static_path;

  switch (effect->SpecifiedTiming().fill_mode) {
    case Timing::FillMode::AUTO:
    case Timing::FillMode::NONE:
    case Timing::FillMode::FORWARDS: {
      ClipPathOperation* static_shape =
          element->GetLayoutObject()->StyleRef().ClipPath();
      DCHECK_EQ(static_shape->GetType(), ClipPathOperation::kShape);
      Path path = To<ShapeClipPathOperation>(static_shape)
                      ->GetPath(reference_box, zoom);
      static_path = path.GetSkPath();
      break;
    }
    case Timing::FillMode::BOTH:
    case Timing::FillMode::BACKWARDS: {
      Path path;
      animated_shapes[0].get()->GetPath(path, reference_box, zoom);
      static_path = path.GetSkPath();
    }
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
          reference_box, clip_area_size, worklet_id, zoom, animated_shapes,
          offsets, std::move(timing_functions), progress, static_path,
          std::move(input_property_keys));

  return PaintWorkletDeferredImage::Create(std::move(input), clip_area_size);
}

// static
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
    Path path;
    // TODO(pdr): Handle geometry box.
    cur_shape->GetPath(path, reference_box, zoom);
    clip_area.Union(path.BoundingRect());

    if (i + 1 == frames->size()) {
      break;
    }

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
      Path toPath;
      next_shape->GetPath(toPath, reference_box, zoom);
      SkPath interpolated =
          InterpolatePaths(cur_shape->GetType() == next_shape->GetType(),
                           path.GetSkPath(), toPath.GetSkPath(), min_progress);
      clip_area.Union(gfx::SkRectToRectF(interpolated.getBounds()));
    }
    if (max_progress > 1) {
      scoped_refptr<BasicShape> next_shape = animated_shapes[i + 1];
      Path toPath;
      next_shape->GetPath(toPath, reference_box, zoom);
      SkPath interpolated =
          InterpolatePaths(cur_shape->GetType() == next_shape->GetType(),
                           path.GetSkPath(), toPath.GetSkPath(), max_progress);
      clip_area.Union(gfx::SkRectToRectF(interpolated.getBounds()));
    }
  }

  return clip_area;
}

void ClipPathPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink

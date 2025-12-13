// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_definition.h"

#include "cc/paint/paint_recorder.h"
#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css_default_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_shape_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/path_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value_mappings.h"
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
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/geometry_box_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
      const gfx::RectF& image_area,
      int worklet_id,
      float zoom,
      Vector<SkPath> paths,
      Vector<bool> shape_compatibilities,
      Vector<double> offsets,
      Vector<std::unique_ptr<gfx::TimingFunction>> timing_functions,
      const std::optional<double>& progress,
      const SkPath static_shape,
      cc::PaintWorkletInput::PropertyKeys property_keys)
      : PaintWorkletInput(image_area.size(),
                          worklet_id,
                          std::move(property_keys)),
        paths_(std::move(paths)),
        shape_compatibilities_(std::move(shape_compatibilities)),
        offsets_(std::move(offsets)),
        timing_functions_(std::move(timing_functions)),
        progress_(progress),
        static_shape_(static_shape),
        dx_(-image_area.x()),
        dy_(-image_area.y()) {}

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

  // TODO(clchambers): This is essentially the inverse translation that is
  // applied by the serialization of the paint worklet deferred image. Rather
  // than applying two equal but opposite translations, we could instead modify
  // PaintOpBufferSerializer::WillSerializeNextOp to simply remove the
  // translation, so that we paint directly in content space, similarly to main
  // thread clip paths.
  void ApplyTranslation(cc::PaintCanvas* canvas) const {
    canvas->translate(dx_, dy_);
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

  SkScalar dx_, dy_;
};

BasicShape* CreateBasicShape(
    BasicShape::ShapeType type,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue& untyped_non_interpolable_value) {
  if (type == BasicShape::kStylePathType) {
    return PathInterpolationFunctions::AppliedValue(
        interpolable_value, &untyped_non_interpolable_value);
  }

  CSSToLengthConversionData conversion_data(/*element=*/nullptr);
  if (type == BasicShape::kStyleShapeType) {
    return CSSShapeInterpolationType::CreateShape(
        interpolable_value, &untyped_non_interpolable_value, conversion_data);
  }

  return basic_shape_interpolation_functions::CreateBasicShape(
      interpolable_value, untyped_non_interpolable_value, conversion_data);
}

bool CanExtractShapeOrPath(const CSSValue* computed_value) {
  // TODO(pdr): Support <geometry-box> (alone, or with a shape).
  if (const auto* list = DynamicTo<CSSValueList>(computed_value)) {
    return list->First().IsBasicShapeValue() || list->First().IsPathValue() ||
           (list->First().IsShapeValue());
  }
  return false;
}

bool IsClipPathNone(const CSSValue* computed_value) {
  if (computed_value->IsIdentifierValue()) {
    const CSSIdentifierValue* id_val = To<CSSIdentifierValue>(computed_value);
    switch (id_val->GetValueID()) {
      case CSSValueID::kNone:
      case CSSValueID::kInitial:
      case CSSValueID::kUnset:
        return true;
      default:
        return false;
    }
  }
  return false;
}

BasicShape* GetAnimatedShapeFromCSSValue(const CSSValue* computed_value,
                                         const Element* element) {
  StyleResolverState state(element->GetDocument(),
                           *const_cast<Element*>(element));

  // TODO(pdr): Support <geometry-box> (alone, or with a shape).
  if (CanExtractShapeOrPath(computed_value)) {
    return BasicShapeForValue(state,
                              DynamicTo<CSSValueList>(computed_value)->First());
  } else {
    DCHECK(IsClipPathNone(computed_value));
    return nullptr;
  }
}

// Returns the basic shape of a keyframe, or null if the keyframe has no path
BasicShape* GetAnimatedShapeFromKeyframe(const PropertySpecificKeyframe* frame,
                                         const KeyframeEffectModelBase* model,
                                         const Element* element) {
  if (model->IsStringKeyframeEffectModel()) {
    DCHECK(frame->IsCSSPropertySpecificKeyframe());
    const CSSValue* value =
        static_cast<const CSSPropertySpecificKeyframe*>(frame)->Value();
    const CSSPropertyName property_name =
        CSSPropertyName(CSSPropertyID::kClipPath);
    const CSSValue* computed_value = StyleResolver::ComputeValue(
        const_cast<Element*>(element), property_name, *value);

    return GetAnimatedShapeFromCSSValue(computed_value, element);
  } else {
    DCHECK(frame->IsTransitionPropertySpecificKeyframe());
    const TransitionKeyframe::PropertySpecificKeyframe* keyframe =
        To<TransitionKeyframe::PropertySpecificKeyframe>(frame);
    const NonInterpolableValue* non_interpolable_value =
        keyframe->GetValue()->Value().non_interpolable_value.Get();

    if (IsA<CSSDefaultNonInterpolableValue>(non_interpolable_value)) {
      return GetAnimatedShapeFromCSSValue(
          To<CSSDefaultNonInterpolableValue>(non_interpolable_value)
              ->CssValue(),
          element);
    } else {
      BasicShape::ShapeType type =
          PathInterpolationFunctions::IsPathNonInterpolableValue(
              *non_interpolable_value)
              ? BasicShape::kStylePathType
              // This can be any shape but kStylePathType. This is needed to
              // distinguish between Path shape and other shapes in
              // CreateBasicShape function.
              : BasicShape::kBasicShapeCircleType;
      return CreateBasicShape(
          type, *keyframe->GetValue()->Value().interpolable_value.Get(),
          *non_interpolable_value);
    }
  }
}

std::optional<SkPath> GetFillRequiredByEffect(const AnimationEffect* effect,
                                              const LayoutObject& obj,
                                              const gfx::RectF& reference_box,
                                              const gfx::Vector2dF& clip_offset,
                                              const float zoom,
                                              SkPath first_keyframe) {
  switch (effect->SpecifiedTiming().fill_mode) {
    case Timing::FillMode::AUTO:
    case Timing::FillMode::NONE:
    case Timing::FillMode::FORWARDS: {
      if (obj.StyleRef().HasClipPath()) {
        ClipPathOperation* static_op = obj.StyleRef().ClipPath();
        Path path;
        switch (static_op->GetType()) {
          case ClipPathOperation::kShape:
            path = To<ShapeClipPathOperation>(static_op)->GetPath(
                reference_box, zoom, /*path_scale=*/1.f);
            if (!clip_offset.IsZero()) {
              path = PathBuilder(path).Translate(clip_offset).Finalize();
            }
            break;
          case ClipPathOperation::kGeometryBox: {
            ContouredRect box = ClipPathClipper::RoundedReferenceBox(
                To<GeometryBoxClipPathOperation>(static_op)->GetGeometryBox(),
                obj);
            if (!clip_offset.IsZero()) {
              box.Move(clip_offset);
            }
            path = box.GetPath();
            break;
          }
          case ClipPathOperation::kReference:
            // Reference clip paths are implemented with mask images, and are
            // not reducible to single SkPaths.
            NOTREACHED();
        }
        return path.GetSkPath();
      } else {
        // Caller decides what to do for clip-path: none.
        return std::nullopt;
      }
    }
    case Timing::FillMode::BOTH:
    case Timing::FillMode::BACKWARDS: {
      return first_keyframe;
    }
  }
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
      const auto* list = DynamicTo<CSSValueList>(computed_value);

      // TODO(crbug.com/379052285): We do not currently support anything other
      // than kBorderBox. Any other value will result in bad interpolation. This
      // should be resolved in future.
      if (list->length() == 2 &&
          (DynamicTo<CSSIdentifierValue>(list->Item(1))
               ->ConvertTo<GeometryBox>() != GeometryBox::kBorderBox)) {
        return false;
      }

      return true;
    }

    // clip-path: none is a special case where we decline to clip a path.
    if (IsClipPathNone(value)) {
      return true;
    }

    return false;
  } else if (interpolable_value) {
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
  if (!element->GetElementAnimations()) {
    return nullptr;
  }

  Animation* compositable_animation =
      element->GetElementAnimations()->PaintWorkletClipPathAnimation();

  if (!compositable_animation) {
    return nullptr;
  }

  DCHECK(compositable_animation->Affects(*element, GetCSSPropertyClipPath()));

  if (element->GetElementAnimations()->CompositedClipPathStatus() ==
      ElementAnimations::CompositedPaintStatus::kComposited) {
    DCHECK(AnimationIsValidForPaintWorklets(compositable_animation, element,
                                            GetCSSPropertyClipPath(),
                                            ValidateClipPathValue));
    return compositable_animation;
  }

  return AnimationIsValidForPaintWorklets(compositable_animation, element,
                                          GetCSSPropertyClipPath(),
                                          ValidateClipPathValue)
             ? compositable_animation
             : nullptr;
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
  const gfx::Size clip_area_size(
      gfx::ToRoundedSize(gfx::RectF(InfiniteIntRect()).size()));
  cc::PaintCanvas* canvas = paint_recorder.beginRecording(clip_area_size);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  input->ApplyTranslation(canvas);

  // TODO(crbug.com/451650621): Painting a full Skia path every time is
  // expensive. Main-thread clip-path animations use RRects when possible, and
  // this behavior should be replicated here. See:
  // SynthesizedClip::PaintContentsToDisplayList.
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
    const gfx::RectF& clip_area_rect,
    const Node& node,
    int worklet_id) {
  DCHECK(node.IsElementNode());
  const Element* element = To<Element>(&node);
  gfx::Vector2dF clip_offset =
      gfx::Vector2dF(node.GetLayoutObject()->FirstFragment().PaintOffset());

  Vector<SkPath> paths;
  Vector<bool> shape_compatibilities;
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

  // TODO(crbug.com/459701868): The following code essentially re-implments
  // ClipPathClipper::PathBasedClipInternal as well as
  // CSSBasicShapeInterpolationType. There's no good reason cc clip paths need a
  // completely divergent implementation, all we really need is to extract shape
  // compatibility as well as handle the case where clip path is none. This
  // class should be refactored to use the main thread machinery directly.
  std::optional<BasicShape::ShapeType> prev_type = std::nullopt;
  for (const auto& frame : *frames) {
    BasicShape* basic_shape =
        GetAnimatedShapeFromKeyframe(frame, model, element);

    // No compatibility for the first shape.
    if (!paths.empty()) {
      shape_compatibilities.push_back(
          prev_type && basic_shape ? (basic_shape->GetType() == *prev_type)
                                   : false);
    }

    if (basic_shape) {
      Path path = basic_shape->GetPath(reference_box, zoom, /*path_scale=*/1.f);
      if (!clip_offset.IsZero()) {
        path = PathBuilder(path).Translate(clip_offset).Finalize();
      }
      paths.push_back(path.GetSkPath());
      prev_type = basic_shape->GetType();
    } else {
      paths.push_back(SkPath::Rect(gfx::RectFToSkRect(clip_area_rect)));
      prev_type = std::nullopt;
    }

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
  SkPath static_path =
      GetFillRequiredByEffect(effect, *element->GetLayoutObject(),
                              reference_box, clip_offset, zoom, paths[0])
          .value_or(SkPath::Rect(gfx::RectFToSkRect(clip_area_rect)));

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
          clip_area_rect, worklet_id, zoom, std::move(paths),
          std::move(shape_compatibilities), std::move(offsets),
          std::move(timing_functions), progress, static_path,
          std::move(input_property_keys));

  return PaintWorkletDeferredImage::Create(std::move(input),
                                           clip_area_rect.size());
}

// Helper functions for GetAnimationBoundingRect
namespace {

// Returns a definite containing rectangle for all keyframes and fills for
// this animation, or none, if clip-path: none is encountered. For the
// typical case, this is simply the enclosing rect of the union of all
// keyframes. For animations with timing functions outside [0,1], extra
// work is done to account for keyframe extrapolation.
std::optional<gfx::RectF> ComputeKeyframeUnionIncludingExtrapolation(
    const LayoutObject& obj,
    const Element* element,
    const KeyframeEffect* effect) {
  const KeyframeEffectModelBase* model = effect->Model();
  const PropertySpecificKeyframeVector* frames =
      model->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyClipPath()));

  HeapVector<Member<BasicShape>> animated_shapes;
  gfx::RectF clip_area;

  for (const auto& frame : *frames) {
    BasicShape* shape = GetAnimatedShapeFromKeyframe(frame, model, element);
    if (!shape) {
      // clip-path: none
      return std::nullopt;
    }
    animated_shapes.push_back(shape);
  }

  scoped_refptr<TimingFunction> effect_timing =
      effect->SpecifiedTiming().timing_function;

  // TODO(crbug.com/379052285): these assumptions are currently valid
  // because of value filters. Eventually, these should be removed when
  // proper geometry-box support is added.
  gfx::RectF reference_box = ClipPathClipper::CalcLocalReferenceBox(
      obj, ClipPathOperation::OperationType::kShape, GeometryBox::kBorderBox);
  const float zoom = ClipPathClipper::UsesZoomedReferenceBox(obj)
                         ? 1
                         : obj.StyleRef().EffectiveZoom();

  if (effect->SpecifiedTiming().start_delay.time_delay > AnimationTimeDelta()) {
    std::optional<SkPath> fill = GetFillRequiredByEffect(
        effect, obj, reference_box, gfx::Vector2dF(0.f, 0.f), zoom, SkPath());
    if (!fill.has_value()) {
      // clip-path: none
      return std::nullopt;
    }

    if (!fill->isEmpty()) {
      clip_area.Union(gfx::SkRectToRectF(fill->getBounds()));
    }
  }

  double min_total_progress = 0.0;
  double max_total_progress = 1.0;
  effect_timing->Range(&min_total_progress, &max_total_progress);

  for (unsigned i = 0; i < frames->size(); i++) {
    BasicShape* cur_shape = animated_shapes[i];
    CHECK(cur_shape);

    const Path path = cur_shape->GetPath(reference_box, zoom, 1.f);
    clip_area.Union(path.BoundingRect());

    if (i + 1 == frames->size()) {
      break;
    }

    double min_progress =
        i == 0 ? ((min_total_progress - frames->at(0)->Offset()) /
                  (frames->at(1)->Offset() - frames->at(0)->Offset()))
               : 0.0;
    double max_progress =
        (i + 2) == frames->size()
            ? ((max_total_progress - frames->at(i)->Offset()) /
               (frames->at(i + 1)->Offset() - frames->at(i)->Offset()))
            : 1.0;

    TimingFunction& timing = frames->at(i)->Easing();
    timing.Range(&min_progress, &max_progress);

    // If the timing function results in values outside [0,1], this
    // will result in extrapolated values that could potentially be
    // larger than either keyframe in the pair. Do the extrapolation
    // ourselves for the maximal value to find the clip area for
    // this keyframe pair.

    if (min_progress < 0) {
      BasicShape* next_shape = animated_shapes[i + 1];
      Path toPath = next_shape->GetPath(reference_box, zoom, 1.f);
      SkPath interpolated =
          InterpolatePaths(cur_shape->GetType() == next_shape->GetType(),
                           path.GetSkPath(), toPath.GetSkPath(), min_progress);
      clip_area.Union(gfx::SkRectToRectF(interpolated.getBounds()));
    }
    if (max_progress > 1) {
      BasicShape* next_shape = animated_shapes[i + 1];
      Path toPath = next_shape->GetPath(reference_box, zoom, 1.f);
      SkPath interpolated =
          InterpolatePaths(cur_shape->GetType() == next_shape->GetType(),
                           path.GetSkPath(), toPath.GetSkPath(), max_progress);
      clip_area.Union(gfx::SkRectToRectF(interpolated.getBounds()));
    }
  }

  return clip_area;
}

}  // namespace

// static
std::optional<gfx::RectF> ClipPathPaintDefinition::GetAnimationBoundingRect(
    const LayoutObject& obj) {
  const Element* element = To<Element>(obj.GetNode());

  CHECK(element);

  const Animation* animation = GetAnimationIfCompositable(element);
  CHECK(animation);

  const AnimationEffect* effect = animation->effect();
  CHECK(effect);
  CHECK(effect->IsKeyframeEffect());

  const std::optional<gfx::RectF> keyframe_union =
      ComputeKeyframeUnionIncludingExtrapolation(obj, element,
                                                 To<KeyframeEffect>(effect));
  if (keyframe_union.has_value()) {
    return *keyframe_union;
  }

  // The interaction between clip-path animations with clip-path: none and
  // descendant transform animations requires a fallback, because right now
  // there is no way to estimate the maximum visible area
  // TODO(clchambers): Once compositor and main-thread clip-path implementations
  // are merged, it may be possible to remove this case by either inverting the
  // blend mode (kXor?) or using edge mode for this case on cc/viz side.
  // Alternatively, since cc knows the definite state of any cc-animated
  // transforms, it's possible that the required mask size could be computed
  // directly at impl-side paint time, making the size of the painted mask image
  // variable (which would potentially involve (re)allocating new tiles).
  if (obj.PaintingLayer()->HasDescendantWithTransformAnim() ||
      obj.StyleRef().HasCurrentTransformRelatedAnimation()) {
    return std::nullopt;
  }

  // Return an infinite rect. This won't actually be used as the mask image
  // size. Instead, it is the responsibility of ClipPathClipper during
  // paint-time to use the current cull rect as the image size.
  return gfx::RectF(InfiniteIntRect());
}

void ClipPathPaintDefinition::Trace(Visitor* visitor) const {
  NativePaintDefinition::Trace(visitor);
}

}  // namespace blink

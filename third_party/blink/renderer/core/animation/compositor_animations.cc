/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/compositor_animations.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/animatable/animatable_double.h"
#include "third_party/blink/renderer/core/animation/animatable/animatable_filter_operations.h"
#include "third_party/blink/renderer/core/animation/animatable/animatable_transform.h"
#include "third_party/blink/renderer/core/animation/animatable/animatable_value.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/animation/animation_translation_util.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_filter_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_filter_keyframe.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_keyframe.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/animation/compositor_transform_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_transform_keyframe.h"
#include "third_party/blink/renderer/platform/geometry/float_box.h"

namespace blink {

namespace {

bool ConsiderAnimationAsIncompatible(const Animation& animation,
                                     const Animation& animation_to_add,
                                     const EffectModel& effect_to_add) {
  if (&animation == &animation_to_add)
    return false;

  switch (animation.PlayStateInternal()) {
    case Animation::kIdle:
      return false;
    case Animation::kPending:
    case Animation::kRunning:
      return true;
    case Animation::kPaused:
    case Animation::kFinished:
      if (Animation::HasLowerPriority(&animation, &animation_to_add)) {
        return effect_to_add.AffectedByUnderlyingAnimations();
      }
      return true;
    default:
      NOTREACHED();
      return true;
  }
}

bool IsTransformRelatedCSSProperty(const PropertyHandle property) {
  return property.IsCSSProperty() &&
         (property.GetCSSProperty().IDEquals(CSSPropertyRotate) ||
          property.GetCSSProperty().IDEquals(CSSPropertyScale) ||
          property.GetCSSProperty().IDEquals(CSSPropertyTransform) ||
          property.GetCSSProperty().IDEquals(CSSPropertyTranslate));
}

bool IsTransformRelatedAnimation(const Element& target_element,
                                 const Animation* animation) {
  return animation->Affects(target_element, GetCSSPropertyTransform()) ||
         animation->Affects(target_element, GetCSSPropertyRotate()) ||
         animation->Affects(target_element, GetCSSPropertyScale()) ||
         animation->Affects(target_element, GetCSSPropertyTranslate());
}

bool HasIncompatibleAnimations(const Element& target_element,
                               const Animation& animation_to_add,
                               const EffectModel& effect_to_add) {
  if (!target_element.HasAnimations())
    return false;

  ElementAnimations* element_animations = target_element.GetElementAnimations();
  DCHECK(element_animations);

  const bool affects_opacity =
      effect_to_add.Affects(PropertyHandle(GetCSSPropertyOpacity()));
  const bool affects_transform = effect_to_add.IsTransformRelatedEffect();
  const bool affects_filter =
      effect_to_add.Affects(PropertyHandle(GetCSSPropertyFilter()));
  const bool affects_backdrop_filter =
      effect_to_add.Affects(PropertyHandle(GetCSSPropertyBackdropFilter()));

  for (const auto& entry : element_animations->Animations()) {
    const Animation* attached_animation = entry.key;
    if (!ConsiderAnimationAsIncompatible(*attached_animation, animation_to_add,
                                         effect_to_add)) {
      continue;
    }

    if ((affects_opacity && attached_animation->Affects(
                                target_element, GetCSSPropertyOpacity())) ||
        (affects_transform &&
         IsTransformRelatedAnimation(target_element, attached_animation)) ||
        (affects_filter &&
         attached_animation->Affects(target_element, GetCSSPropertyFilter())) ||
        (affects_backdrop_filter &&
         attached_animation->Affects(target_element,
                                     GetCSSPropertyBackdropFilter()))) {
      return true;
    }
  }

  return false;
}

}  // namespace

CompositorAnimations::FailureCode
CompositorAnimations::CheckCanStartEffectOnCompositor(
    const Timing& timing,
    const Element& target_element,
    const Animation* animation_to_add,
    const EffectModel& effect,
    const base::Optional<CompositorElementIdSet>& composited_element_ids,
    double animation_playback_rate) {
  const KeyframeEffectModelBase& keyframe_effect =
      ToKeyframeEffectModelBase(effect);

  PropertyHandleSet properties = keyframe_effect.Properties();
  if (properties.IsEmpty()) {
    return FailureCode::Actionable("Animation does not affect any properties");
  }

  if (composited_element_ids) {
    // If we are going to check that we can animate these below, we need
    // to have the UniqueID to compute the target ID.  Let's check it
    // once in common in advance.
    if (!target_element.GetLayoutObject() ||
        !target_element.GetLayoutObject()->UniqueId()) {
      return FailureCode::NonActionable("Target element has no layout");
    }
  }

  unsigned transform_property_count = 0;
  for (const auto& property : properties) {
    if (!property.IsCSSProperty()) {
      return FailureCode::Actionable("Animation affects non-CSS properties");
    }

    if (IsTransformRelatedCSSProperty(property)) {
      // We use this later in computing element IDs too.
      if (target_element.GetLayoutObject() &&
          !target_element.GetLayoutObject()->IsTransformApplicable()) {
        return FailureCode::Actionable(
            "Transform-related property cannot be accelerated on target "
            "element");
      }
      transform_property_count++;
    }

    const PropertySpecificKeyframeVector& keyframes =
        keyframe_effect.GetPropertySpecificKeyframes(property);
    DCHECK_GE(keyframes.size(), 2U);
    for (const auto& keyframe : keyframes) {
      if (keyframe->Composite() != EffectModel::kCompositeReplace &&
          !keyframe->IsNeutral()) {
        return FailureCode::Actionable(
            "Accelerated animations don't support keyframes with composite "
            "modes other than 'replace'");
      }

      if (!keyframe->GetAnimatableValue()) {
        return FailureCode::NonActionable(
            "Accelerated keyframe value could not be computed");
      }

      CompositorElementIdNamespace property_namespace =
          CompositorElementIdNamespace::kPrimary;
      // FIXME: Determine candidacy based on the CSSValue instead of a snapshot
      // AnimatableValue.
      switch (property.GetCSSProperty().PropertyID()) {
        case CSSPropertyOpacity:
          break;
        case CSSPropertyRotate:
        case CSSPropertyScale:
        case CSSPropertyTranslate:
        case CSSPropertyTransform:
          if (ToAnimatableTransform(keyframe->GetAnimatableValue())
                  ->GetTransformOperations()
                  .DependsOnBoxSize()) {
            return FailureCode::Actionable(
                "Transform-related property value depends on layout box "
                "size");
          }
          break;
        case CSSPropertyFilter:
        case CSSPropertyBackdropFilter: {
          const FilterOperations& operations =
              ToAnimatableFilterOperations(keyframe->GetAnimatableValue())
                  ->Operations();
          if (operations.HasFilterThatMovesPixels()) {
            return FailureCode::Actionable(
                "Filter-related property may affect surrounding pixels");
          }
          property_namespace = CompositorElementIdNamespace::kEffectFilter;
          break;
        }
        case CSSPropertyVariable: {
          DCHECK(RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled());
          if (!keyframe->GetAnimatableValue()->IsDouble()) {
            // TODO(kevers): Extend support to other custom property types.
            return FailureCode::Actionable(
                "Accelerated animation cannot be applied to a custom property "
                "that is not numeric-valued");
          }
          break;
        }
        default:
          // any other types are not allowed to run on compositor.
          StringBuilder builder;
          builder.Append("CSS property not supported: ");
          if (property.IsCSSCustomProperty()) {
            builder.Append(property.CustomPropertyName());
          } else {
            builder.Append(property.GetCSSProperty().GetPropertyName());
          }
          return FailureCode::Actionable(builder.ToString());
      }
      if (composited_element_ids) {
        CompositorElementId target_element_id =
            CompositorElementIdFromUniqueObjectId(
                target_element.GetLayoutObject()->UniqueId(),
                property_namespace);
        DCHECK(target_element_id);
        if (!composited_element_ids->count(target_element_id)) {
          return FailureCode::NonActionable(
              "Target element does not have its own compositing layer");
        }
      }
    }
  }

  // TODO: Support multiple transform property animations on the compositor
  if (transform_property_count > 1) {
    return FailureCode::Actionable(
        "Accelerated animations do not support multiple transform-related "
        "properties in a single animation");
  }

  if (animation_to_add &&
      HasIncompatibleAnimations(target_element, *animation_to_add, effect)) {
    return FailureCode::Actionable(
        "Animation not compatible for acceleration with other animations on "
        "the target element");
  }

  CompositorTiming out;
  if (!ConvertTimingForCompositor(timing, 0, out, animation_playback_rate)) {
    return FailureCode::NonActionable(
        "The specified timing parameters are not supported by accelerated "
        "animations");
  }

  return FailureCode::None();
}

CompositorAnimations::FailureCode
CompositorAnimations::CheckCanStartElementOnCompositor(
    const Element& target_element) {
  if (!Platform::Current()->IsThreadedAnimationEnabled()) {
    return FailureCode::NonActionable("Accelerated animations are disabled");
  }

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // We query paint property tree state below to determine whether the
    // animation is compositable. There is a known lifecycle violation where an
    // animation can be cancelled during style update. See
    // CompositorAnimations::cancelAnimationOnCompositor and
    // http://crbug.com/676456. When this is fixed we would like to enable
    // the DCHECK below.
    // DCHECK(document().lifecycle().state() >=
    // DocumentLifecycle::PrePaintClean);
    DCHECK(target_element.GetLayoutObject());
    if (const auto* paint_properties = target_element.GetLayoutObject()
                                           ->FirstFragment()
                                           .PaintProperties()) {
      const TransformPaintPropertyNode* transform_node =
          paint_properties->Transform();
      const EffectPaintPropertyNode* effect_node = paint_properties->Effect();
      bool has_direct_compositing_reasons =
          (transform_node && transform_node->HasDirectCompositingReasons()) ||
          (effect_node && effect_node->HasDirectCompositingReasons());
      if (!has_direct_compositing_reasons) {
        return FailureCode::NonActionable(
            "Element has no direct compositing reasons");
      }
    }
  } else {
    LayoutObject* layout_object = target_element.GetLayoutObject();
    bool paints_into_own_backing =
        layout_object &&
        layout_object->GetCompositingState() == kPaintsIntoOwnBacking;
    if (!paints_into_own_backing) {
      return FailureCode::NonActionable(
          "Element does not paint into own backing");
    }
  }

  return FailureCode::None();
}

// TODO(crbug.com/809685): consider refactor this function.
CompositorAnimations::FailureCode
CompositorAnimations::CheckCanStartAnimationOnCompositor(
    const Timing& timing,
    const Element& target_element,
    const Animation* animation_to_add,
    const EffectModel& effect,
    const base::Optional<CompositorElementIdSet>& composited_element_ids,
    double animation_playback_rate) {
  FailureCode code = CheckCanStartEffectOnCompositor(
      timing, target_element, animation_to_add, effect, composited_element_ids,
      animation_playback_rate);
  if (!code.Ok()) {
    return code;
  }
  return CheckCanStartElementOnCompositor(target_element);
}

void CompositorAnimations::CancelIncompatibleAnimationsOnCompositor(
    const Element& target_element,
    const Animation& animation_to_add,
    const EffectModel& effect_to_add) {
  const bool affects_opacity =
      effect_to_add.Affects(PropertyHandle(GetCSSPropertyOpacity()));
  const bool affects_transform = effect_to_add.IsTransformRelatedEffect();
  const bool affects_filter =
      effect_to_add.Affects(PropertyHandle(GetCSSPropertyFilter()));
  const bool affects_backdrop_filter =
      effect_to_add.Affects(PropertyHandle(GetCSSPropertyBackdropFilter()));

  if (!target_element.HasAnimations())
    return;

  ElementAnimations* element_animations = target_element.GetElementAnimations();
  DCHECK(element_animations);

  for (const auto& entry : element_animations->Animations()) {
    Animation* attached_animation = entry.key;
    if (!ConsiderAnimationAsIncompatible(*attached_animation, animation_to_add,
                                         effect_to_add)) {
      continue;
    }

    if ((affects_opacity && attached_animation->Affects(
                                target_element, GetCSSPropertyOpacity())) ||
        (affects_transform &&
         IsTransformRelatedAnimation(target_element, attached_animation)) ||
        (affects_filter &&
         attached_animation->Affects(target_element, GetCSSPropertyFilter())) ||
        (affects_backdrop_filter &&
         attached_animation->Affects(target_element,
                                     GetCSSPropertyBackdropFilter()))) {
      attached_animation->CancelAnimationOnCompositor();
    }
  }
}

void CompositorAnimations::StartAnimationOnCompositor(
    const Element& element,
    int group,
    base::Optional<double> start_time,
    double time_offset,
    const Timing& timing,
    const Animation* animation,
    CompositorAnimation& compositor_animation,
    const EffectModel& effect,
    Vector<int>& started_keyframe_model_ids,
    double animation_playback_rate) {
  DCHECK(started_keyframe_model_ids.IsEmpty());
  // TODO(petermayo): Find and pass the set of valid compositor elements before
  // BlinkGenPropertyTrees is always on.
  DCHECK(CheckCanStartAnimationOnCompositor(
             timing, element, animation, effect,
             base::Optional<CompositorElementIdSet>(), animation_playback_rate)
             .Ok());

  const KeyframeEffectModelBase& keyframe_effect =
      ToKeyframeEffectModelBase(effect);

  Vector<std::unique_ptr<CompositorKeyframeModel>> keyframe_models;
  GetAnimationOnCompositor(timing, group, start_time, time_offset,
                           keyframe_effect, keyframe_models,
                           animation_playback_rate);
  DCHECK(!keyframe_models.IsEmpty());
  for (auto& compositor_keyframe_model : keyframe_models) {
    int id = compositor_keyframe_model->Id();
    compositor_animation.AddKeyframeModel(std::move(compositor_keyframe_model));
    started_keyframe_model_ids.push_back(id);
  }
  DCHECK(!started_keyframe_model_ids.IsEmpty());
}

void CompositorAnimations::CancelAnimationOnCompositor(
    const Element& element,
    CompositorAnimation* compositor_animation,
    int id) {
  if (!CheckCanStartElementOnCompositor(element).Ok()) {
    // When an element is being detached, we cancel any associated
    // Animations for CSS animations. But by the time we get
    // here the mapping will have been removed.
    // FIXME: Defer remove/pause operations until after the
    // compositing update.
    return;
  }
  if (compositor_animation)
    compositor_animation->RemoveKeyframeModel(id);
}

void CompositorAnimations::PauseAnimationForTestingOnCompositor(
    const Element& element,
    const Animation& animation,
    int id,
    double pause_time) {
  // FIXME: CheckCanStartAnimationOnCompositor queries compositingState, which
  // is not necessarily up to date.
  // https://code.google.com/p/chromium/issues/detail?id=339847
  DisableCompositingQueryAsserts disabler;

  DCHECK(CheckCanStartElementOnCompositor(element).Ok());
  CompositorAnimation* compositor_animation =
      animation.GetCompositorAnimation();
  DCHECK(compositor_animation);
  compositor_animation->PauseKeyframeModel(id, pause_time);
}

void CompositorAnimations::AttachCompositedLayers(
    Element& element,
    CompositorAnimation* compositor_animation) {
  if (!compositor_animation)
    return;

  if (!element.GetLayoutObject() ||
      !element.GetLayoutObject()->IsBoxModelObject() ||
      !element.GetLayoutObject()->HasLayer())
    return;

  PaintLayer* layer =
      ToLayoutBoxModelObject(element.GetLayoutObject())->Layer();

  // Composited animations do not depend on a composited layer mapping for SPv2.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    if (!layer->IsAllowedToQueryCompositingState() ||
        !layer->GetCompositedLayerMapping() ||
        !layer->GetCompositedLayerMapping()->MainGraphicsLayer())
      return;

    if (!layer->GetCompositedLayerMapping()->MainGraphicsLayer()->CcLayer())
      return;
  }

  compositor_animation->AttachElement(CompositorElementIdFromUniqueObjectId(
      element.GetLayoutObject()->UniqueId(),
      CompositorElementIdNamespace::kPrimary));
}

bool CompositorAnimations::ConvertTimingForCompositor(
    const Timing& timing,
    double time_offset,
    CompositorTiming& out,
    double animation_playback_rate) {
  timing.AssertValid();

  // FIXME: Compositor does not know anything about endDelay.
  if (timing.end_delay != 0)
    return false;

  if (!timing.iteration_duration || !timing.iteration_count ||
      timing.iteration_duration->is_zero())
    return false;

  out.adjusted_iteration_count =
      std::isfinite(timing.iteration_count) ? timing.iteration_count : -1;
  out.scaled_duration = timing.iteration_duration.value();
  out.direction = timing.direction;
  // Compositor's time offset is positive for seeking into the animation.
  out.scaled_time_offset =
      -timing.start_delay / animation_playback_rate + time_offset;
  out.playback_rate = animation_playback_rate;
  out.fill_mode = timing.fill_mode == Timing::FillMode::AUTO
                      ? Timing::FillMode::NONE
                      : timing.fill_mode;
  out.iteration_start = timing.iteration_start;

  DCHECK_GT(out.scaled_duration, AnimationTimeDelta());
  DCHECK(std::isfinite(out.scaled_time_offset));
  DCHECK(out.adjusted_iteration_count > 0 ||
         out.adjusted_iteration_count == -1);
  DCHECK(std::isfinite(out.playback_rate) && out.playback_rate);
  DCHECK_GE(out.iteration_start, 0);

  return true;
}

namespace {

void AddKeyframeToCurve(CompositorFilterAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const AnimatableValue* value,
                        const TimingFunction& keyframe_timing_function) {
  FilterEffectBuilder builder(FloatRect(), 1);
  CompositorFilterKeyframe filter_keyframe(
      keyframe->Offset(),
      builder.BuildFilterOperations(
          ToAnimatableFilterOperations(value)->Operations()),
      keyframe_timing_function);
  curve.AddKeyframe(filter_keyframe);
}

void AddKeyframeToCurve(CompositorFloatAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const AnimatableValue* value,
                        const TimingFunction& keyframe_timing_function) {
  CompositorFloatKeyframe float_keyframe(keyframe->Offset(),
                                         ToAnimatableDouble(value)->ToDouble(),
                                         keyframe_timing_function);
  curve.AddKeyframe(float_keyframe);
}

void AddKeyframeToCurve(CompositorTransformAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const AnimatableValue* value,
                        const TimingFunction& keyframe_timing_function) {
  CompositorTransformOperations ops;
  ToCompositorTransformOperations(
      ToAnimatableTransform(value)->GetTransformOperations(), &ops);

  CompositorTransformKeyframe transform_keyframe(
      keyframe->Offset(), std::move(ops), keyframe_timing_function);
  curve.AddKeyframe(transform_keyframe);
}

template <typename PlatformAnimationCurveType>
void AddKeyframesToCurve(PlatformAnimationCurveType& curve,
                         const PropertySpecificKeyframeVector& keyframes) {
  Keyframe::PropertySpecificKeyframe* last_keyframe = keyframes.back();
  for (const auto& keyframe : keyframes) {
    const TimingFunction* keyframe_timing_function = nullptr;
    // Ignore timing function of last frame.
    if (keyframe == last_keyframe)
      keyframe_timing_function = LinearTimingFunction::Shared();
    else
      keyframe_timing_function = &keyframe->Easing();

    const AnimatableValue* value = keyframe->GetAnimatableValue();
    AddKeyframeToCurve(curve, keyframe, value, *keyframe_timing_function);
  }
}

}  // namespace

void CompositorAnimations::GetAnimationOnCompositor(
    const Timing& timing,
    int group,
    base::Optional<double> start_time,
    double time_offset,
    const KeyframeEffectModelBase& effect,
    Vector<std::unique_ptr<CompositorKeyframeModel>>& keyframe_models,
    double animation_playback_rate) {
  DCHECK(keyframe_models.IsEmpty());
  CompositorTiming compositor_timing;
  bool timing_valid = ConvertTimingForCompositor(
      timing, time_offset, compositor_timing, animation_playback_rate);
  ALLOW_UNUSED_LOCAL(timing_valid);

  PropertyHandleSet properties = effect.Properties();
  DCHECK(!properties.IsEmpty());
  for (const auto& property : properties) {
    // If the animation duration is infinite, it doesn't make sense to scale
    // the keyframe offset, so use a scale of 1.0. This is connected to
    // the known issue of how the Web Animations spec handles infinite
    // durations. See https://github.com/w3c/web-animations/issues/142
    double scale = compositor_timing.scaled_duration.InSecondsF();
    if (!std::isfinite(scale))
      scale = 1.0;
    const PropertySpecificKeyframeVector& values =
        effect.GetPropertySpecificKeyframes(property);

    CompositorTargetProperty::Type target_property;
    std::unique_ptr<CompositorAnimationCurve> curve;
    DCHECK(timing.timing_function);
    switch (property.GetCSSProperty().PropertyID()) {
      case CSSPropertyOpacity: {
        target_property = CompositorTargetProperty::OPACITY;
        std::unique_ptr<CompositorFloatAnimationCurve> float_curve =
            CompositorFloatAnimationCurve::Create();
        AddKeyframesToCurve(*float_curve, values);
        float_curve->SetTimingFunction(*timing.timing_function);
        float_curve->SetScaledDuration(scale);
        curve = std::move(float_curve);
        break;
      }
      case CSSPropertyFilter:
      case CSSPropertyBackdropFilter: {
        target_property = CompositorTargetProperty::FILTER;
        std::unique_ptr<CompositorFilterAnimationCurve> filter_curve =
            CompositorFilterAnimationCurve::Create();
        AddKeyframesToCurve(*filter_curve, values);
        filter_curve->SetTimingFunction(*timing.timing_function);
        filter_curve->SetScaledDuration(scale);
        curve = std::move(filter_curve);
        break;
      }
      case CSSPropertyRotate:
      case CSSPropertyScale:
      case CSSPropertyTranslate:
      case CSSPropertyTransform: {
        target_property = CompositorTargetProperty::TRANSFORM;
        std::unique_ptr<CompositorTransformAnimationCurve> transform_curve =
            CompositorTransformAnimationCurve::Create();
        AddKeyframesToCurve(*transform_curve, values);
        transform_curve->SetTimingFunction(*timing.timing_function);
        transform_curve->SetScaledDuration(scale);
        curve = std::move(transform_curve);
        break;
      }
      case CSSPropertyVariable: {
        DCHECK(RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled());
        target_property = CompositorTargetProperty::CSS_CUSTOM_PROPERTY;
        // TODO(kevers): Extend support to non-float types.
        std::unique_ptr<CompositorFloatAnimationCurve> float_curve =
            CompositorFloatAnimationCurve::Create();
        AddKeyframesToCurve(*float_curve, values);
        float_curve->SetTimingFunction(*timing.timing_function);
        float_curve->SetScaledDuration(scale);
        curve = std::move(float_curve);
        break;
      }
      default:
        NOTREACHED();
        continue;
    }
    DCHECK(curve.get());

    std::unique_ptr<CompositorKeyframeModel> keyframe_model =
        CompositorKeyframeModel::Create(*curve, target_property, group, 0);

    if (start_time)
      keyframe_model->SetStartTime(start_time.value());

    keyframe_model->SetIterations(compositor_timing.adjusted_iteration_count);
    keyframe_model->SetIterationStart(compositor_timing.iteration_start);
    keyframe_model->SetTimeOffset(compositor_timing.scaled_time_offset);
    keyframe_model->SetDirection(compositor_timing.direction);
    keyframe_model->SetPlaybackRate(compositor_timing.playback_rate);
    keyframe_model->SetFillMode(compositor_timing.fill_mode);
    keyframe_models.push_back(std::move(keyframe_model));
  }
  DCHECK(!keyframe_models.IsEmpty());
}

}  // namespace blink

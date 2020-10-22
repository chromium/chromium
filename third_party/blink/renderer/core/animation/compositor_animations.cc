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
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_color.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_filter_operations.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_transform.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/animation/animation_translation_util.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_color_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_filter_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_filter_keyframe.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_keyframe.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/animation/compositor_transform_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_transform_keyframe.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool ConsiderAnimationAsIncompatible(const Animation& animation,
                                     const Animation& animation_to_add,
                                     const EffectModel& effect_to_add) {
  if (&animation == &animation_to_add)
    return false;

  if (animation.PendingInternal())
    return true;

  switch (animation.CalculateAnimationPlayState()) {
    case Animation::kIdle:
      return false;
    case Animation::kRunning:
      return true;
    case Animation::kPaused:
    case Animation::kFinished:
      if (Animation::HasLowerCompositeOrdering(
              &animation, &animation_to_add,
              Animation::CompareAnimationsOrdering::kPointerOrder)) {
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
         (property.GetCSSProperty().IDEquals(CSSPropertyID::kRotate) ||
          property.GetCSSProperty().IDEquals(CSSPropertyID::kScale) ||
          property.GetCSSProperty().IDEquals(CSSPropertyID::kTransform) ||
          property.GetCSSProperty().IDEquals(CSSPropertyID::kTranslate));
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

CompositorElementIdNamespace
CompositorAnimations::CompositorElementNamespaceForProperty(
    CSSPropertyID property) {
  switch (property) {
    case CSSPropertyID::kOpacity:
    case CSSPropertyID::kBackdropFilter:
      return CompositorElementIdNamespace::kPrimaryEffect;
    case CSSPropertyID::kRotate:
    case CSSPropertyID::kScale:
    case CSSPropertyID::kTranslate:
    case CSSPropertyID::kTransform:
      return CompositorElementIdNamespace::kPrimaryTransform;
    case CSSPropertyID::kFilter:
      return CompositorElementIdNamespace::kEffectFilter;
    case CSSPropertyID::kVariable:
      // TODO(crbug.com/883721): Variables should not require the target
      // element to have any composited property tree nodes - i.e. should
      // not need to check for existence of a property tree node.
      // For now, variable animations target the primary animation target
      // node - the effect namespace.
      return CompositorElementIdNamespace::kPrimaryEffect;
    default:
      NOTREACHED();
  }
  return CompositorElementIdNamespace::kPrimary;
}

CompositorAnimations::FailureReasons
CompositorAnimations::CheckCanStartEffectOnCompositor(
    const Timing& timing,
    const Element& target_element,
    const Animation* animation_to_add,
    const EffectModel& effect,
    const PaintArtifactCompositor* paint_artifact_compositor,
    double animation_playback_rate,
    PropertyHandleSet* unsupported_properties) {
  FailureReasons reasons = kNoFailure;
  const auto& keyframe_effect = To<KeyframeEffectModelBase>(effect);

  LayoutObject* layout_object = target_element.GetLayoutObject();
  if (paint_artifact_compositor) {
    // Elements with subtrees containing will-change: contents are not
    // composited for animations as if the contents change the tiles
    // would need to be rerastered anyways.
    if (layout_object && layout_object->Style()->SubtreeWillChangeContents()) {
      reasons |= kTargetHasInvalidCompositingState;
    }
  }

  PropertyHandleSet properties = keyframe_effect.Properties();
  unsigned transform_property_count = 0;
  for (const auto& property : properties) {
    if (!property.IsCSSProperty()) {
      // None of the below reasons make any sense if |property| isn't CSS, so we
      // skip the rest of the loop in that case.
      reasons |= kAnimationAffectsNonCSSProperties;
      continue;
    }

    if (IsTransformRelatedCSSProperty(property)) {
      // We use this later in computing element IDs too.
      if (layout_object && !layout_object->IsTransformApplicable()) {
        reasons |= kTransformRelatedPropertyCannotBeAcceleratedOnTarget;
      }
      if (const auto* svg_element = DynamicTo<SVGElement>(target_element)) {
        reasons |=
            CheckCanStartTransformAnimationOnCompositorForSVG(*svg_element);
      }
      transform_property_count++;
    }

    const PropertySpecificKeyframeVector& keyframes =
        *keyframe_effect.GetPropertySpecificKeyframes(property);
    DCHECK_GE(keyframes.size(), 2U);
    for (const auto& keyframe : keyframes) {
      if (keyframe->Composite() != EffectModel::kCompositeReplace &&
          !keyframe->IsNeutral()) {
        reasons |= kEffectHasNonReplaceCompositeMode;
      }

      // FIXME: Determine candidacy based on the CSSValue instead of a snapshot
      // CompositorKeyframeValue.
      switch (property.GetCSSProperty().PropertyID()) {
        case CSSPropertyID::kOpacity:
          break;
        case CSSPropertyID::kRotate:
        case CSSPropertyID::kScale:
        case CSSPropertyID::kTranslate:
        case CSSPropertyID::kTransform:
          if (!RuntimeEnabledFeatures::CompositeRelativeKeyframesEnabled()) {
            if (keyframe->GetCompositorKeyframeValue() &&
                To<CompositorKeyframeTransform>(
                    keyframe->GetCompositorKeyframeValue())
                    ->GetTransformOperations()
                    .BoxSizeDependencies()) {
              reasons |= kTransformRelatedPropertyDependsOnBoxSize;
            }
          }
          break;
        case CSSPropertyID::kFilter:
          if (keyframe->GetCompositorKeyframeValue() &&
              To<CompositorKeyframeFilterOperations>(
                  keyframe->GetCompositorKeyframeValue())
                  ->Operations()
                  .HasFilterThatMovesPixels()) {
            reasons |= kFilterRelatedPropertyMayMovePixels;
          }
          break;
        case CSSPropertyID::kBackdropFilter:
          // Backdrop-filter pixel moving filters do not change the layer bounds
          // like regular filters do, so they can still be composited.
          break;
        case CSSPropertyID::kVariable: {
          // Custom properties are supported only in the case of
          // OffMainThreadCSSPaintEnabled, and even then only for some specific
          // property types. Otherwise they are treated as unsupported.
          const CompositorKeyframeValue* keyframe_value =
              keyframe->GetCompositorKeyframeValue();
          if (keyframe_value) {
            DCHECK(RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled());
            DCHECK(keyframe_value->IsDouble() || keyframe_value->IsColor());
            // If a custom property is not used by CSS Paint, then we should not
            // support that on the compositor thread.
            if (layout_object && layout_object->Style() &&
                !layout_object->Style()->HasCSSPaintImagesUsingCustomProperty(
                    property.CustomPropertyName(),
                    layout_object->GetDocument())) {
              if (unsupported_properties) {
                unsupported_properties->insert(property);
              }
              reasons |= kUnsupportedCSSProperty;
            }
            // TODO: Add support for keyframes containing different types
            if (!keyframes.front() ||
                !keyframes.front()->GetCompositorKeyframeValue() ||
                keyframes.front()->GetCompositorKeyframeValue()->GetType() !=
                    keyframe_value->GetType()) {
              reasons |= kMixedKeyframeValueTypes;
            }
          } else {
            // We skip the rest of the loop in this case for the same reason as
            // unsupported CSS properties - see below.
            if (unsupported_properties) {
              unsupported_properties->insert(property);
            }
            reasons |= kUnsupportedCSSProperty;
            continue;
          }
          break;
        }
        default:
          // We skip the rest of the loop in this case for two reasons:
          //   i.  Getting a CompositorElementId below will DCHECK if we pass it
          //       an unsupported property.
          //   ii. GetCompositorKeyframeValue() will be false so we will
          //       accidentally count this as kInvalidAnimationOrEffect as well.
          if (unsupported_properties) {
            unsupported_properties->insert(property);
          }
          reasons |= kUnsupportedCSSProperty;
          continue;
      }

      // If an element does not have style, then it will never have taken a
      // snapshot of its (non-existent) value for the compositor to use.
      if (!keyframe->GetCompositorKeyframeValue()) {
        reasons |= kInvalidAnimationOrEffect;
      }

      if (paint_artifact_compositor) {
        // If we don't have paint properties, we won't have a UniqueId to use
        // for checking here.
        if (!target_element.GetLayoutObject() ||
            !target_element.GetLayoutObject()
                 ->FirstFragment()
                 .PaintProperties()) {
          continue;
        }

        CompositorElementId target_element_id =
            CompositorElementIdFromUniqueObjectId(
                layout_object->UniqueId(),
                CompositorElementNamespaceForProperty(
                    property.GetCSSProperty().PropertyID()));
        DCHECK(target_element_id);
        if (!paint_artifact_compositor->HasComposited(target_element_id))
          reasons |= kTargetHasInvalidCompositingState;
      }
    }
  }

  // TODO: Support multiple transform property animations on the compositor
  if (transform_property_count > 1)
    reasons |= kMultipleTransformAnimationsOnSameTarget;

  if (animation_to_add &&
      HasIncompatibleAnimations(target_element, *animation_to_add, effect)) {
    reasons |= kTargetHasIncompatibleAnimations;
  }

  CompositorTiming out;
  if (!ConvertTimingForCompositor(timing, base::TimeDelta(), out,
                                  animation_playback_rate)) {
    reasons |= kEffectHasUnsupportedTimingParameters;
  }

  return reasons;
}

CompositorAnimations::FailureReasons
CompositorAnimations::CheckCanStartElementOnCompositor(
    const Element& target_element,
    const EffectModel& model) {
  FailureReasons reasons = kNoFailure;

  // Both of these checks are required. It is legal to enable the compositor
  // thread but disable threaded animations, and there are situations where
  // threaded animations are enabled globally but this particular LocalFrame
  // does not have a compositor (e.g. for overlays).
  const Settings* settings = target_element.GetDocument().GetSettings();
  if ((settings && !settings->GetAcceleratedCompositingEnabled()) ||
      !Platform::Current()->IsThreadedAnimationEnabled()) {
    reasons |= kAcceleratedAnimationsDisabled;
  }

  if (const auto* svg_element = DynamicTo<SVGElement>(target_element))
    reasons |= CheckCanStartSVGElementOnCompositor(*svg_element);

  if (const auto* layout_object = target_element.GetLayoutObject()) {
    // We query paint property tree state below to determine whether the
    // animation is compositable. TODO(crbug.com/676456): There is a known
    // lifecycle violation where an animation can be cancelled during style
    // update. See CompositorAnimations::CancelAnimationOnCompositor().
    // When this is fixed we would like to enable the DCHECK below.
    // DCHECK_GE(GetDocument().Lifecycle().GetState(),
    //           DocumentLifecycle::kPrePaintClean);
    bool has_direct_compositing_reasons = false;
    if (const auto* paint_properties =
            layout_object->FirstFragment().PaintProperties()) {
      const auto* transform = paint_properties->Transform();
      const auto* effect = paint_properties->Effect();
      has_direct_compositing_reasons =
          (transform && transform->HasDirectCompositingReasons()) ||
          (effect && effect->HasDirectCompositingReasons());
    }
    if (!has_direct_compositing_reasons &&
        To<KeyframeEffectModelBase>(model).HasNonVariableProperty()) {
      reasons |= kTargetHasInvalidCompositingState;
    }
  } else {
    reasons |= kTargetHasInvalidCompositingState;
  }

  return reasons;
}

// TODO(crbug.com/809685): consider refactor this function.
CompositorAnimations::FailureReasons
CompositorAnimations::CheckCanStartAnimationOnCompositor(
    const Timing& timing,
    const Element& target_element,
    const Animation* animation_to_add,
    const EffectModel& effect,
    const PaintArtifactCompositor* paint_artifact_compositor,
    double animation_playback_rate,
    PropertyHandleSet* unsupported_properties) {
  FailureReasons reasons = CheckCanStartEffectOnCompositor(
      timing, target_element, animation_to_add, effect,
      paint_artifact_compositor, animation_playback_rate,
      unsupported_properties);
  return reasons | CheckCanStartElementOnCompositor(target_element, effect);
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
    base::TimeDelta time_offset,
    const Timing& timing,
    const Animation* animation,
    CompositorAnimation& compositor_animation,
    const EffectModel& effect,
    Vector<int>& started_keyframe_model_ids,
    double animation_playback_rate) {
  DCHECK(started_keyframe_model_ids.IsEmpty());
  // TODO(petermayo): Pass the PaintArtifactCompositor before
  // BlinkGenPropertyTrees is always on.
  DCHECK_EQ(
      CheckCanStartAnimationOnCompositor(timing, element, animation, effect,
                                         nullptr, animation_playback_rate),
      kNoFailure);

  const auto& keyframe_effect = To<KeyframeEffectModelBase>(effect);

  Vector<std::unique_ptr<CompositorKeyframeModel>> keyframe_models;
  GetAnimationOnCompositor(element, timing, group, start_time, time_offset,
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
    int id,
    const EffectModel& model) {
  if (CheckCanStartElementOnCompositor(element, model) != kNoFailure) {
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
    base::TimeDelta pause_time,
    const EffectModel& model) {
  DCHECK_EQ(CheckCanStartElementOnCompositor(element, model), kNoFailure);
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

  CompositorElementIdNamespace element_id_namespace =
      CompositorElementIdNamespace::kPrimary;
  // We create an animation namespace element id when an element has created all
  // property tree nodes which may be required by the keyframe effects. The
  // animation affects multiple element ids, and one is pushed each
  // KeyframeModel. See |GetAnimationOnCompositor|. We use the kPrimaryEffect
  // node to know if nodes have been created for animations.
  element_id_namespace = CompositorElementIdNamespace::kPrimaryEffect;
  compositor_animation->AttachElement(CompositorElementIdFromUniqueObjectId(
      element.GetLayoutObject()->UniqueId(), element_id_namespace));
}

bool CompositorAnimations::ConvertTimingForCompositor(
    const Timing& timing,
    base::TimeDelta time_offset,
    CompositorTiming& out,
    double animation_playback_rate) {
  timing.AssertValid();

  if (animation_playback_rate == 0)
    return false;

  // FIXME: Compositor does not know anything about endDelay.
  if (timing.end_delay != 0)
    return false;

  if (!timing.iteration_duration || !timing.iteration_count ||
      timing.iteration_duration->is_zero() ||
      timing.iteration_duration->is_max())
    return false;

  // Compositor's time offset is positive for seeking into the animation.
  DCHECK(animation_playback_rate);
  double delay = animation_playback_rate > 0 ? timing.start_delay : 0;
  out.scaled_time_offset =
      -base::TimeDelta::FromSecondsD(delay / animation_playback_rate) +
      time_offset;
  // Delay is effectively +/- infinity.
  if (out.scaled_time_offset.is_max() || out.scaled_time_offset.is_min())
    return false;

  out.adjusted_iteration_count = std::isfinite(timing.iteration_count)
                                     ? timing.iteration_count
                                     : std::numeric_limits<double>::infinity();
  out.scaled_duration = timing.iteration_duration.value();
  out.direction = timing.direction;

  out.playback_rate = animation_playback_rate;
  out.fill_mode = timing.fill_mode == Timing::FillMode::AUTO
                      ? Timing::FillMode::NONE
                      : timing.fill_mode;
  out.iteration_start = timing.iteration_start;

  DCHECK_GT(out.scaled_duration, AnimationTimeDelta());
  DCHECK(out.adjusted_iteration_count > 0 ||
         out.adjusted_iteration_count ==
             std::numeric_limits<double>::infinity());
  DCHECK(std::isfinite(out.playback_rate) && out.playback_rate);
  DCHECK_GE(out.iteration_start, 0);

  return true;
}

namespace {

void AddKeyframeToCurve(CompositorFilterAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const CompositorKeyframeValue* value,
                        const TimingFunction& keyframe_timing_function) {
  FilterEffectBuilder builder(FloatRect(), 1);
  CompositorFilterKeyframe filter_keyframe(
      keyframe->Offset(),
      builder.BuildFilterOperations(
          To<CompositorKeyframeFilterOperations>(value)->Operations()),
      keyframe_timing_function);
  curve.AddKeyframe(filter_keyframe);
}

void AddKeyframeToCurve(CompositorFloatAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const CompositorKeyframeValue* value,
                        const TimingFunction& keyframe_timing_function) {
  CompositorFloatKeyframe float_keyframe(
      keyframe->Offset(), To<CompositorKeyframeDouble>(value)->ToDouble(),
      keyframe_timing_function);
  curve.AddKeyframe(float_keyframe);
}

void AddKeyframeToCurve(CompositorColorAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const CompositorKeyframeValue* value,
                        const TimingFunction& keyframe_timing_function) {
  CompositorColorKeyframe color_keyframe(
      keyframe->Offset(), To<CompositorKeyframeColor>(value)->ToColor(),
      keyframe_timing_function);
  curve.AddKeyframe(color_keyframe);
}

void AddKeyframeToCurve(CompositorTransformAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const CompositorKeyframeValue* value,
                        const TimingFunction& keyframe_timing_function,
                        const FloatSize& box_size) {
  CompositorTransformOperations ops;
  ToCompositorTransformOperations(
      To<CompositorKeyframeTransform>(value)->GetTransformOperations(), &ops,
      box_size);

  CompositorTransformKeyframe transform_keyframe(
      keyframe->Offset(), std::move(ops), keyframe_timing_function);
  curve.AddKeyframe(transform_keyframe);
}

template <typename PlatformAnimationCurveType, typename... Args>
void AddKeyframesToCurve(PlatformAnimationCurveType& curve,
                         const PropertySpecificKeyframeVector& keyframes,
                         Args... parameters) {
  Keyframe::PropertySpecificKeyframe* last_keyframe = keyframes.back();
  for (const auto& keyframe : keyframes) {
    const TimingFunction* keyframe_timing_function = nullptr;
    // Ignore timing function of last frame.
    if (keyframe == last_keyframe)
      keyframe_timing_function = LinearTimingFunction::Shared();
    else
      keyframe_timing_function = &keyframe->Easing();

    const CompositorKeyframeValue* value =
        keyframe->GetCompositorKeyframeValue();
    AddKeyframeToCurve(curve, keyframe, value, *keyframe_timing_function,
                       parameters...);
  }
}

}  // namespace

void CompositorAnimations::GetAnimationOnCompositor(
    const Element& target_element,
    const Timing& timing,
    int group,
    base::Optional<double> start_time,
    base::TimeDelta time_offset,
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
    AtomicString custom_property_name = "";
    // If the animation duration is infinite, it doesn't make sense to scale
    // the keyframe offset, so use a scale of 1.0. This is connected to
    // the known issue of how the Web Animations spec handles infinite
    // durations. See https://github.com/w3c/web-animations/issues/142
    double scale = compositor_timing.scaled_duration.InSecondsF();
    if (!std::isfinite(scale))
      scale = 1.0;
    const PropertySpecificKeyframeVector& values =
        *effect.GetPropertySpecificKeyframes(property);

    compositor_target_property::Type target_property;
    std::unique_ptr<CompositorAnimationCurve> curve;
    DCHECK(timing.timing_function);
    switch (property.GetCSSProperty().PropertyID()) {
      case CSSPropertyID::kOpacity: {
        target_property = compositor_target_property::OPACITY;
        auto float_curve = std::make_unique<CompositorFloatAnimationCurve>();
        AddKeyframesToCurve(*float_curve, values);
        float_curve->SetTimingFunction(*timing.timing_function);
        float_curve->SetScaledDuration(scale);
        curve = std::move(float_curve);
        break;
      }
      case CSSPropertyID::kFilter:
      case CSSPropertyID::kBackdropFilter: {
        target_property = compositor_target_property::FILTER;
        if (property.GetCSSProperty().PropertyID() ==
            CSSPropertyID::kBackdropFilter) {
          target_property = compositor_target_property::BACKDROP_FILTER;
        }
        auto filter_curve = std::make_unique<CompositorFilterAnimationCurve>();
        AddKeyframesToCurve(*filter_curve, values);
        filter_curve->SetTimingFunction(*timing.timing_function);
        filter_curve->SetScaledDuration(scale);
        curve = std::move(filter_curve);
        break;
      }
      case CSSPropertyID::kRotate:
      case CSSPropertyID::kScale:
      case CSSPropertyID::kTranslate:
      case CSSPropertyID::kTransform: {
        FloatSize box_size;
        if (RuntimeEnabledFeatures::CompositeRelativeKeyframesEnabled()) {
          box_size = ComputedStyleUtils::ReferenceBoxForTransform(
                         *target_element.GetLayoutObject())
                         .Size();
        }
        target_property = compositor_target_property::TRANSFORM;
        auto transform_curve =
            std::make_unique<CompositorTransformAnimationCurve>();
        AddKeyframesToCurve(*transform_curve, values, box_size);
        transform_curve->SetTimingFunction(*timing.timing_function);
        transform_curve->SetScaledDuration(scale);
        curve = std::move(transform_curve);
        break;
      }
      case CSSPropertyID::kVariable: {
        DCHECK(RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled());
        custom_property_name = property.CustomPropertyName();
        target_property = compositor_target_property::CSS_CUSTOM_PROPERTY;

        // Create curve based on the keyframe value type
        if (values.front()->GetCompositorKeyframeValue()->IsColor()) {
          auto color_curve = std::make_unique<CompositorColorAnimationCurve>();
          AddKeyframesToCurve(*color_curve, values);
          color_curve->SetTimingFunction(*timing.timing_function);
          color_curve->SetScaledDuration(scale);
          curve = std::move(color_curve);
        } else {
          auto float_curve = std::make_unique<CompositorFloatAnimationCurve>();
          AddKeyframesToCurve(*float_curve, values);
          float_curve->SetTimingFunction(*timing.timing_function);
          float_curve->SetScaledDuration(scale);
          curve = std::move(float_curve);
        }
        break;
      }
      default:
        NOTREACHED();
        continue;
    }
    DCHECK(curve.get());

    auto keyframe_model = std::make_unique<CompositorKeyframeModel>(
        *curve, target_property, 0, group, std::move(custom_property_name));

    if (start_time)
      keyframe_model->SetStartTime(start_time.value());

    keyframe_model->SetElementId(CompositorElementIdFromUniqueObjectId(
        target_element.GetLayoutObject()->UniqueId(),
        CompositorElementNamespaceForProperty(
            property.GetCSSProperty().PropertyID())));
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

bool CompositorAnimations::CheckUsesCompositedScrolling(Node* target) {
  if (!target)
    return false;
  DCHECK(target->GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kCompositingAssignmentsClean);
  auto* layout_box_model_object = target->GetLayoutBoxModelObject();
  if (!layout_box_model_object)
    return false;
  return layout_box_model_object->UsesCompositedScrolling();
}

CompositorAnimations::FailureReasons
CompositorAnimations::CheckCanStartSVGElementOnCompositor(
    const SVGElement& svg_element) {
  FailureReasons reasons = kNoFailure;
  if (svg_element.HasMainThreadAnimations())
    reasons |= kTargetHasIncompatibleAnimations;
  return reasons;
}

CompositorAnimations::FailureReasons
CompositorAnimations::CheckCanStartTransformAnimationOnCompositorForSVG(
    const SVGElement& svg_element) {
  FailureReasons reasons = kNoFailure;
  if (const auto* layout_object = svg_element.GetLayoutObject()) {
    if (layout_object->IsSVGViewportContainer()) {
      // Nested SVG doesn't support transforms for now.
      reasons |= kTransformRelatedPropertyCannotBeAcceleratedOnTarget;
    } else if (layout_object->IsSVGForeignObject() &&
               layout_object->StyleRef().EffectiveZoom() != 1) {
      // TODO(crbug.com/1134775): If a foreignObject's effect zoom is not 1,
      // its transform node contains an additional scale which would be removed
      // by composited animation.
      reasons |= kTransformRelatedPropertyCannotBeAcceleratedOnTarget;
    } else if (layout_object->IsSVGTransformableContainer() &&
               !ToLayoutSVGTransformableContainer(layout_object)
                    ->AdditionalTranslation()
                    .IsZero()) {
      // TODO(crbug.com/1134775): Similarly, composited animation would also
      // remove the additional translation of LayoutSVGTransformableContainer.
      reasons |= kTransformRelatedPropertyCannotBeAcceleratedOnTarget;
    }
  }
  return reasons;
}

bool CompositorAnimations::CanStartTransformAnimationOnCompositorForSVG(
    const SVGElement& svg_element) {
  return CheckCanStartSVGElementOnCompositor(svg_element) == kNoFailure &&
         CheckCanStartTransformAnimationOnCompositorForSVG(svg_element) ==
             kNoFailure;
}

}  // namespace blink

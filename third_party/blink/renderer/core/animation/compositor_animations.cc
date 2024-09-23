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

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/filter_animation_curve.h"
#include "cc/base/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_color.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_filter_operations.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_transform.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/css/background_color_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/box_shadow_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/animation/animation_translation_util.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace blink {

namespace {

constexpr auto kCompositableProperties = std::to_array<CSSPropertyID>({
    CSSPropertyID::kBackdropFilter,
    CSSPropertyID::kFilter,
    CSSPropertyID::kOpacity,
    CSSPropertyID::kRotate,
    CSSPropertyID::kScale,
    CSSPropertyID::kTransform,
    CSSPropertyID::kTranslate,
});

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
      NOTREACHED_IN_MIGRATION();
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

bool HasIncompatibleAnimations(const Element& target_element,
                               const Animation& animation_to_add,
                               const EffectModel& effect_to_add) {
  if (!target_element.HasAnimations())
    return false;

  std::array<bool, kCompositableProperties.size()> affects_property;
  for (size_t i = 0; i < kCompositableProperties.size(); i++) {
    PropertyHandle property(CSSProperty::Get(kCompositableProperties[i]));
    affects_property[i] = effect_to_add.Affects(property);
  }

  ElementAnimations* element_animations = target_element.GetElementAnimations();
  DCHECK(element_animations);

  for (const auto& entry : element_animations->Animations()) {
    Animation* attached_animation = entry.key;
    const auto* effect =
        DynamicTo<KeyframeEffect>(attached_animation->effect());
    if (!effect || effect->EffectTarget() != target_element)
      continue;

    if (!ConsiderAnimationAsIncompatible(*attached_animation, animation_to_add,
                                         effect_to_add)) {
      continue;
    }

    for (size_t i = 0; i < kCompositableProperties.size(); i++) {
      if (!affects_property[i])
        continue;

      PropertyHandle property(CSSProperty::Get(kCompositableProperties[i]));
      if (effect->Affects(property))
        return true;
    }
  }

  return false;
}

void DefaultToUnsupportedProperty(
    PropertyHandleSet* unsupported_properties,
    const PropertyHandle& property,
    CompositorAnimations::FailureReasons* reasons) {
  (*reasons) |= CompositorAnimations::kUnsupportedCSSProperty;
  if (unsupported_properties) {
    unsupported_properties->insert(property);
  }
}

// True if it is either a no-op background-color animation, or a no-op custom
// property animation.
bool IsNoOpPaintWorkletOrVariableAnimation(const PropertyHandle& property,
                                      const LayoutObject* layout_object) {
  // If the background color paint worklet was painted, a unique id will be
  // generated. See BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams
  // for details.
  // Similar to that, if a CSS paint worklet was painted, a unique id will be
  // generated. See CSSPaintValue::GetImage for details.
  bool has_unique_id = layout_object->FirstFragment().HasUniqueId();
  if (has_unique_id)
    return false;
  // Now the |has_unique_id| == false.
  bool is_no_op_bgcolor_anim =
      RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled() &&
      property.GetCSSProperty().PropertyID() == CSSPropertyID::kBackgroundColor;
  bool is_no_op_clip_anim =
      RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled() &&
      property.GetCSSProperty().PropertyID() == CSSPropertyID::kClipPath;
  bool is_no_op_variable_anim =
      property.GetCSSProperty().PropertyID() == CSSPropertyID::kVariable;
  return is_no_op_variable_anim || is_no_op_clip_anim || is_no_op_bgcolor_anim;
}

bool CompositedAnimationRequiresProperties(const PropertyHandle& property,
                                           LayoutObject* layout_object) {
  if (!property.IsCSSProperty())
    return false;
  switch (property.GetCSSProperty().PropertyID()) {
    case CSSPropertyID::kRotate:
    case CSSPropertyID::kScale:
    case CSSPropertyID::kTranslate:
    case CSSPropertyID::kTransform:
      return !layout_object || layout_object->IsTransformApplicable();
    case CSSPropertyID::kOpacity:
    case CSSPropertyID::kBackdropFilter:
    case CSSPropertyID::kFilter:
      return true;
    default:
      return false;
  }
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
      return CompositorElementIdNamespace::kRotateTransform;
    case CSSPropertyID::kScale:
      return CompositorElementIdNamespace::kScaleTransform;
    case CSSPropertyID::kTranslate:
      return CompositorElementIdNamespace::kTranslateTransform;
    case CSSPropertyID::kTransform:
      return CompositorElementIdNamespace::kPrimaryTransform;
    case CSSPropertyID::kFilter:
      return CompositorElementIdNamespace::kEffectFilter;
    case CSSPropertyID::kBackgroundColor:
    case CSSPropertyID::kBoxShadow:
    case CSSPropertyID::kClipPath:
    case CSSPropertyID::kVariable:
      // TODO(crbug.com/883721): Variables and these raster-inducing properties
      // should not require the target element to have any composited property
      // tree nodes - i.e. should not need to check for existence of a property
      // tree node. For now, variable animations target the primary animation
      // target node - the effect namespace.
      return CompositorElementIdNamespace::kPrimaryEffect;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return CompositorElementIdNamespace::kPrimary;
}

CompositorAnimations::FailureReasons
CompositorAnimations::CheckCanStartEffectOnCompositor(
    const Timing& timing,
    const Timing::NormalizedTiming& normalized_timing,
    const Element& target_element,
    const Animation* animation_to_add,
    const EffectModel& effect,
    const PaintArtifactCompositor* paint_artifact_compositor,
    double animation_playback_rate,
    PropertyHandleSet* unsupported_properties) {
  FailureReasons reasons = kNoFailure;
  const auto& keyframe_effect = To<KeyframeEffectModelBase>(effect);

  LayoutObject* layout_object = target_element.GetLayoutObject();
  // Elements with subtrees containing will-change: contents are not
  // composited for animations as if the contents change the tiles
  // would need to be rerastered anyways.
  if (layout_object && layout_object->Style()->SubtreeWillChangeContents()) {
    reasons |= kTargetHasInvalidCompositingState;
  }

  const PropertyHandleSet& properties =
      keyframe_effect.EnsureDynamicProperties();
  if (RuntimeEnabledFeatures::StaticAnimationOptimizationEnabled()) {
    // If all properties are static, we don't need to composite. The animation
    // can only change at a phase boundary.
    if (properties.empty()) {
      reasons |= kAnimationHasNoVisibleChange;
    }
  }
  if (keyframe_effect.HasStaticProperty()) {
    UseCounter::Count(target_element.GetDocument(),
                      WebFeature::kStaticPropertyInAnimation);
  }
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
        // TODO(dbaron): We could consider ignoring the
        // transform-related property and still running the others on
        // the compositor.
        reasons |= kTransformRelatedPropertyCannotBeAcceleratedOnTarget;
      }
      if (const auto* svg_element = DynamicTo<SVGElement>(target_element)) {
        reasons |=
            CheckCanStartTransformAnimationOnCompositorForSVG(*svg_element);
        // TODO(https://crbug.com/1278452): When we make the transform tree
        // structure for SVG work like everything else, we should instead
        // start compositing animations of transform properties other than
        // transform.
        if (!property.GetCSSProperty().IDEquals(CSSPropertyID::kTransform))
          reasons |= kSVGTargetHasIndependentTransformProperty;
      }
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
        case CSSPropertyID::kBackgroundColor:
        case CSSPropertyID::kBoxShadow:
        case CSSPropertyID::kClipPath: {
          NativePaintImageGenerator* generator = nullptr;
          // Not having a layout object is a reason for not compositing marked
          // in CompositorAnimations::CheckCanStartElementOnCompositor.
          if (!layout_object) {
            continue;
          }
          if (property.GetCSSProperty().PropertyID() ==
                  CSSPropertyID::kBackgroundColor &&
              RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled()) {
            generator = target_element.GetDocument()
                            .GetFrame()
                            ->GetBackgroundColorPaintImageGenerator();
          } else if (property.GetCSSProperty().PropertyID() ==
                         CSSPropertyID::kBoxShadow &&
                     RuntimeEnabledFeatures ::
                         CompositeBoxShadowAnimationEnabled()) {
            generator = target_element.GetDocument()
                            .GetFrame()
                            ->GetBoxShadowPaintImageGenerator();
          } else if (property.GetCSSProperty().PropertyID() ==
                         CSSPropertyID::kClipPath &&
                     RuntimeEnabledFeatures::
                         CompositeClipPathAnimationEnabled()) {
            generator = target_element.GetDocument()
                            .GetFrame()
                            ->GetClipPathPaintImageGenerator();
          }
          Animation* compositable_animation = nullptr;

          // The generator may be null in tests.
          if (generator) {
            compositable_animation =
                generator->GetAnimationIfCompositable(&target_element);
          }

          if (!compositable_animation) {
            DefaultToUnsupportedProperty(unsupported_properties, property,
                                         &reasons);
          }
          break;
        }
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
              DefaultToUnsupportedProperty(unsupported_properties, property,
                                           &reasons);
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
            DefaultToUnsupportedProperty(unsupported_properties, property,
                                         &reasons);
            continue;
          }
          break;
        }
        default:
          // We skip the rest of the loop in this case because
          // |GetCompositorKeyframeValue()| will be false so we will
          // accidentally count this as kInvalidAnimationOrEffect as well.
          DefaultToUnsupportedProperty(unsupported_properties, property,
                                       &reasons);
          continue;
      }

      // The compositor animation for paint worklet animations do not snapshot
      // the individual keyframes. Instead the keyframes are interpolated within
      // the worklet based on the overall animation progress.
      const bool needs_compositor_keyframe_value =
          CompositedPropertyRequiresSnapshot(property);
      // If an element does not have style, then it will never have taken a
      // snapshot of its (non-existent) value for the compositor to use.
      if (needs_compositor_keyframe_value &&
          !keyframe->GetCompositorKeyframeValue()) {
        reasons |= kInvalidAnimationOrEffect;
      }
    }
  }

  if (CompositorPropertyAnimationsHaveNoEffect(target_element, effect,
                                               paint_artifact_compositor)) {
#if DCHECK_IS_ON()
    if (effect.Affects(PropertyHandle(GetCSSPropertyBackgroundColor()))) {
      ElementAnimations* element_animations =
          target_element.GetElementAnimations();
      DCHECK(element_animations &&
             element_animations->CompositedBackgroundColorStatus() !=
                 ElementAnimations::CompositedPaintStatus::kComposited);
    }
    if (effect.Affects(PropertyHandle(GetCSSPropertyClipPath()))) {
      ElementAnimations* element_animations =
          target_element.GetElementAnimations();
      DCHECK(element_animations &&
             element_animations->CompositedClipPathStatus() !=
                 ElementAnimations::CompositedPaintStatus::kComposited);
    }
#endif
    reasons |= kAnimationHasNoVisibleChange;
  }

  if (animation_to_add &&
      HasIncompatibleAnimations(target_element, *animation_to_add, effect)) {
    reasons |= kTargetHasIncompatibleAnimations;
  }

  CompositorTiming out;
  base::TimeDelta time_offset =
      animation_to_add ? animation_to_add->ComputeCompositorTimeOffset()
                       : base::TimeDelta();
  if (!ConvertTimingForCompositor(timing, normalized_timing, time_offset, out,
                                  animation_playback_rate)) {
    reasons |= kEffectHasUnsupportedTimingParameters;
  }

  return reasons;
}

bool CompositorAnimations::CompositorPropertyAnimationsHaveNoEffect(
    const Element& target_element,
    const EffectModel& effect,
    const PaintArtifactCompositor* paint_artifact_compositor) {
  LayoutObject* layout_object = target_element.GetLayoutObject();

  if (!paint_artifact_compositor) {
    // TODO(pdr): This should return true. This likely only affects tests.
    return false;
  }

  bool any_compositor_properties_missing = false;
  bool any_compositor_properties_present = false;

  const auto& keyframe_effect = To<KeyframeEffectModelBase>(effect);
  const auto& groups = keyframe_effect.GetPropertySpecificKeyframeGroups();
  bool has_paint_properties =
      layout_object && layout_object->FirstFragment().PaintProperties();
  for (const PropertyHandle& property : groups.Keys()) {
    if (!CompositedAnimationRequiresProperties(property, layout_object))
      continue;

    if (!has_paint_properties) {
      // We have an animated property that requires a property node but no paint
      // properties.
      any_compositor_properties_missing = true;
      break;
    }

    CompositorElementId target_element_id =
        CompositorElementIdFromUniqueObjectId(
            layout_object->UniqueId(),
            CompositorAnimations::CompositorElementNamespaceForProperty(
                property.GetCSSProperty().PropertyID()));
    DCHECK(target_element_id);
    if (paint_artifact_compositor->HasComposited(target_element_id))
      any_compositor_properties_present = true;
    else
      any_compositor_properties_missing = true;
  }

  // Because animations are a direct compositing reason for paint properties,
  // the only case when we wouldn't have compositor paint properties if when
  // they were optimized out due to not having an effect. An example of this is
  // hidden animations that do not paint.
  if (any_compositor_properties_missing) {
    // Because we're only considering properties that are animated on this
    // element, we should either have all properties or be missing all
    // properties.
    DCHECK(!any_compositor_properties_present);
    return true;
  }

  return false;
}

CompositorAnimations::FailureReasons
CompositorAnimations::CheckCanStartElementOnCompositor(
    const Element& target_element,
    const EffectModel& model) {
  FailureReasons reasons = kNoFailure;

  // TODO(crbug.com/1287221): Add a more specific reason.
  if (target_element.GetDocument().ShouldForceReduceMotion())
    reasons |= kAcceleratedAnimationsDisabled;

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
    if (layout_object->IsFragmented()) {
      // Composited animation on multiple fragments is not supported.
      reasons |= kTargetHasInvalidCompositingState;
    } else if (const auto* paint_properties =
                   layout_object->FirstFragment().PaintProperties()) {
      const auto* transform = paint_properties->Transform();
      const auto* scale = paint_properties->Scale();
      const auto* rotate = paint_properties->Rotate();
      const auto* translate = paint_properties->Translate();
      const auto* effect = paint_properties->Effect();
      const auto* filter = paint_properties->Filter();
      has_direct_compositing_reasons =
          (transform && transform->HasDirectCompositingReasons()) ||
          (scale && scale->HasDirectCompositingReasons()) ||
          (rotate && rotate->HasDirectCompositingReasons()) ||
          (translate && translate->HasDirectCompositingReasons()) ||
          (effect && effect->HasDirectCompositingReasons()) ||
          (filter && filter->HasDirectCompositingReasons());
    }
    if (!has_direct_compositing_reasons &&
        To<KeyframeEffectModelBase>(model).RequiresPropertyNode()) {
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
    const Timing::NormalizedTiming& normalized_timing,
    const Element& target_element,
    const Animation* animation_to_add,
    const EffectModel& effect,
    const PaintArtifactCompositor* paint_artifact_compositor,
    double animation_playback_rate,
    PropertyHandleSet* unsupported_properties) {
  FailureReasons reasons = CheckCanStartEffectOnCompositor(
      timing, normalized_timing, target_element, animation_to_add, effect,
      paint_artifact_compositor, animation_playback_rate,
      unsupported_properties);
  return reasons | CheckCanStartElementOnCompositor(target_element, effect);
}

void CompositorAnimations::CancelIncompatibleAnimationsOnCompositor(
    const Element& target_element,
    const Animation& animation_to_add,
    const EffectModel& effect_to_add) {
  if (!target_element.HasAnimations())
    return;

  std::array<bool, kCompositableProperties.size()> affects_property;
  for (size_t i = 0; i < kCompositableProperties.size(); i++) {
    PropertyHandle property(CSSProperty::Get(kCompositableProperties[i]));
    affects_property[i] = effect_to_add.Affects(property);
  }

  ElementAnimations* element_animations = target_element.GetElementAnimations();
  DCHECK(element_animations);

  for (const auto& entry : element_animations->Animations()) {
    Animation* attached_animation = entry.key;
    const auto* effect =
        DynamicTo<KeyframeEffect>(attached_animation->effect());
    if (!effect || effect->EffectTarget() != target_element)
      continue;

    if (!ConsiderAnimationAsIncompatible(*attached_animation, animation_to_add,
                                         effect_to_add)) {
      continue;
    }

    for (size_t i = 0; i < kCompositableProperties.size(); i++) {
      if (!affects_property[i])
        continue;

      PropertyHandle property(CSSProperty::Get(kCompositableProperties[i]));
      if (effect->Affects(property)) {
        attached_animation->CancelAnimationOnCompositor();
        break;
      }
    }
  }
}

void CompositorAnimations::StartAnimationOnCompositor(
    const Element& element,
    int group,
    std::optional<double> start_time,
    base::TimeDelta time_offset,
    const Timing& timing,
    const Timing::NormalizedTiming& normalized_timing,
    const Animation* animation,
    CompositorAnimation& compositor_animation,
    const EffectModel& effect,
    Vector<int>& started_keyframe_model_ids,
    double animation_playback_rate,
    bool is_monotonic_timeline,
    bool is_boundary_aligned) {
  DCHECK(started_keyframe_model_ids.empty());
  // TODO(petermayo): Pass the PaintArtifactCompositor before
  // BlinkGenPropertyTrees is always on.
  DCHECK_EQ(CheckCanStartAnimationOnCompositor(
                timing, normalized_timing, element, animation, effect, nullptr,
                animation_playback_rate),
            kNoFailure);

  const auto& keyframe_effect = To<KeyframeEffectModelBase>(effect);

  Vector<std::unique_ptr<cc::KeyframeModel>> keyframe_models;
  GetAnimationOnCompositor(element, timing, normalized_timing, group,
                           start_time, time_offset, keyframe_effect,
                           keyframe_models, animation_playback_rate,
                           is_monotonic_timeline, is_boundary_aligned);
  DCHECK(!keyframe_models.empty());
  for (auto& keyframe_model : keyframe_models) {
    int id = keyframe_model->id();
    compositor_animation.AddKeyframeModel(std::move(keyframe_model));
    started_keyframe_model_ids.push_back(id);
  }
  DCHECK(!started_keyframe_model_ids.empty());
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
    const Timing::NormalizedTiming& normalized_timing,
    base::TimeDelta time_offset,
    CompositorTiming& out,
    double animation_playback_rate,
    bool is_monotonic_timeline,
    bool is_boundary_aligned) {
  timing.AssertValid();

  if (animation_playback_rate == 0)
    return false;

  // FIXME: Compositor does not know anything about endDelay.
  if (!normalized_timing.end_delay.is_zero())
    return false;

  if (!timing.iteration_count ||
      normalized_timing.iteration_duration.is_zero() ||
      normalized_timing.iteration_duration.is_max())
    return false;

  // Compositor's time offset is positive for seeking into the animation.
  DCHECK(animation_playback_rate);
  double delay = animation_playback_rate > 0
                     ? normalized_timing.start_delay.InSecondsF()
                     : 0;

  base::TimeDelta scaled_delay = base::Seconds(delay / animation_playback_rate);

  // Arithmetic operations involving a value that is effectively +/-infinity
  // result in a value that is +/-infinity or undefined. Check before computing
  // the scaled time offset to guard against the following:
  //     infinity - infinity or
  //     -infinity + infinity
  // The result of either of these edge cases is undefined.
  if (scaled_delay.is_max() || scaled_delay.is_min())
    return false;

  out.scaled_time_offset = -scaled_delay + time_offset;
  // Delay is effectively +/- infinity.
  if (out.scaled_time_offset.is_max() || out.scaled_time_offset.is_min())
    return false;

  out.adjusted_iteration_count = std::isfinite(timing.iteration_count)
                                     ? timing.iteration_count
                                     : std::numeric_limits<double>::infinity();
  out.scaled_duration = normalized_timing.iteration_duration;
  out.direction = timing.direction;

  out.playback_rate = animation_playback_rate;
  out.fill_mode = timing.fill_mode == Timing::FillMode::AUTO
                      ? Timing::FillMode::NONE
                      : timing.fill_mode;

  // If we have a monotonic timeline we ensure that the animation will fill
  // after finishing until it is removed by a subsequent main thread commit.
  // This allows developers to apply a post animation style or start a
  // subsequent animation without flicker.
  if ((base::FeatureList::IsEnabled(features::kNoPreserveLastMutation) &&
       is_monotonic_timeline) ||
      is_boundary_aligned) {
    if (animation_playback_rate >= 0) {
      switch (out.fill_mode) {
        case Timing::FillMode::BOTH:
        case Timing::FillMode::FORWARDS:
          break;
        case Timing::FillMode::BACKWARDS:
          out.fill_mode = Timing::FillMode::BOTH;
          break;
        case Timing::FillMode::NONE:
          out.fill_mode = Timing::FillMode::FORWARDS;
          break;
        case Timing::FillMode::AUTO:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    } else {
      switch (out.fill_mode) {
        case Timing::FillMode::BOTH:
        case Timing::FillMode::BACKWARDS:
          break;
        case Timing::FillMode::FORWARDS:
          out.fill_mode = Timing::FillMode::BOTH;
          break;
        case Timing::FillMode::NONE:
          out.fill_mode = Timing::FillMode::BACKWARDS;
          break;
        case Timing::FillMode::AUTO:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
  }

  out.iteration_start = timing.iteration_start;

  // Verify that timing calculations will be correct in gfx::KeyframeModel,
  // which uses times in base::TimeDelta rather than AnimationTimeDelta.
  // AnimationTimeDelta is backed by a double or int64 depending on the compile
  // options. base::TimeDelta is backed by an int64. Thus, base::TimeDelta
  // saturates at a much lower time delta. The largest quantity worked with
  // is the active duration or scaled active duration depending on the magnitude
  // of the playback rate. If this value cannot be expressed in int64, then we
  // cannot composite the animation.
  if (animation_playback_rate < 0) {
    AnimationTimeDelta active_duration =
        out.scaled_duration * out.adjusted_iteration_count;
    if (std::abs(animation_playback_rate) < 1) {
      active_duration /= std::abs(animation_playback_rate);
    }
    // base::TimeDelta ticks are in microseconds.
    if (active_duration.InSecondsF() >
        std::numeric_limits<int64_t>::max() / 1e6) {
      return false;
    }
  }

  DCHECK_GT(out.scaled_duration, AnimationTimeDelta());
  DCHECK(out.adjusted_iteration_count > 0 ||
         out.adjusted_iteration_count ==
             std::numeric_limits<double>::infinity());
  DCHECK(std::isfinite(out.playback_rate) && out.playback_rate);
  DCHECK_GE(out.iteration_start, 0);

  return true;
}

namespace {

void AddKeyframeToCurve(cc::KeyframedFilterAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const CompositorKeyframeValue* value,
                        const TimingFunction& keyframe_timing_function) {
  FilterEffectBuilder builder(gfx::RectF(), std::nullopt, 1, Color::kBlack,
                              mojom::blink::ColorScheme::kLight);
  CompositorFilterOperations operations = builder.BuildFilterOperations(
      To<CompositorKeyframeFilterOperations>(value)->Operations());
  std::unique_ptr<cc::FilterKeyframe> filter_keyframe =
      cc::FilterKeyframe::Create(base::Seconds(keyframe->Offset()),
                                 operations.ReleaseCcFilterOperations(),
                                 keyframe_timing_function.CloneToCC());
  curve.AddKeyframe(std::move(filter_keyframe));
}

void AddKeyframeToCurve(gfx::KeyframedFloatAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const CompositorKeyframeValue* value,
                        const TimingFunction& keyframe_timing_function) {
  std::unique_ptr<gfx::FloatKeyframe> float_keyframe =
      gfx::FloatKeyframe::Create(
          base::Seconds(keyframe->Offset()),
          To<CompositorKeyframeDouble>(value)->ToDouble(),
          keyframe_timing_function.CloneToCC());
  curve.AddKeyframe(std::move(float_keyframe));
}

void AddKeyframeToCurve(gfx::KeyframedColorAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const CompositorKeyframeValue* value,
                        const TimingFunction& keyframe_timing_function) {
  std::unique_ptr<gfx::ColorKeyframe> color_keyframe =
      gfx::ColorKeyframe::Create(base::Seconds(keyframe->Offset()),
                                 To<CompositorKeyframeColor>(value)->ToColor(),
                                 keyframe_timing_function.CloneToCC());
  curve.AddKeyframe(std::move(color_keyframe));
}

void AddKeyframeToCurve(gfx::KeyframedTransformAnimationCurve& curve,
                        Keyframe::PropertySpecificKeyframe* keyframe,
                        const CompositorKeyframeValue* value,
                        const TimingFunction& keyframe_timing_function,
                        const gfx::SizeF& box_size) {
  gfx::TransformOperations ops;
  ToGfxTransformOperations(
      To<CompositorKeyframeTransform>(value)->GetTransformOperations(), &ops,
      box_size);

  std::unique_ptr<gfx::TransformKeyframe> transform_keyframe =
      gfx::TransformKeyframe::Create(base::Seconds(keyframe->Offset()), ops,
                                     keyframe_timing_function.CloneToCC());
  curve.AddKeyframe(std::move(transform_keyframe));
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

void AddKeyframesForPaintWorkletAnimation(
    gfx::KeyframedFloatAnimationCurve& curve) {
  curve.AddKeyframe(gfx::FloatKeyframe::Create(
      base::Seconds(0.0), 0.0, gfx::LinearTimingFunction::Create()));
  curve.AddKeyframe(gfx::FloatKeyframe::Create(
      base::Seconds(1.0), 1.0, gfx::LinearTimingFunction::Create()));
}

}  // namespace

bool CompositorAnimations::CompositedPropertyRequiresSnapshot(
    const PropertyHandle& property) {
  switch (property.GetCSSProperty().PropertyID()) {
    case CSSPropertyID::kClipPath:
    case CSSPropertyID::kBackgroundColor:
      return false;
    default:
      return true;
  }
}

void CompositorAnimations::GetAnimationOnCompositor(
    const Element& target_element,
    const Timing& timing,
    const Timing::NormalizedTiming& normalized_timing,
    int group,
    std::optional<double> start_time,
    base::TimeDelta time_offset,
    const KeyframeEffectModelBase& effect,
    Vector<std::unique_ptr<cc::KeyframeModel>>& keyframe_models,
    double animation_playback_rate,
    bool is_monotonic_timeline,
    bool is_boundary_aligned) {
  DCHECK(keyframe_models.empty());
  CompositorTiming compositor_timing;
  [[maybe_unused]] bool timing_valid = ConvertTimingForCompositor(
      timing, normalized_timing, time_offset, compositor_timing,
      animation_playback_rate, is_monotonic_timeline, is_boundary_aligned);

  const PropertyHandleSet& properties = effect.EnsureDynamicProperties();
  DCHECK(!properties.empty());
  for (const auto& property : properties) {
    // If the animation duration is infinite, it doesn't make sense to scale
    // the keyframe offset, so use a scale of 1.0. This is connected to
    // the known issue of how the Web Animations spec handles infinite
    // durations. See https://github.com/w3c/web-animations/issues/142
    double scale = compositor_timing.scaled_duration.InSecondsF();
    if (!std::isfinite(scale))
      scale = 1.0;
    const PropertySpecificKeyframeVector& values =
        *effect.GetPropertySpecificKeyframes(property);

    std::unique_ptr<gfx::AnimationCurve> curve;
    DCHECK(timing.timing_function);
    std::optional<cc::KeyframeModel::TargetPropertyId> target_property_id =
        std::nullopt;
    CSSPropertyID css_property_id = property.GetCSSProperty().PropertyID();
    switch (css_property_id) {
      case CSSPropertyID::kOpacity: {
        auto float_curve = gfx::KeyframedFloatAnimationCurve::Create();
        AddKeyframesToCurve(*float_curve, values);
        float_curve->SetTimingFunction(timing.timing_function->CloneToCC());
        float_curve->set_scaled_duration(scale);
        curve = std::move(float_curve);
        target_property_id =
            cc::KeyframeModel::TargetPropertyId(cc::TargetProperty::OPACITY);
        break;
      }
      case CSSPropertyID::kFilter:
      case CSSPropertyID::kBackdropFilter: {
        auto filter_curve = cc::KeyframedFilterAnimationCurve::Create();
        AddKeyframesToCurve(*filter_curve, values);
        filter_curve->SetTimingFunction(timing.timing_function->CloneToCC());
        filter_curve->set_scaled_duration(scale);
        curve = std::move(filter_curve);
        target_property_id = cc::KeyframeModel::TargetPropertyId(
            css_property_id == CSSPropertyID::kFilter
                ? cc::TargetProperty::FILTER
                : cc::TargetProperty::BACKDROP_FILTER);
        break;
      }
      case CSSPropertyID::kRotate:
      case CSSPropertyID::kScale:
      case CSSPropertyID::kTranslate:
      case CSSPropertyID::kTransform: {
        gfx::SizeF box_size(ComputedStyleUtils::ReferenceBoxForTransform(
                                *target_element.GetLayoutObject())
                                .size());
        auto transform_curve = gfx::KeyframedTransformAnimationCurve::Create();
        AddKeyframesToCurve(*transform_curve, values, box_size);
        transform_curve->SetTimingFunction(timing.timing_function->CloneToCC());
        transform_curve->set_scaled_duration(scale);
        curve = std::move(transform_curve);
        switch (css_property_id) {
          case CSSPropertyID::kRotate:
            target_property_id =
                cc::KeyframeModel::TargetPropertyId(cc::TargetProperty::ROTATE);
            break;
          case CSSPropertyID::kScale:
            target_property_id =
                cc::KeyframeModel::TargetPropertyId(cc::TargetProperty::SCALE);
            break;
          case CSSPropertyID::kTranslate:
            target_property_id = cc::KeyframeModel::TargetPropertyId(
                cc::TargetProperty::TRANSLATE);
            break;
          case CSSPropertyID::kTransform:
            target_property_id = cc::KeyframeModel::TargetPropertyId(
                cc::TargetProperty::TRANSFORM);
            break;
          default:
            NOTREACHED_IN_MIGRATION()
                << "only possible cases for nested switch";
            break;
        }
        break;
      }
      case CSSPropertyID::kBackgroundColor:
      case CSSPropertyID::kClipPath: {
        CompositorPaintWorkletInput::NativePropertyType native_property_type =
            property.GetCSSProperty().PropertyID() ==
                    CSSPropertyID::kBackgroundColor
                ? CompositorPaintWorkletInput::NativePropertyType::
                      kBackgroundColor
                : CompositorPaintWorkletInput::NativePropertyType::kClipPath;
        auto float_curve = gfx::KeyframedFloatAnimationCurve::Create();

        AddKeyframesForPaintWorkletAnimation(*float_curve);

        float_curve->SetTimingFunction(timing.timing_function->CloneToCC());
        float_curve->set_scaled_duration(scale);
        curve = std::move(float_curve);
        target_property_id = cc::KeyframeModel::TargetPropertyId(
            cc::TargetProperty::NATIVE_PROPERTY, native_property_type);
        break;
      }
      case CSSPropertyID::kVariable: {
        DCHECK(RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled());
        // Create curve based on the keyframe value type
        if (values.front()->GetCompositorKeyframeValue()->IsColor()) {
          auto color_curve = gfx::KeyframedColorAnimationCurve::Create();
          AddKeyframesToCurve(*color_curve, values);
          color_curve->SetTimingFunction(timing.timing_function->CloneToCC());
          color_curve->set_scaled_duration(scale);
          curve = std::move(color_curve);
        } else {
          auto float_curve = gfx::KeyframedFloatAnimationCurve::Create();
          AddKeyframesToCurve(*float_curve, values);
          float_curve->SetTimingFunction(timing.timing_function->CloneToCC());
          float_curve->set_scaled_duration(scale);
          curve = std::move(float_curve);
        }
        target_property_id = cc::KeyframeModel::TargetPropertyId(
            cc::TargetProperty::CSS_CUSTOM_PROPERTY,
            property.CustomPropertyName().Utf8().data());
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        continue;
    }
    DCHECK(curve.get());
    DCHECK(target_property_id.has_value());
    int keyframe_model_id = cc::AnimationIdProvider::NextKeyframeModelId();
    if (!group)
      group = cc::AnimationIdProvider::NextGroupId();
    std::unique_ptr<cc::KeyframeModel> keyframe_model =
        cc::KeyframeModel::Create(std::move(curve), keyframe_model_id, group,
                                  std::move(target_property_id.value()));

    if (start_time) {
      keyframe_model->set_start_time(base::TimeTicks() +
                                     base::Seconds(start_time.value()));
    }

    // By default, it is a kInvalidElementId.
    CompositorElementId id;
    if (!IsNoOpPaintWorkletOrVariableAnimation(
            property, target_element.GetLayoutObject())) {
      id = CompositorElementIdFromUniqueObjectId(
              target_element.GetLayoutObject()->UniqueId(),
              CompositorElementNamespaceForProperty(
                  property.GetCSSProperty().PropertyID()));
    }
    keyframe_model->set_element_id(id);
    keyframe_model->set_iterations(compositor_timing.adjusted_iteration_count);
    keyframe_model->set_iteration_start(compositor_timing.iteration_start);
    keyframe_model->set_time_offset(compositor_timing.scaled_time_offset);
    keyframe_model->set_direction(compositor_timing.direction);
    keyframe_model->set_playback_rate(compositor_timing.playback_rate);
    keyframe_model->set_fill_mode(compositor_timing.fill_mode);
    keyframe_models.push_back(std::move(keyframe_model));
  }
  DCHECK(!keyframe_models.empty());
}

bool CompositorAnimations::CanStartScrollTimelineOnCompositor(Node* target) {
  if (!target) {
    return false;
  }
  DCHECK_GE(target->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  auto* layout_box = target->GetLayoutBox();
  if (!layout_box) {
    return false;
  }
  if (auto* properties = layout_box->FirstFragment().PaintProperties()) {
    return properties->Scroll() &&
           (!RuntimeEnabledFeatures::ScrollNodeForOverflowHiddenEnabled() ||
            properties->Scroll()->UserScrollable());
  }
  return false;
}

CompositorAnimations::FailureReasons
CompositorAnimations::CheckCanStartSVGElementOnCompositor(
    const SVGElement& svg_element) {
  FailureReasons reasons = kNoFailure;
  if (svg_element.HasNonCSSPropertyAnimations())
    reasons |= kTargetHasIncompatibleAnimations;
  if (!svg_element.InstancesForElement().empty()) {
    // TODO(crbug.com/785246): Currently when an SVGElement has svg:use
    // instances, each instance gets style from the original element, using
    // the original element's animation (thus the animation affects
    // transform nodes). This should be removed once instances style
    // themmselves and create their own blink::Animation objects for CSS
    // animations and transitions.
    reasons |= kTargetHasInvalidCompositingState;
  }
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
    } else if (layout_object->StyleRef().EffectiveZoom() != 1) {
      // TODO(crbug.com/1186312): Composited transform animation with non-1
      // effective zoom is incorrectly scaled for now.
      // TODO(crbug.com/1134775): If a foreignObject's effect zoom is not 1,
      // its transform node contains an additional scale which would be removed
      // by composited animation.
      reasons |= kTransformRelatedPropertyCannotBeAcceleratedOnTarget;
    } else if (layout_object->IsSVGTransformableContainer() &&
               !To<LayoutSVGTransformableContainer>(layout_object)
                    ->AdditionalTranslation()
                    .IsZero()) {
      // TODO(crbug.com/1134775): Similarly, composited animation would also
      // remove the additional translation of LayoutSVGTransformableContainer.
      reasons |= kTransformRelatedPropertyCannotBeAcceleratedOnTarget;
    } else if (layout_object->TransformAffectsVectorEffect()) {
      // If the subtree has vector effect, transform affects paint thus
      // animation can not be composited.
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

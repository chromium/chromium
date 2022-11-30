// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timing_input.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyframe_animation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyframe_effect_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_optional_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_offset_phase.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_timelineoffset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_keyframeanimationoptions_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_keyframeeffectoptions_unrestricteddouble.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {

Timing::PlaybackDirection ConvertPlaybackDirection(const String& direction) {
  if (direction == "reverse")
    return Timing::PlaybackDirection::REVERSE;
  if (direction == "alternate")
    return Timing::PlaybackDirection::ALTERNATE_NORMAL;
  if (direction == "alternate-reverse")
    return Timing::PlaybackDirection::ALTERNATE_REVERSE;
  DCHECK_EQ(direction, "normal");
  return Timing::PlaybackDirection::NORMAL;
}

absl::optional<AnimationTimeDelta> ConvertIterationDuration(
    const V8UnionCSSNumericValueOrStringOrUnrestrictedDouble* duration) {
  if (duration->IsUnrestrictedDouble()) {
    return ANIMATION_TIME_DELTA_FROM_MILLISECONDS(
        duration->GetAsUnrestrictedDouble());
  }
  return absl::nullopt;
}

Timing::Delay ConvertDelay(const V8UnionDoubleOrTimelineOffset* delay,
                           double default_percent,
                           ExceptionState& exception_state) {
  Timing::Delay result;
  if (delay->IsDouble()) {
    double delay_in_ms = delay->GetAsDouble();
    DCHECK(std::isfinite(delay_in_ms));
    result.time_delay = ANIMATION_TIME_DELTA_FROM_MILLISECONDS(delay_in_ms);
  } else if (delay->IsTimelineOffset()) {
    if (!RuntimeEnabledFeatures::ScrollTimelineEnabled())
      exception_state.ThrowTypeError("Delay must be a finite double");

    TimelineOffset* timeline_offset = delay->GetAsTimelineOffset();
    V8TimelineOffsetPhase::Enum timeline_offset_phase =
        timeline_offset->hasPhase() ? timeline_offset->phase().AsEnum()
                                    : V8TimelineOffsetPhase::Enum::kCover;
    switch (timeline_offset_phase) {
      case V8TimelineOffsetPhase::Enum::kCover:
        result.phase = Timing::TimelineNamedPhase::kCover;
        break;

      case V8TimelineOffsetPhase::Enum::kContain:
        result.phase = Timing::TimelineNamedPhase::kContain;
        break;

      case V8TimelineOffsetPhase::Enum::kEnter:
        result.phase = Timing::TimelineNamedPhase::kEnter;
        break;

      case V8TimelineOffsetPhase::Enum::kExit:
        result.phase = Timing::TimelineNamedPhase::kExit;
        break;

      default:
        NOTREACHED();
    }
    if (timeline_offset->hasPercent()) {
      CSSUnitValue* percent = timeline_offset->percent()->to(
          CSSPrimitiveValue::UnitType::kPercentage);
      if (!percent) {
        exception_state.ThrowTypeError(
            "CSSNumericValue must be a percentage for animation delay.");
        return result;
      }
      result.relative_offset = percent->value() / 100;
    } else {
      result.relative_offset = 0.01 * default_percent;
    }
  }
  return result;
}

Timing ConvertEffectTiming(const EffectTiming* timing_input,
                           Document* document,
                           ExceptionState& exception_state) {
  Timing timing_output;
  TimingInput::Update(timing_output, timing_input, document, exception_state);
  if (!exception_state.HadException()) {
    timing_output.AssertValid();
  }
  return timing_output;
}

template <class V>
bool UpdateValueIfChanged(V& lhs, const V& rhs) {
  if (lhs != rhs) {
    lhs = rhs;
    return true;
  }
  return false;
}

}  // namespace

Timing TimingInput::Convert(
    const V8UnionKeyframeEffectOptionsOrUnrestrictedDouble* options,
    Document* document,
    ExceptionState& exception_state) {
  if (!options) {
    return Timing();
  }

  switch (options->GetContentType()) {
    case V8UnionKeyframeEffectOptionsOrUnrestrictedDouble::ContentType::
        kKeyframeEffectOptions:
      return ConvertEffectTiming(options->GetAsKeyframeEffectOptions(),
                                 document, exception_state);
    case V8UnionKeyframeEffectOptionsOrUnrestrictedDouble::ContentType::
        kUnrestrictedDouble: {
      // https://drafts.csswg.org/web-animations-1/#dom-keyframeeffect-keyframeeffect
      // If options is a double,
      //   Let timing input be a new EffectTiming object with all members set to
      //   their default values and duration set to options.
      EffectTiming* timing_input = EffectTiming::Create();
      timing_input->setDuration(
          MakeGarbageCollected<
              V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
              options->GetAsUnrestrictedDouble()));
      return ConvertEffectTiming(timing_input, document, exception_state);
    }
  }
  NOTREACHED();
  return Timing();
}

Timing TimingInput::Convert(
    const V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble* options,
    Document* document,
    ExceptionState& exception_state) {
  if (!options) {
    return Timing();
  }

  switch (options->GetContentType()) {
    case V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble::ContentType::
        kKeyframeAnimationOptions:
      return ConvertEffectTiming(options->GetAsKeyframeAnimationOptions(),
                                 document, exception_state);
    case V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble::ContentType::
        kUnrestrictedDouble: {
      // https://drafts.csswg.org/web-animations-1/#dom-keyframeeffect-keyframeeffect
      // If options is a double,
      //   Let timing input be a new EffectTiming object with all members set to
      //   their default values and duration set to options.
      EffectTiming* timing_input = EffectTiming::Create();
      timing_input->setDuration(
          MakeGarbageCollected<
              V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
              options->GetAsUnrestrictedDouble()));
      return ConvertEffectTiming(timing_input, document, exception_state);
    }
  }
  NOTREACHED();
  return Timing();
}

template <class InputTiming>
bool TimingInput::Update(Timing& timing,
                         const InputTiming* input,
                         Document* document,
                         ExceptionState& exception_state) {
  // 1. If the iterationStart member of input is present and less than zero,
  // throw a TypeError and abort this procedure.
  if (input->hasIterationStart() && input->iterationStart() < 0) {
    exception_state.ThrowTypeError("iterationStart must be non-negative");
    return false;
  }

  // 2. If the iterations member of input is present, and less than zero or is
  // the value NaN, throw a TypeError and abort this procedure.
  if (input->hasIterations() &&
      (std::isnan(input->iterations()) || input->iterations() < 0)) {
    exception_state.ThrowTypeError("iterationCount must be non-negative");
    return false;
  }

  // 3. If the duration member of input is present, and less than zero or is the
  // value NaN, throw a TypeError and abort this procedure.
  //
  // We also throw if the value is a string but not 'auto', as per
  // https://github.com/w3c/csswg-drafts/issues/247 .
  if (input->hasDuration()) {
    const char* error_message = "duration must be non-negative or auto";
    switch (input->duration()->GetContentType()) {
      case V8UnionCSSNumericValueOrStringOrUnrestrictedDouble::ContentType::
          kCSSNumericValue:
        exception_state.ThrowTypeError(
            "Setting duration using CSSNumericValue is not supported.");
        return false;
      case V8UnionCSSNumericValueOrStringOrUnrestrictedDouble::ContentType::
          kString:
        if (input->duration()->GetAsString() != "auto") {
          exception_state.ThrowTypeError(error_message);
          return false;
        }
        break;
      case V8UnionCSSNumericValueOrStringOrUnrestrictedDouble::ContentType::
          kUnrestrictedDouble: {
        double duration = input->duration()->GetAsUnrestrictedDouble();
        if (std::isnan(duration) || duration < 0) {
          exception_state.ThrowTypeError(error_message);
          return false;
        }
        break;
      }
    }
  }

  // 4. If the easing member of input is present but cannot be parsed using the
  // <timing-function> production  [CSS-TIMING-1], throw a TypeError and abort
  // this procedure.
  scoped_refptr<TimingFunction> timing_function;
  if (input->hasEasing()) {
    timing_function = AnimationInputHelpers::ParseTimingFunction(
        input->easing(), document, exception_state);
    if (!timing_function) {
      DCHECK(exception_state.HadException());
      return false;
    }
  }

  // 5. Assign each member present in input to the corresponding timing property
  // of effect as follows:
  bool changed = false;
  if (input->hasDelay()) {
    changed |= UpdateValueIfChanged(
        timing.start_delay, ConvertDelay(input->delay(), 0, exception_state));
    timing.SetTimingOverride(Timing::kOverrideStartDelay);
  }
  if (input->hasEndDelay()) {
    changed |= UpdateValueIfChanged(
        timing.end_delay,
        ConvertDelay(input->endDelay(), 100, exception_state));
    timing.SetTimingOverride(Timing::kOverrideEndDelay);
  }
  if (input->hasFill()) {
    changed |= UpdateValueIfChanged(timing.fill_mode,
                                    Timing::StringToFillMode(input->fill()));
    timing.SetTimingOverride(Timing::kOverideFillMode);
  }
  if (input->hasIterationStart()) {
    changed |=
        UpdateValueIfChanged(timing.iteration_start, input->iterationStart());
    timing.SetTimingOverride(Timing::kOverrideIterationStart);
  }
  if (input->hasIterations()) {
    changed |=
        UpdateValueIfChanged(timing.iteration_count, input->iterations());
    timing.SetTimingOverride(Timing::kOverrideIterationCount);
  }
  if (input->hasDuration()) {
    changed |= UpdateValueIfChanged(
        timing.iteration_duration, ConvertIterationDuration(input->duration()));
    timing.SetTimingOverride(Timing::kOverrideDuration);
  }
  if (input->hasDirection()) {
    changed |= UpdateValueIfChanged(
        timing.direction, ConvertPlaybackDirection(input->direction()));
    timing.SetTimingOverride(Timing::kOverrideDirection);
  }
  if (timing_function) {
    // We need to compare the timing functions by underlying value to see if
    // they have really changed, but update the scoped_refptr, so cant use
    // UpdateValueIfChanged.
    changed |= (*timing.timing_function != *timing_function);
    timing.timing_function = timing_function;
    timing.SetTimingOverride(Timing::kOverrideTimingFunction);
  }
  return changed;
}

// Export the OptionalEffectTiming version for AnimationEffect::updateTiming.
template CORE_EXPORT bool TimingInput::Update(Timing&,
                                              const OptionalEffectTiming*,
                                              Document*,
                                              ExceptionState&);

}  // namespace blink

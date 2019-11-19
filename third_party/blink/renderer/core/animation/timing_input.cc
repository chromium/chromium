// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timing_input.h"

#include "third_party/blink/renderer/bindings/core/v8/unrestricted_double_or_keyframe_animation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/unrestricted_double_or_keyframe_effect_options.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/effect_timing.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_options.h"
#include "third_party/blink/renderer/core/animation/optional_effect_timing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

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

base::Optional<AnimationTimeDelta> ConvertIterationDuration(
    const UnrestrictedDoubleOrString& duration) {
  if (duration.IsUnrestrictedDouble()) {
    return AnimationTimeDelta::FromMillisecondsD(
        duration.GetAsUnrestrictedDouble());
  }
  return base::nullopt;
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
    const UnrestrictedDoubleOrKeyframeEffectOptions& options,
    Document* document,
    ExceptionState& exception_state) {
  if (options.IsNull()) {
    return Timing();
  }

  if (options.IsKeyframeEffectOptions()) {
    return ConvertEffectTiming(options.GetAsKeyframeEffectOptions(), document,
                               exception_state);
  }

  DCHECK(options.IsUnrestrictedDouble());

  // https://drafts.csswg.org/web-animations-1/#dom-keyframeeffect-keyframeeffect
  // If options is a double,
  //   Let timing input be a new EffectTiming object with all members set to
  //   their default values and duration set to options.
  EffectTiming* timing_input = EffectTiming::Create();
  timing_input->setDuration(UnrestrictedDoubleOrString::FromUnrestrictedDouble(
      options.GetAsUnrestrictedDouble()));
  return ConvertEffectTiming(timing_input, document, exception_state);
}

Timing TimingInput::Convert(
    const UnrestrictedDoubleOrKeyframeAnimationOptions& options,
    Document* document,
    ExceptionState& exception_state) {
  if (options.IsNull())
    return Timing();

  if (options.IsKeyframeAnimationOptions()) {
    return ConvertEffectTiming(options.GetAsKeyframeAnimationOptions(),
                               document, exception_state);
  }

  DCHECK(options.IsUnrestrictedDouble());

  // https://drafts.csswg.org/web-animations-1/#dom-keyframeeffect-keyframeeffect
  // If options is a double,
  //   Let timing input be a new EffectTiming object with all members set to
  //   their default values and duration set to options.
  EffectTiming* timing_input = EffectTiming::Create();
  timing_input->setDuration(UnrestrictedDoubleOrString::FromUnrestrictedDouble(
      options.GetAsUnrestrictedDouble()));
  return ConvertEffectTiming(timing_input, document, exception_state);
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
    if (input->duration().IsUnrestrictedDouble()) {
      double duration = input->duration().GetAsUnrestrictedDouble();
      if (std::isnan(duration) || duration < 0) {
        exception_state.ThrowTypeError(error_message);
        return false;
      }
    } else if (input->duration().GetAsString() != "auto") {
      exception_state.ThrowTypeError(error_message);
      return false;
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
    DCHECK(std::isfinite(input->delay()));
    changed |= UpdateValueIfChanged(timing.start_delay, input->delay() / 1000);
  }
  if (input->hasEndDelay()) {
    DCHECK(std::isfinite(input->endDelay()));
    changed |= UpdateValueIfChanged(timing.end_delay, input->endDelay() / 1000);
  }
  if (input->hasFill()) {
    changed |= UpdateValueIfChanged(timing.fill_mode,
                                    Timing::StringToFillMode(input->fill()));
  }
  if (input->hasIterationStart()) {
    changed |=
        UpdateValueIfChanged(timing.iteration_start, input->iterationStart());
  }
  if (input->hasIterations()) {
    changed |=
        UpdateValueIfChanged(timing.iteration_count, input->iterations());
  }
  if (input->hasDuration()) {
    changed |= UpdateValueIfChanged(
        timing.iteration_duration, ConvertIterationDuration(input->duration()));
  }
  if (input->hasDirection()) {
    changed |= UpdateValueIfChanged(
        timing.direction, ConvertPlaybackDirection(input->direction()));
  }
  if (timing_function) {
    // We need to compare the timing functions by underlying value to see if
    // they have really changed, but update the scoped_refptr, so cant use
    // UpdateValueIfChanged.
    changed |= (*timing.timing_function != *timing_function);
    timing.timing_function = timing_function;
  }

  return changed;
}

// Export the OptionalEffectTiming version for AnimationEffect::updateTiming.
template CORE_EXPORT bool TimingInput::Update(Timing&,
                                              const OptionalEffectTiming*,
                                              Document*,
                                              ExceptionState&);

}  // namespace blink

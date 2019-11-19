/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/audio_param_timeline.h"

#include <algorithm>
#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <emmintrin.h>
#endif

namespace blink {

// For a SetTarget event, we want the event to terminate eventually so that
// we can stop using the timeline to compute the values.  See
// |HasSetTargetConverged()| for the algorithm.  |kSetTargetThreshold| is
// exp(-kTimeConstantsToConverge).
const float kTimeConstantsToConverge = 10;
const float kSetTargetThreshold = 4.539992976248485e-05;

static bool IsNonNegativeAudioParamTime(double time,
                                        ExceptionState& exception_state,
                                        String message = "Time") {
  if (time >= 0)
    return true;

  exception_state.ThrowRangeError(
      message +
      " must be a finite non-negative number: " + String::Number(time));
  return false;
}

static bool IsPositiveAudioParamTime(double time,
                                     ExceptionState& exception_state,
                                     String message) {
  if (time > 0)
    return true;

  exception_state.ThrowRangeError(
      message + " must be a finite positive number: " + String::Number(time));
  return false;
}

String AudioParamTimeline::EventToString(const ParamEvent& event) const {
  // The default arguments for most automation methods is the value and the
  // time.
  String args =
      String::Number(event.Value()) + ", " + String::Number(event.Time(), 16);

  // Get a nice printable name for the event and update the args if necessary.
  String s;
  switch (event.GetType()) {
    case ParamEvent::kSetValue:
      s = "setValueAtTime";
      break;
    case ParamEvent::kLinearRampToValue:
      s = "linearRampToValueAtTime";
      break;
    case ParamEvent::kExponentialRampToValue:
      s = "exponentialRampToValue";
      break;
    case ParamEvent::kSetTarget:
      s = "setTargetAtTime";
      // This has an extra time constant arg
      args = args + ", " + String::Number(event.TimeConstant(), 16);
      break;
    case ParamEvent::kSetValueCurve:
      s = "setValueCurveAtTime";
      // Replace the default arg, using "..." to denote the curve argument.
      args = "..., " + String::Number(event.Time(), 16) + ", " +
             String::Number(event.Duration(), 16);
      break;
    case ParamEvent::kCancelValues:
    case ParamEvent::kSetValueCurveEnd:
    // Fall through; we should never have to print out the internal
    // |kCancelValues| or |kSetValueCurveEnd| event.
    case ParamEvent::kLastType:
      NOTREACHED();
      break;
  };

  return s + "(" + args + ")";
}

// Computes the value of a linear ramp event at time t with the given event
// parameters.
float AudioParamTimeline::LinearRampAtTime(double t,
                                           float value1,
                                           double time1,
                                           float value2,
                                           double time2) {
  return value1 + (value2 - value1) * (t - time1) / (time2 - time1);
}

// Computes the value of an exponential ramp event at time t with the given
// event parameters.
float AudioParamTimeline::ExponentialRampAtTime(double t,
                                                float value1,
                                                double time1,
                                                float value2,
                                                double time2) {
  return value1 * pow(value2 / value1, (t - time1) / (time2 - time1));
}

// Compute the value of a set target event at time t with the given event
// parameters.
float AudioParamTimeline::TargetValueAtTime(double t,
                                            float value1,
                                            double time1,
                                            float value2,
                                            float time_constant) {
  return value2 + (value1 - value2) * exp(-(t - time1) / time_constant);
}

// Compute the value of a set curve event at time t with the given event
// parameters.
float AudioParamTimeline::ValueCurveAtTime(double t,
                                           double time1,
                                           double duration,
                                           const float* curve_data,
                                           unsigned curve_length) {
  double curve_index = (curve_length - 1) / duration * (t - time1);
  unsigned k = std::min(static_cast<unsigned>(curve_index), curve_length - 1);
  unsigned k1 = std::min(k + 1, curve_length - 1);
  float c0 = curve_data[k];
  float c1 = curve_data[k1];
  float delta = std::min(curve_index - k, 1.0);

  return c0 + (c1 - c0) * delta;
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::CreateSetValueEvent(float value, double time) {
  return base::WrapUnique(new ParamEvent(ParamEvent::kSetValue, value, time));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::CreateLinearRampEvent(float value,
                                                      double time,
                                                      float initial_value,
                                                      double call_time) {
  return base::WrapUnique(new ParamEvent(ParamEvent::kLinearRampToValue, value,
                                         time, initial_value, call_time));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::CreateExponentialRampEvent(float value,
                                                           double time,
                                                           float initial_value,
                                                           double call_time) {
  return base::WrapUnique(new ParamEvent(ParamEvent::kExponentialRampToValue,
                                         value, time, initial_value,
                                         call_time));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::CreateSetTargetEvent(float value,
                                                     double time,
                                                     double time_constant) {
  // The time line code does not expect a timeConstant of 0. (IT
  // returns NaN or Infinity due to division by zero.  The caller
  // should have converted this to a SetValueEvent.
  DCHECK_NE(time_constant, 0);
  return base::WrapUnique(
      new ParamEvent(ParamEvent::kSetTarget, value, time, time_constant));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::CreateSetValueCurveEvent(
    const Vector<float>& curve,
    double time,
    double duration) {
  double curve_points = (curve.size() - 1) / duration;
  float end_value = curve.data()[curve.size() - 1];

  return base::WrapUnique(new ParamEvent(ParamEvent::kSetValueCurve, time,
                                         duration, curve, curve_points,
                                         end_value));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::CreateSetValueCurveEndEvent(float value,
                                                            double time) {
  return base::WrapUnique(
      new ParamEvent(ParamEvent::kSetValueCurveEnd, value, time));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::CreateCancelValuesEvent(
    double time,
    std::unique_ptr<ParamEvent> saved_event) {
  if (saved_event) {
    // The savedEvent can only have certain event types.  Verify that.
    ParamEvent::Type saved_type = saved_event->GetType();

    DCHECK_NE(saved_type, ParamEvent::kLastType);
    DCHECK(saved_type == ParamEvent::kLinearRampToValue ||
           saved_type == ParamEvent::kExponentialRampToValue ||
           saved_type == ParamEvent::kSetValueCurve);
  }

  return base::WrapUnique(
      new ParamEvent(ParamEvent::kCancelValues, time, std::move(saved_event)));
}

std::unique_ptr<AudioParamTimeline::ParamEvent>
AudioParamTimeline::ParamEvent::CreateGeneralEvent(
    Type type,
    float value,
    double time,
    float initial_value,
    double call_time,
    double time_constant,
    double duration,
    Vector<float>& curve,
    double curve_points_per_second,
    float curve_end_value,
    std::unique_ptr<ParamEvent> saved_event) {
  return base::WrapUnique(new ParamEvent(
      type, value, time, initial_value, call_time, time_constant, duration,
      curve, curve_points_per_second, curve_end_value, std::move(saved_event)));
}

AudioParamTimeline::ParamEvent* AudioParamTimeline::ParamEvent::SavedEvent()
    const {
  DCHECK_EQ(GetType(), ParamEvent::kCancelValues);
  return saved_event_.get();
}

bool AudioParamTimeline::ParamEvent::HasDefaultCancelledValue() const {
  DCHECK_EQ(GetType(), ParamEvent::kCancelValues);
  return has_default_cancelled_value_;
}

void AudioParamTimeline::ParamEvent::SetCancelledValue(float value) {
  DCHECK_EQ(GetType(), ParamEvent::kCancelValues);
  value_ = value;
  has_default_cancelled_value_ = true;
}

// General event
AudioParamTimeline::ParamEvent::ParamEvent(
    ParamEvent::Type type,
    float value,
    double time,
    float initial_value,
    double call_time,
    double time_constant,
    double duration,
    Vector<float>& curve,
    double curve_points_per_second,
    float curve_end_value,
    std::unique_ptr<ParamEvent> saved_event)
    : type_(type),
      value_(value),
      time_(time),
      initial_value_(initial_value),
      call_time_(call_time),
      time_constant_(time_constant),
      duration_(duration),
      curve_points_per_second_(curve_points_per_second),
      curve_end_value_(curve_end_value),
      saved_event_(std::move(saved_event)),
      has_default_cancelled_value_(false) {
  curve_ = curve;
}

// Create simplest event needing just a value and time, like setValueAtTime
AudioParamTimeline::ParamEvent::ParamEvent(ParamEvent::Type type,
                                           float value,
                                           double time)
    : type_(type),
      value_(value),
      time_(time),
      initial_value_(0),
      call_time_(0),
      time_constant_(0),
      duration_(0),
      curve_points_per_second_(0),
      curve_end_value_(0),
      saved_event_(nullptr),
      has_default_cancelled_value_(false) {
  DCHECK(type == ParamEvent::kSetValue ||
         type == ParamEvent::kSetValueCurveEnd);
}

// Create a linear or exponential ramp that requires an initial value and
// time in case
// there is no actual event that preceeds this event.
AudioParamTimeline::ParamEvent::ParamEvent(ParamEvent::Type type,
                                           float value,
                                           double time,
                                           float initial_value,
                                           double call_time)
    : type_(type),
      value_(value),
      time_(time),
      initial_value_(initial_value),
      call_time_(call_time),
      time_constant_(0),
      duration_(0),
      curve_points_per_second_(0),
      curve_end_value_(0),
      saved_event_(nullptr),
      has_default_cancelled_value_(false) {
  DCHECK(type == ParamEvent::kLinearRampToValue ||
         type == ParamEvent::kExponentialRampToValue);
}

// Create an event needing a time constant (setTargetAtTime)
AudioParamTimeline::ParamEvent::ParamEvent(ParamEvent::Type type,
                                           float value,
                                           double time,
                                           double time_constant)
    : type_(type),
      value_(value),
      time_(time),
      initial_value_(0),
      call_time_(0),
      time_constant_(time_constant),
      duration_(0),
      curve_points_per_second_(0),
      curve_end_value_(0),
      saved_event_(nullptr),
      has_default_cancelled_value_(false) {
  DCHECK_EQ(type, ParamEvent::kSetTarget);
}

// Create a setValueCurve event
AudioParamTimeline::ParamEvent::ParamEvent(ParamEvent::Type type,
                                           double time,
                                           double duration,
                                           const Vector<float>& curve,
                                           double curve_points_per_second,
                                           float curve_end_value)
    : type_(type),
      value_(0),
      time_(time),
      initial_value_(0),
      call_time_(0),
      time_constant_(0),
      duration_(duration),
      curve_points_per_second_(curve_points_per_second),
      curve_end_value_(curve_end_value),
      saved_event_(nullptr),
      has_default_cancelled_value_(false) {
  DCHECK_EQ(type, ParamEvent::kSetValueCurve);
  unsigned curve_length = curve.size();
  curve_.resize(curve_length);
  memcpy(curve_.data(), curve.data(), curve_length * sizeof(float));
}

// Create CancelValues event
AudioParamTimeline::ParamEvent::ParamEvent(
    ParamEvent::Type type,
    double time,
    std::unique_ptr<ParamEvent> saved_event)
    : type_(type),
      value_(0),
      time_(time),
      initial_value_(0),
      call_time_(0),
      time_constant_(0),
      duration_(0),
      curve_points_per_second_(0),
      curve_end_value_(0),
      saved_event_(std::move(saved_event)),
      has_default_cancelled_value_(false) {
  DCHECK_EQ(type, ParamEvent::kCancelValues);
}

void AudioParamTimeline::SetValueAtTime(float value,
                                        double time,
                                        ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(time, exception_state))
    return;

  MutexLocker locker(events_lock_);
  InsertEvent(ParamEvent::CreateSetValueEvent(value, time), exception_state);
}

void AudioParamTimeline::LinearRampToValueAtTime(
    float value,
    double time,
    float initial_value,
    double call_time,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(time, exception_state))
    return;

  MutexLocker locker(events_lock_);
  InsertEvent(
      ParamEvent::CreateLinearRampEvent(value, time, initial_value, call_time),
      exception_state);
}

void AudioParamTimeline::ExponentialRampToValueAtTime(
    float value,
    double time,
    float initial_value,
    double call_time,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(time, exception_state))
    return;

  if (!value) {
    exception_state.ThrowRangeError(
        "The float target value provided (" + String::Number(value) +
        ") should not be in the range (" +
        String::Number(-std::numeric_limits<float>::denorm_min()) + ", " +
        String::Number(std::numeric_limits<float>::denorm_min()) + ").");
    return;
  }

  MutexLocker locker(events_lock_);
  InsertEvent(ParamEvent::CreateExponentialRampEvent(value, time, initial_value,
                                                     call_time),
              exception_state);
}

void AudioParamTimeline::SetTargetAtTime(float target,
                                         double time,
                                         double time_constant,
                                         ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(time, exception_state) ||
      !IsNonNegativeAudioParamTime(time_constant, exception_state,
                                   "Time constant"))
    return;

  MutexLocker locker(events_lock_);

  // If timeConstant = 0, we instantly jump to the target value, so
  // insert a SetValueEvent instead of SetTargetEvent.
  if (time_constant == 0) {
    InsertEvent(ParamEvent::CreateSetValueEvent(target, time), exception_state);
  } else {
    InsertEvent(ParamEvent::CreateSetTargetEvent(target, time, time_constant),
                exception_state);
  }
}

void AudioParamTimeline::SetValueCurveAtTime(const Vector<float>& curve,
                                             double time,
                                             double duration,
                                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(time, exception_state) ||
      !IsPositiveAudioParamTime(duration, exception_state, "Duration"))
    return;

  if (curve.size() < 2) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        ExceptionMessages::IndexExceedsMinimumBound("curve length",
                                                    curve.size(), 2u));
    return;
  }

  MutexLocker locker(events_lock_);
  InsertEvent(ParamEvent::CreateSetValueCurveEvent(curve, time, duration),
              exception_state);

  // Insert a setValueAtTime event too to establish an event so that all
  // following events will process from the end of the curve instead of the
  // beginning.
  InsertEvent(ParamEvent::CreateSetValueCurveEndEvent(
                  curve.data()[curve.size() - 1], time + duration),
              exception_state);
}

void AudioParamTimeline::InsertEvent(std::unique_ptr<ParamEvent> event,
                                     ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Sanity check the event. Be super careful we're not getting infected with
  // NaN or Inf. These should have been handled by the caller.
  DCHECK_LT(event->GetType(), ParamEvent::kLastType);
  DCHECK(std::isfinite(event->Value()));
  DCHECK(std::isfinite(event->Time()));
  DCHECK(std::isfinite(event->TimeConstant()));
  DCHECK(std::isfinite(event->Duration()));
  DCHECK_GE(event->Duration(), 0);

  unsigned i = 0;
  double insert_time = event->Time();

  if (!events_.size() &&
      (event->GetType() == ParamEvent::kLinearRampToValue ||
       event->GetType() == ParamEvent::kExponentialRampToValue)) {
    // There are no events preceding these ramps.  Insert a new
    // setValueAtTime event to set the starting point for these
    // events.  Use a time of 0 to make sure it preceeds all other
    // events.  This will get fixed when when handle new events.
    events_.insert(0, AudioParamTimeline::ParamEvent::CreateSetValueEvent(
                          event->InitialValue(), 0));
    new_events_.insert(events_[0].get());
  }

  for (i = 0; i < events_.size(); ++i) {
    if (event->GetType() == ParamEvent::kSetValueCurve) {
      // If this event is a SetValueCurve, make sure it doesn't overlap any
      // existing event. It's ok if the SetValueCurve starts at the same time as
      // the end of some other duration.
      double end_time = event->Time() + event->Duration();
      ParamEvent::Type test_type = events_[i]->GetType();
      // Events of type |kSetValueCurveEnd| or |kCancelValues| never
      // conflict.
      if (!(test_type == ParamEvent::kSetValueCurveEnd ||
            test_type == ParamEvent::kCancelValues)) {
        if (test_type == ParamEvent::kSetValueCurve) {
          // A SetValueCurve overlapping an existing SetValueCurve requires
          // special care.
          double test_end_time = events_[i]->Time() + events_[i]->Duration();
          // Test if old event starts somewhere in the middle of the new event.
          bool overlap = (events_[i]->Time() >= event->Time() &&
                          events_[i]->Time() < end_time);
          // Test if old event ends somewhere in the middle of the new event.
          overlap = overlap ||
                    (test_end_time > event->Time() && test_end_time < end_time);
          // Test if new event starts somewhere in the middle of the old event.
          overlap = overlap || (event->Time() >= events_[i]->Time() &&
                                event->Time() < test_end_time);
          // Test if new event ends somewhere in the middle of the old event.
          overlap = overlap || (end_time >= events_[i]->Time() &&
                                end_time < test_end_time);
          if (overlap) {
            // If the start time of the event overlaps the start/end of an
            // existing event or if the existing event end overlaps the
            // start/end of the event, it's an error.
            exception_state.ThrowDOMException(
                DOMExceptionCode::kNotSupportedError,
                EventToString(*event) + " overlaps " +
                    EventToString(*events_[i]));
            return;
          }
        } else {
          if (events_[i]->Time() > event->Time() &&
              events_[i]->Time() < end_time) {
            exception_state.ThrowDOMException(
                DOMExceptionCode::kNotSupportedError,
                EventToString(*event) + " overlaps " +
                    EventToString(*events_[i]));
            return;
          }
        }
      }
    } else {
      // Otherwise, make sure this event doesn't overlap any existing
      // SetValueCurve event.
      if (events_[i]->GetType() == ParamEvent::kSetValueCurve) {
        double end_time = events_[i]->Time() + events_[i]->Duration();
        if (event->GetType() != ParamEvent::kSetValueCurveEnd &&
            event->Time() >= events_[i]->Time() && event->Time() < end_time) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              EventToString(*event) + " overlaps " +
                  EventToString(*events_[i]));
          return;
        }
      }
    }

    if (events_[i]->Time() > insert_time)
      break;
  }

  events_.insert(i, std::move(event));
  new_events_.insert(events_[i].get());
}

bool AudioParamTimeline::HasValues(size_t current_frame,
                                   double sample_rate) const {
  MutexTryLocker try_locker(events_lock_);

  if (try_locker.Locked()) {
    unsigned n_events = events_.size();

    // Clearly, if there are no scheduled events, we have no timeline values.
    if (n_events == 0)
      return false;

    // Handle the case where the first event (of certain types) is in the
    // future.  Then, no sample-accurate processing is needed because the event
    // hasn't started.
    if (events_[0]->Time() >
        (current_frame + audio_utilities::kRenderQuantumFrames) / sample_rate) {
      switch (events_[0]->GetType()) {
        case ParamEvent::kSetTarget:
        case ParamEvent::kSetValue:
        case ParamEvent::kSetValueCurve:
          // If the first event is one of these types, and the event starts
          // after the end of the current render quantum, we don't need to do
          // the slow sample-accurate path.
          return false;
        default:
          // Handle other event types below.
          break;
      }
    }

    // If there are at least 2 events in the timeline, assume there are timeline
    // values.  This could be optimized to be more careful, but checking is
    // complicated and keeping this consistent with |ValuesForFrameRangeImpl()|
    // will be hard, so it's probably best to let the general timeline handle
    // this until the events are in the past.
    if (n_events >= 2)
      return true;

    // We have exactly one event in the timeline.
    switch (events_[0]->GetType()) {
      case ParamEvent::kSetTarget:
        // Need automation if the event starts somewhere before the
        // end of the current render quantum.
        return events_[0]->Time() <=
               (current_frame + audio_utilities::kRenderQuantumFrames) /
                   sample_rate;
      case ParamEvent::kSetValue:
      case ParamEvent::kLinearRampToValue:
      case ParamEvent::kExponentialRampToValue:
      case ParamEvent::kCancelValues:
      case ParamEvent::kSetValueCurveEnd:
        // If these events are in the past, we don't need any automation; the
        // value is a constant.
        return !(events_[0]->Time() < current_frame / sample_rate);
      case ParamEvent::kSetValueCurve: {
        double curve_end_time = events_[0]->Time() + events_[0]->Duration();
        double current_time = current_frame / sample_rate;

        return (events_[0]->Time() <= current_time) &&
               (current_time < curve_end_time);
      }
      case ParamEvent::kLastType:
        NOTREACHED();
        return true;
    }
  }

  // Can't get the lock so that means the main thread is trying to insert an
  // event.  Just return true then.  If the main thread releases the lock before
  // valueForContextTime or valuesForFrameRange runs, then the there will be an
  // event on the timeline, so everything is fine.  If the lock is held so that
  // neither valueForContextTime nor valuesForFrameRange can run, this is ok
  // too, because they have tryLocks to produce a default value.  The event will
  // then get processed in the next rendering quantum.
  //
  // Don't want to return false here because that would confuse the processing
  // of the timeline if previously we returned true and now suddenly return
  // false, only to return true on the next rendering quantum.  Currently, once
  // a timeline has been introduced it is always true forever because m_events
  // never shrinks.
  return true;
}

void AudioParamTimeline::CancelScheduledValues(
    double start_time,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  MutexLocker locker(events_lock_);

  // Remove all events starting at startTime.
  for (wtf_size_t i = 0; i < events_.size(); ++i) {
    if (events_[i]->Time() >= start_time) {
      RemoveCancelledEvents(i);
      break;
    }
  }
}

void AudioParamTimeline::CancelAndHoldAtTime(double cancel_time,
                                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(cancel_time, exception_state))
    return;

  MutexLocker locker(events_lock_);

  wtf_size_t i;
  // Find the first event at or just past cancelTime.
  for (i = 0; i < events_.size(); ++i) {
    if (events_[i]->Time() > cancel_time) {
      break;
    }
  }

  // The event that is being cancelled.  This is the event just past
  // cancelTime, if any.
  wtf_size_t cancelled_event_index = i;

  // If the event just before cancelTime is a SetTarget or SetValueCurve
  // event, we need to handle that event specially instead of the event after.
  if (i > 0 && ((events_[i - 1]->GetType() == ParamEvent::kSetTarget) ||
                (events_[i - 1]->GetType() == ParamEvent::kSetValueCurve))) {
    cancelled_event_index = i - 1;
  } else if (i >= events_.size()) {
    // If there were no events occurring after |cancelTime| (and the
    // previous event is not SetTarget or SetValueCurve, we're done.
    return;
  }

  // cancelledEvent is the event that is being cancelled.
  ParamEvent* cancelled_event = events_[cancelled_event_index].get();
  ParamEvent::Type event_type = cancelled_event->GetType();

  // New event to be inserted, if any, and a SetValueEvent if needed.
  std::unique_ptr<ParamEvent> new_event = nullptr;
  std::unique_ptr<ParamEvent> new_set_value_event = nullptr;

  switch (event_type) {
    case ParamEvent::kLinearRampToValue:
    case ParamEvent::kExponentialRampToValue: {
      // For these events we need to remember the parameters of the event
      // for a CancelValues event so that we can properly cancel the event
      // and hold the value.
      std::unique_ptr<ParamEvent> saved_event = ParamEvent::CreateGeneralEvent(
          event_type, cancelled_event->Value(), cancelled_event->Time(),
          cancelled_event->InitialValue(), cancelled_event->CallTime(),
          cancelled_event->TimeConstant(), cancelled_event->Duration(),
          cancelled_event->Curve(), cancelled_event->CurvePointsPerSecond(),
          cancelled_event->CurveEndValue(), nullptr);

      new_event = ParamEvent::CreateCancelValuesEvent(cancel_time,
                                                      std::move(saved_event));
    } break;
    case ParamEvent::kSetTarget: {
      if (cancelled_event->Time() < cancel_time) {
        // Don't want to remove the SetTarget event if it started before the
        // cancel time, so bump the index.  But we do want to insert a
        // cancelEvent so that we stop this automation and hold the value when
        // we get there.
        ++cancelled_event_index;

        new_event = ParamEvent::CreateCancelValuesEvent(cancel_time, nullptr);
      }
    } break;
    case ParamEvent::kSetValueCurve: {
      // If the setValueCurve event started strictly before the cancel time,
      // there might be something to do....
      if (cancelled_event->Time() < cancel_time) {
        if (cancel_time >
            cancelled_event->Time() + cancelled_event->Duration()) {
          // If the cancellation time is past the end of the curve there's
          // nothing to do except remove the following events.
          ++cancelled_event_index;
        } else {
          // Cancellation time is in the middle of the curve.  Therefore,
          // create a new SetValueCurve event with the appropriate new
          // parameters to cancel this event properly.  Since it's illegal
          // to insert any event within a SetValueCurve event, we can
          // compute the new end value now instead of doing when running
          // the timeline.
          double new_duration = cancel_time - cancelled_event->Time();
          float end_value = ValueCurveAtTime(
              cancel_time, cancelled_event->Time(), cancelled_event->Duration(),
              cancelled_event->Curve().data(), cancelled_event->Curve().size());

          // Replace the existing SetValueCurve with this new one that is
          // identical except for the duration.
          new_event = ParamEvent::CreateGeneralEvent(
              event_type, cancelled_event->Value(), cancelled_event->Time(),
              cancelled_event->InitialValue(), cancelled_event->CallTime(),
              cancelled_event->TimeConstant(), new_duration,
              cancelled_event->Curve(), cancelled_event->CurvePointsPerSecond(),
              end_value, nullptr);

          new_set_value_event = ParamEvent::CreateSetValueEvent(
              end_value, cancelled_event->Time() + new_duration);
        }
      }
    } break;
    case ParamEvent::kSetValue:
    case ParamEvent::kSetValueCurveEnd:
    case ParamEvent::kCancelValues:
      // Nothing needs to be done for a SetValue or CancelValues event.
      break;
    case ParamEvent::kLastType:
      NOTREACHED();
      break;
  }

  // Now remove all the following events from the timeline.
  if (cancelled_event_index < events_.size()) {
    RemoveCancelledEvents(cancelled_event_index);
  }

  // Insert the new event, if any.
  if (new_event) {
    InsertEvent(std::move(new_event), exception_state);
    if (new_set_value_event)
      InsertEvent(std::move(new_set_value_event), exception_state);
  }
}

std::tuple<bool, float> AudioParamTimeline::ValueForContextTime(
    AudioDestinationHandler& audio_destination,
    float default_value,
    float min_value,
    float max_value) {
  {
    MutexTryLocker try_locker(events_lock_);
    if (!try_locker.Locked() || !events_.size() ||
        audio_destination.CurrentTime() < events_[0]->Time()) {
      return std::make_tuple(false, default_value);
    }
  }

  // Ask for just a single value.
  float value;
  double sample_rate = audio_destination.SampleRate();
  size_t start_frame = audio_destination.CurrentSampleFrame();
  // One parameter change per render quantum.
  double control_rate = sample_rate / audio_utilities::kRenderQuantumFrames;
  value =
      ValuesForFrameRange(start_frame, start_frame + 1, default_value, &value,
                          1, sample_rate, control_rate, min_value, max_value);

  return std::make_tuple(true, value);
}

float AudioParamTimeline::ValuesForFrameRange(size_t start_frame,
                                              size_t end_frame,
                                              float default_value,
                                              float* values,
                                              unsigned number_of_values,
                                              double sample_rate,
                                              double control_rate,
                                              float min_value,
                                              float max_value) {
  // We can't contend the lock in the realtime audio thread.
  MutexTryLocker try_locker(events_lock_);
  if (!try_locker.Locked()) {
    if (values) {
      for (unsigned i = 0; i < number_of_values; ++i)
        values[i] = default_value;
    }
    return default_value;
  }

  float last_value =
      ValuesForFrameRangeImpl(start_frame, end_frame, default_value, values,
                              number_of_values, sample_rate, control_rate);

  // Clamp the values now to the nominal range
  vector_math::Vclip(values, 1, &min_value, &max_value, values, 1,
                     number_of_values);

  return last_value;
}

float AudioParamTimeline::ValuesForFrameRangeImpl(size_t start_frame,
                                                  size_t end_frame,
                                                  float default_value,
                                                  float* values,
                                                  unsigned number_of_values,
                                                  double sample_rate,
                                                  double control_rate) {
  DCHECK(values);
  DCHECK_GE(number_of_values, 1u);

  // Return default value if there are no events matching the desired time
  // range.
  if (!events_.size() || (end_frame / sample_rate <= events_[0]->Time())) {
    FillWithDefault(values, default_value, number_of_values, 0);

    return default_value;
  }

  int number_of_events = events_.size();

  // MUST clamp event before |events_| is possibly mutated because
  // |new_events_| has raw pointers to objects in |events_|.  Clamping
  // will clear out all of these pointers before |events_| is
  // potentially modified.
  //
  // TODO(rtoy): Consider making |events_| be scoped_refptr instead of
  // unique_ptr.
  if (new_events_.size() > 0) {
    ClampNewEventsToCurrentTime(start_frame / sample_rate);
  }

  if (number_of_events > 0) {
    double current_time = start_frame / sample_rate;

    if (HandleAllEventsInThePast(current_time, sample_rate, default_value,
                                 number_of_values, values))
      return default_value;
  }

  // Maintain a running time (frame) and index for writing the values buffer.
  size_t current_frame = start_frame;
  unsigned write_index = 0;

  // If first event is after startFrame then fill initial part of values buffer
  // with defaultValue until we reach the first event time.
  std::tie(current_frame, write_index) =
      HandleFirstEvent(values, default_value, number_of_values, start_frame,
                       end_frame, sample_rate, current_frame, write_index);

  float value = default_value;

  // Go through each event and render the value buffer where the times overlap,
  // stopping when we've rendered all the requested values.
  int last_skipped_event_index = 0;
  for (int i = 0; i < number_of_events && write_index < number_of_values; ++i) {
    ParamEvent* event = events_[i].get();
    ParamEvent* next_event =
        i < number_of_events - 1 ? events_[i + 1].get() : nullptr;

    // Wait until we get a more recent event.
    if (!IsEventCurrent(event, next_event, current_frame, sample_rate)) {
      // This is not the special SetValue event case, and nextEvent is
      // in the past. We can skip processing of this event since it's
      // in past. We keep track of this event in lastSkippedEventIndex
      // to note what events we've skipped.
      last_skipped_event_index = i;
      continue;
    }

    // If there's no next event, set nextEventType to LastType to indicate that.
    ParamEvent::Type next_event_type =
        next_event ? static_cast<ParamEvent::Type>(next_event->GetType())
                   : ParamEvent::kLastType;

    ProcessSetTargetFollowedByRamp(i, event, next_event_type, current_frame,
                                   sample_rate, control_rate, value);

    float value1 = event->Value();
    double time1 = event->Time();

    float value2 = next_event ? next_event->Value() : value1;
    double time2 =
        next_event ? next_event->Time() : end_frame / sample_rate + 1;

    // Check to see if an event was cancelled.
    std::tie(value2, time2, next_event_type) =
        HandleCancelValues(event, next_event, value2, time2);

    DCHECK_GE(time2, time1);

    // |fillToEndFrame| is the exclusive upper bound of the last frame to be
    // computed for this event.  It's either the last desired frame (|endFrame|)
    // or derived from the end time of the next event (time2). We compute
    // ceil(time2*sampleRate) because fillToEndFrame is the exclusive upper
    // bound.  Consider the case where |startFrame| = 128 and time2 = 128.1
    // (assuming sampleRate = 1).  Since time2 is greater than 128, we want to
    // output a value for frame 128.  This requires that fillToEndFrame be at
    // least 129.  This is achieved by ceil(time2).
    //
    // However, time2 can be very large, so compute this carefully in the case
    // where time2 exceeds the size of a size_t.

    size_t fill_to_end_frame = end_frame;
    if (end_frame > time2 * sample_rate)
      fill_to_end_frame = static_cast<size_t>(ceil(time2 * sample_rate));

    DCHECK_GE(fill_to_end_frame, start_frame);
    unsigned fill_to_frame =
        static_cast<unsigned>(fill_to_end_frame - start_frame);
    fill_to_frame = std::min(fill_to_frame, number_of_values);

    const AutomationState current_state = {
        number_of_values,
        start_frame,
        end_frame,
        sample_rate,
        control_rate,
        fill_to_frame,
        fill_to_end_frame,
        value1,
        time1,
        value2,
        time2,
        event,
        i,
    };

    // First handle linear and exponential ramps which require looking ahead to
    // the next event.
    if (next_event_type == ParamEvent::kLinearRampToValue) {
      std::tie(current_frame, value, write_index) = ProcessLinearRamp(
          current_state, values, current_frame, value, write_index);
    } else if (next_event_type == ParamEvent::kExponentialRampToValue) {
      std::tie(current_frame, value, write_index) = ProcessExponentialRamp(
          current_state, values, current_frame, value, write_index);
    } else {
      // Handle event types not requiring looking ahead to the next event.
      switch (event->GetType()) {
        case ParamEvent::kSetValue:
        case ParamEvent::kSetValueCurveEnd:
        case ParamEvent::kLinearRampToValue: {
          current_frame = fill_to_end_frame;

          // Simply stay at a constant value.
          value = event->Value();
          write_index =
              FillWithDefault(values, value, fill_to_frame, write_index);

          break;
        }

        case ParamEvent::kCancelValues: {
          std::tie(current_frame, value, write_index) = ProcessCancelValues(
              current_state, values, current_frame, value, write_index);
          break;
        }

        case ParamEvent::kExponentialRampToValue: {
          current_frame = fill_to_end_frame;

          // If we're here, we've reached the end of the ramp.  For
          // the values after the end of the ramp, we just want to
          // continue with the ramp end value.
          value = event->Value();
          write_index =
              FillWithDefault(values, value, fill_to_frame, write_index);

          break;
        }

        case ParamEvent::kSetTarget: {
          std::tie(current_frame, value, write_index) = ProcessSetTarget(
              current_state, values, current_frame, value, write_index);
          break;
        }

        case ParamEvent::kSetValueCurve: {
          std::tie(current_frame, value, write_index) = ProcessSetValueCurve(
              current_state, values, current_frame, value, write_index);
          break;
        }
        case ParamEvent::kLastType:
          NOTREACHED();
          break;
      }
    }
  }

  // If we skipped over any events (because they are in the past), we can
  // remove them so we don't have to check them ever again.  (This MUST be
  // running with the m_events lock so we can safely modify the m_events
  // array.)
  if (last_skipped_event_index > 0) {
    // |new_events_| should be empty here so we don't have to
    // do any updates due to this mutation of |events_|.
    DCHECK_EQ(new_events_.size(), 0u);
    RemoveOldEvents(last_skipped_event_index - 1);
  }

  // If there's any time left after processing the last event then just
  // propagate the last value to the end of the values buffer.
  write_index = FillWithDefault(values, value, number_of_values, write_index);

  // This value is used to set the .value attribute of the AudioParam.  it
  // should be the last computed value.
  return values[number_of_values - 1];
}

std::tuple<size_t, unsigned> AudioParamTimeline::HandleFirstEvent(
    float* values,
    float default_value,
    unsigned number_of_values,
    size_t start_frame,
    size_t end_frame,
    double sample_rate,
    size_t current_frame,
    unsigned write_index) {
  double first_event_time = events_[0]->Time();
  if (first_event_time > start_frame / sample_rate) {
    // |fillToFrame| is an exclusive upper bound, so use ceil() to compute the
    // bound from the firstEventTime.
    size_t fill_to_end_frame = end_frame;
    double first_event_frame = ceil(first_event_time * sample_rate);
    if (end_frame > first_event_frame)
      fill_to_end_frame = first_event_frame;
    DCHECK_GE(fill_to_end_frame, start_frame);

    unsigned fill_to_frame =
        static_cast<unsigned>(fill_to_end_frame - start_frame);
    fill_to_frame = std::min(fill_to_frame, number_of_values);
    write_index =
        FillWithDefault(values, default_value, fill_to_frame, write_index);

    current_frame += fill_to_frame;
  }

  return std::make_tuple(current_frame, write_index);
}

bool AudioParamTimeline::IsEventCurrent(const ParamEvent* event,
                                        const ParamEvent* next_event,
                                        size_t current_frame,
                                        double sample_rate) const {
  // WARNING: due to round-off it might happen that nextEvent->time() is
  // just larger than currentFrame/sampleRate.  This means that we will end
  // up running the |event| again.  The code below had better be prepared
  // for this case!  What should happen is the fillToFrame should be 0 so
  // that while the event is actually run again, nothing actually gets
  // computed, and we move on to the next event.
  //
  // An example of this case is setValueCurveAtTime.  The time at which
  // setValueCurveAtTime ends (and the setValueAtTime begins) might be
  // just past currentTime/sampleRate.  Then setValueCurveAtTime will be
  // processed again before advancing to setValueAtTime.  The number of
  // frames to be processed should be zero in this case.
  if (next_event && next_event->Time() < current_frame / sample_rate) {
    // But if the current event is a SetValue event and the event time is
    // between currentFrame - 1 and curentFrame (in time). we don't want to
    // skip it.  If we do skip it, the SetValue event is completely skipped
    // and not applied, which is wrong.  Other events don't have this problem.
    // (Because currentFrame is unsigned, we do the time check in this funny,
    // but equivalent way.)
    double event_frame = event->Time() * sample_rate;

    // Condition is currentFrame - 1 < eventFrame <= currentFrame, but
    // currentFrame is unsigned and could be 0, so use
    // currentFrame < eventFrame + 1 instead.
    if (!(((event->GetType() == ParamEvent::kSetValue ||
            event->GetType() == ParamEvent::kSetValueCurveEnd) &&
           (event_frame <= current_frame) &&
           (current_frame < event_frame + 1)))) {
      // This is not the special SetValue event case, and nextEvent is
      // in the past. We can skip processing of this event since it's
      // in past.
      return false;
    }
  }
  return true;
}

void AudioParamTimeline::ClampNewEventsToCurrentTime(double current_time) {
  bool clamped_some_event_time = false;

  for (auto* event : new_events_) {
    if (event->Time() < current_time) {
      event->SetTime(current_time);
      clamped_some_event_time = true;
    }
  }

  if (clamped_some_event_time) {
    // If we clamped some event time to current time, we need to sort
    // the event list in time order again, but it must be stable!
    std::stable_sort(events_.begin(), events_.end(), ParamEvent::EventPreceeds);
  }

  new_events_.clear();
}

// Test that for a SetTarget event, the current value is close enough
// to the target value that we can consider the event to have
// converged to the target.
static bool HasSetTargetConverged(float value,
                                  float target,
                                  double current_time,
                                  double start_time,
                                  double time_constant) {
  // Converged if enough time constants (|kTimeConstantsToConverge|) have passed
  // since the start of the event.
  if (current_time > start_time + kTimeConstantsToConverge * time_constant) {
    return true;
  }

  // If |target| is 0, converged if |value| is less than |kSetTargetThreshold|.
  if (target == 0 && fabs(value) < kSetTargetThreshold) {
    return true;
  }

  // If |target| is not zero, converged if relative difference betwenn |value|
  // and |target| is small.  That is |target-value|/|target| <
  // |kSetTargetThreshold|.
  if (target != 0 && fabs(target - value) < kSetTargetThreshold * fabs(value)) {
    return true;
  }

  return false;
}

bool AudioParamTimeline::HandleAllEventsInThePast(double current_time,
                                                  double sample_rate,
                                                  float& default_value,
                                                  unsigned number_of_values,
                                                  float* values) {
  // Optimize the case where the last event is in the past.
  ParamEvent* last_event = events_[events_.size() - 1].get();
  ParamEvent::Type last_event_type = last_event->GetType();
  double last_event_time = last_event->Time();

  // If the last event is in the past and the event has ended, then we can
  // just propagate the same value.  Except for SetTarget which lasts
  // "forever".  SetValueCurve also has an explicit SetValue at the end of
  // the curve, so we don't need to worry that SetValueCurve time is a
  // start time, not an end time.
  if (last_event_time +
          1.5 * audio_utilities::kRenderQuantumFrames / sample_rate <
      current_time) {
    // If the last event is SetTarget, make sure we've converged and, that
    // we're at least 5 time constants past the start of the event.  If not, we
    // have to continue processing it.
    if (last_event_type == ParamEvent::kSetTarget) {
      if (HasSetTargetConverged(default_value, last_event->Value(),
                                current_time, last_event_time,
                                last_event->TimeConstant())) {
        // We've converged. Slam the default value with the target value.
        default_value = last_event->Value();
      } else {
        // Not converged, so give up; we can't remove this event yet.
        return false;
      }
    }

    // |events_| is being mutated.  |new_events_| better be empty because there
    // are raw pointers there.
    DCHECK_EQ(new_events_.size(), 0U);
    // The event has finished, so just copy the default value out.
    // Since all events are now also in the past, we can just remove all
    // timeline events too because |defaultValue| has the expected
    // value.
    FillWithDefault(values, default_value, number_of_values, 0);
    smoothed_value_ = default_value;
    RemoveOldEvents(events_.size());

    return true;
  }

  return false;
}

void AudioParamTimeline::ProcessSetTargetFollowedByRamp(
    int event_index,
    ParamEvent*& event,
    ParamEvent::Type next_event_type,
    size_t current_frame,
    double sample_rate,
    double control_rate,
    float& value) {
  // If the current event is SetTarget and the next event is a
  // LinearRampToValue or ExponentialRampToValue, special handling is needed.
  // In this case, the linear and exponential ramp should start at wherever
  // the SetTarget processing has reached.
  if (event->GetType() == ParamEvent::kSetTarget &&
      (next_event_type == ParamEvent::kLinearRampToValue ||
       next_event_type == ParamEvent::kExponentialRampToValue)) {
    // Replace the SetTarget with a SetValue to set the starting time and
    // value for the ramp using the current frame.  We need to update |value|
    // appropriately depending on whether the ramp has started or not.
    //
    // If SetTarget starts somewhere between currentFrame - 1 and
    // currentFrame, we directly compute the value it would have at
    // currentFrame.  If not, we update the value from the value from
    // currentFrame - 1.
    //
    // Can't use the condition currentFrame - 1 <= t0 * sampleRate <=
    // currentFrame because currentFrame is unsigned and could be 0.  Instead,
    // compute the condition this way,
    // where f = currentFrame and Fs = sampleRate:
    //
    //    f - 1 <= t0 * Fs <= f
    //    2 * f - 2 <= 2 * Fs * t0 <= 2 * f
    //    -2 <= 2 * Fs * t0 - 2 * f <= 0
    //    -1 <= 2 * Fs * t0 - 2 * f + 1 <= 1
    //     abs(2 * Fs * t0 - 2 * f + 1) <= 1
    if (fabs(2 * sample_rate * event->Time() - 2 * current_frame + 1) <= 1) {
      // SetTarget is starting somewhere between currentFrame - 1 and
      // currentFrame. Compute the value the SetTarget would have at the
      // currentFrame.
      value = event->Value() +
              (value - event->Value()) *
                  exp(-(current_frame / sample_rate - event->Time()) /
                      event->TimeConstant());
    } else {
      // SetTarget has already started.  Update |value| one frame because it's
      // the value from the previous frame.
      float discrete_time_constant =
          static_cast<float>(audio_utilities::DiscreteTimeConstantForSampleRate(
              event->TimeConstant(), control_rate));
      value += (event->Value() - value) * discrete_time_constant;
    }

    // Insert a SetValueEvent to mark the starting value and time.
    // Clear the clamp check because this doesn't need it.
    events_[event_index] =
        ParamEvent::CreateSetValueEvent(value, current_frame / sample_rate);

    // Update our pointer to the current event because we just changed it.
    event = events_[event_index].get();
  }
}

std::tuple<float, double, AudioParamTimeline::ParamEvent::Type>
AudioParamTimeline::HandleCancelValues(const ParamEvent* current_event,
                                       ParamEvent* next_event,
                                       float value2,
                                       double time2) {
  DCHECK(current_event);

  ParamEvent::Type next_event_type =
      next_event ? next_event->GetType() : ParamEvent::kLastType;

  if (next_event && next_event->GetType() == ParamEvent::kCancelValues &&
      next_event->SavedEvent()) {
    float value1 = current_event->Value();
    double time1 = current_event->Time();

    switch (current_event->GetType()) {
      case ParamEvent::kCancelValues:
      case ParamEvent::kLinearRampToValue:
      case ParamEvent::kExponentialRampToValue:
      case ParamEvent::kSetValueCurveEnd:
      case ParamEvent::kSetValue: {
        // These three events potentially establish a starting value for
        // the following event, so we need to examine the cancelled
        // event to see what to do.
        const ParamEvent* saved_event = next_event->SavedEvent();

        // Update the end time and type to pretend that we're running
        // this saved event type.
        time2 = next_event->Time();
        next_event_type = saved_event->GetType();

        if (next_event->HasDefaultCancelledValue()) {
          // We've already established a value for the cancelled
          // event, so just return it.
          value2 = next_event->Value();
        } else {
          // If the next event would have been a LinearRamp or
          // ExponentialRamp, we need to compute a new end value for
          // the event so that the curve works continues as if it were
          // not cancelled.
          switch (saved_event->GetType()) {
            case ParamEvent::kLinearRampToValue:
              value2 =
                  LinearRampAtTime(next_event->Time(), value1, time1,
                                   saved_event->Value(), saved_event->Time());
              break;
            case ParamEvent::kExponentialRampToValue:
              value2 = ExponentialRampAtTime(next_event->Time(), value1, time1,
                                             saved_event->Value(),
                                             saved_event->Time());
              break;
            case ParamEvent::kSetValueCurve:
            case ParamEvent::kSetValueCurveEnd:
            case ParamEvent::kSetValue:
            case ParamEvent::kSetTarget:
            case ParamEvent::kCancelValues:
              // These cannot be possible types for the saved event
              // because they can't be created.
              // createCancelValuesEvent doesn't allow them (SetValue,
              // SetTarget, CancelValues) or cancelScheduledValues()
              // doesn't create such an event (SetValueCurve).
              NOTREACHED();
              break;
            case ParamEvent::kLastType:
              // Illegal event type.
              NOTREACHED();
              break;
          }

          // Cache the new value so we don't keep computing it over and over.
          next_event->SetCancelledValue(value2);
        }
      } break;
      case ParamEvent::kSetValueCurve:
        // Everything needed for this was handled when cancelling was
        // done.
        break;
      case ParamEvent::kSetTarget:
        // Nothing special needs to be done for SetTarget
        // followed by CancelValues.
        break;
      case ParamEvent::kLastType:
        NOTREACHED();
        break;
    }
  }

  return std::make_tuple(value2, time2, next_event_type);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::ProcessLinearRamp(
    const AutomationState& current_state,
    float* values,
    size_t current_frame,
    float value,
    unsigned write_index) {
#if defined(ARCH_CPU_X86_FAMILY)
  auto number_of_values = current_state.number_of_values;
#endif
  auto fill_to_frame = current_state.fill_to_frame;
  auto time1 = current_state.time1;
  auto time2 = current_state.time2;
  auto value1 = current_state.value1;
  auto value2 = current_state.value2;
  auto sample_rate = current_state.sample_rate;

  double delta_time = time2 - time1;
  DCHECK_GE(delta_time, 0);
  // Since delta_time is a double, 1/delta_time can easily overflow a float.
  // Thus, if delta_time is close enough to zero (less than float min), treat it
  // as zero.
  float k =
      delta_time <= std::numeric_limits<float>::min() ? 0 : 1 / delta_time;
  const float value_delta = value2 - value1;
#if defined(ARCH_CPU_X86_FAMILY)
  if (fill_to_frame > write_index) {
    // Minimize in-loop operations. Calculate starting value and increment.
    // Next step: value += inc.
    //  value = value1 +
    //      (currentFrame/sampleRate - time1) * k * (value2 - value1);
    //  inc = 4 / sampleRate * k * (value2 - value1);
    // Resolve recursion by expanding constants to achieve a 4-step loop
    // unrolling.
    //  value = value1 +
    //    ((currentFrame/sampleRate - time1) + i * sampleFrameTimeIncr) * k
    //    * (value2 -value1), i in 0..3
    __m128 v_value =
        _mm_mul_ps(_mm_set_ps1(1 / sample_rate), _mm_set_ps(3, 2, 1, 0));
    v_value =
        _mm_add_ps(v_value, _mm_set_ps1(current_frame / sample_rate - time1));
    v_value = _mm_mul_ps(v_value, _mm_set_ps1(k * value_delta));
    v_value = _mm_add_ps(v_value, _mm_set_ps1(value1));
    __m128 v_inc = _mm_set_ps1(4 / sample_rate * k * value_delta);

    // Truncate loop steps to multiple of 4.
    unsigned fill_to_frame_trunc =
        write_index + ((fill_to_frame - write_index) / 4) * 4;
    // Compute final time.
    DCHECK_LE(fill_to_frame_trunc, number_of_values);
    current_frame += fill_to_frame_trunc - write_index;

    // Process 4 loop steps.
    for (; write_index < fill_to_frame_trunc; write_index += 4) {
      _mm_storeu_ps(values + write_index, v_value);
      v_value = _mm_add_ps(v_value, v_inc);
    }
  }
  // Update |value| with the last value computed so that the
  // .value attribute of the AudioParam gets the correct linear
  // ramp value, in case the following loop doesn't execute.
  if (write_index >= 1)
    value = values[write_index - 1];
#endif
  // Serially process remaining values.
  for (; write_index < fill_to_frame; ++write_index) {
    float x = (current_frame / sample_rate - time1) * k;
    // value = (1 - x) * value1 + x * value2;
    value = value1 + x * value_delta;
    values[write_index] = value;
    ++current_frame;
  }

  return std::make_tuple(current_frame, value, write_index);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::ProcessExponentialRamp(
    const AutomationState& current_state,
    float* values,
    size_t current_frame,
    float value,
    unsigned write_index) {
  auto fill_to_frame = current_state.fill_to_frame;
  auto time1 = current_state.time1;
  auto time2 = current_state.time2;
  auto value1 = current_state.value1;
  auto value2 = current_state.value2;
  auto sample_rate = current_state.sample_rate;

  if (value1 * value2 <= 0) {
    // It's an error if value1 and value2 have opposite signs or if one of
    // them is zero.  Handle this by propagating the previous value, and
    // making it the default.
    value = value1;

    for (; write_index < fill_to_frame; ++write_index)
      values[write_index] = value;
  } else {
    double delta_time = time2 - time1;
    double num_sample_frames = delta_time * sample_rate;
    // The value goes exponentially from value1 to value2 in a duration of
    // deltaTime seconds according to
    //
    //  v(t) = v1*(v2/v1)^((t-t1)/(t2-t1))
    //
    // Let c be currentFrame and F be the sampleRate.  Then we want to
    // sample v(t) at times t = (c + k)/F for k = 0, 1, ...:
    //
    //   v((c+k)/F) = v1*(v2/v1)^(((c/F+k/F)-t1)/(t2-t1))
    //              = v1*(v2/v1)^((c/F-t1)/(t2-t1))
    //                  *(v2/v1)^((k/F)/(t2-t1))
    //              = v1*(v2/v1)^((c/F-t1)/(t2-t1))
    //                  *[(v2/v1)^(1/(F*(t2-t1)))]^k
    //
    // Thus, this can be written as
    //
    //   v((c+k)/F) = V*m^k
    //
    // where
    //   V = v1*(v2/v1)^((c/F-t1)/(t2-t1))
    //   m = (v2/v1)^(1/(F*(t2-t1)))

    // Compute the per-sample multiplier.
    float multiplier = powf(value2 / value1, 1 / num_sample_frames);
    // Set the starting value of the exponential ramp.  Do not attempt
    // to optimize pow to powf.  See crbug.com/771306.
    value = value1 * pow(value2 / static_cast<double>(value1),
                         (current_frame / sample_rate - time1) / delta_time);
    for (; write_index < fill_to_frame; ++write_index) {
      values[write_index] = value;
      value *= multiplier;
      ++current_frame;
    }
    // |value| got updated one extra time in the above loop.  Restore it to
    // the last computed value.
    if (write_index >= 1)
      value /= multiplier;

    // Due to roundoff it's possible that value exceeds value2.  Clip value
    // to value2 if we are within 1/2 frame of time2.
    if (current_frame > time2 * sample_rate - 0.5)
      value = value2;
  }

  return std::make_tuple(current_frame, value, write_index);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::ProcessSetTarget(
    const AutomationState& current_state,
    float* values,
    size_t current_frame,
    float value,
    unsigned write_index) {
#if defined(ARCH_CPU_X86_FAMILY)
  auto number_of_values = current_state.number_of_values;
#endif
  auto fill_to_frame = current_state.fill_to_frame;
  auto time1 = current_state.time1;
  auto value1 = current_state.value1;
  auto sample_rate = current_state.sample_rate;
  auto control_rate = current_state.control_rate;
  auto fill_to_end_frame = current_state.fill_to_end_frame;
  auto* event = current_state.event;

  // Exponential approach to target value with given time constant.
  //
  //   v(t) = v2 + (v1 - v2)*exp(-(t-t1/tau))
  //
  float target = value1;
  float time_constant = event->TimeConstant();
  float discrete_time_constant =
      static_cast<float>(audio_utilities::DiscreteTimeConstantForSampleRate(
          time_constant, control_rate));

  // Set the starting value correctly.  This is only needed when the
  // current time is "equal" to the start time of this event.  This is
  // to get the sampling correct if the start time of this automation
  // isn't on a frame boundary.  Otherwise, we can just continue from
  // where we left off from the previous rendering quantum.
  {
    double ramp_start_frame = time1 * sample_rate;
    // Condition is c - 1 < r <= c where c = currentFrame and r =
    // rampStartFrame.  Compute it this way because currentFrame is
    // unsigned and could be 0.
    if (ramp_start_frame <= current_frame &&
        current_frame < ramp_start_frame + 1) {
      value = target +
              (value - target) *
                  exp(-(current_frame / sample_rate - time1) / time_constant);
    } else {
      // Otherwise, need to compute a new value bacause |value| is the
      // last computed value of SetTarget.  Time has progressed by one
      // frame, so we need to update the value for the new frame.
      value += (target - value) * discrete_time_constant;
    }
  }

  // If the value is close enough to the target, just fill in the data
  // with the target value.
  if (HasSetTargetConverged(value, target, current_frame / sample_rate, time1,
                            time_constant)) {
    current_frame += fill_to_frame - write_index;
    for (; write_index < fill_to_frame; ++write_index)
      values[write_index] = target;
  } else {
#if defined(ARCH_CPU_X86_FAMILY)
    if (fill_to_frame > write_index) {
      // Resolve recursion by expanding constants to achieve a 4-step
      // loop unrolling.
      //
      // v1 = v0 + (t - v0) * c
      // v2 = v1 + (t - v1) * c
      // v2 = v0 + (t - v0) * c + (t - (v0 + (t - v0) * c)) * c
      // v2 = v0 + (t - v0) * c + (t - v0) * c - (t - v0) * c * c
      // v2 = v0 + (t - v0) * c * (2 - c)
      // Thus c0 = c, c1 = c*(2-c). The same logic applies to c2 and c3.
      const float c0 = discrete_time_constant;
      const float c1 = c0 * (2 - c0);
      const float c2 = c0 * ((c0 - 3) * c0 + 3);
      const float c3 = c0 * (c0 * ((4 - c0) * c0 - 6) + 4);

      float delta;
      __m128 v_c = _mm_set_ps(c2, c1, c0, 0);
      __m128 v_delta, v_value, v_result;

      // Process 4 loop steps.
      unsigned fill_to_frame_trunc =
          write_index + ((fill_to_frame - write_index) / 4) * 4;
      DCHECK_LE(fill_to_frame_trunc, number_of_values);

      for (; write_index < fill_to_frame_trunc; write_index += 4) {
        delta = target - value;
        v_delta = _mm_set_ps1(delta);
        v_value = _mm_set_ps1(value);

        v_result = _mm_add_ps(v_value, _mm_mul_ps(v_delta, v_c));
        _mm_storeu_ps(values + write_index, v_result);

        // Update value for next iteration.
        value += delta * c3;
      }
    }
#endif
    // Serially process remaining values
    for (; write_index < fill_to_frame; ++write_index) {
      values[write_index] = value;
      value += (target - value) * discrete_time_constant;
    }
    // The previous loops may have updated |value| one extra time.
    // Reset it to the last computed value.
    if (write_index >= 1)
      value = values[write_index - 1];
    current_frame = fill_to_end_frame;
  }

  return std::make_tuple(current_frame, value, write_index);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::ProcessSetValueCurve(
    const AutomationState& current_state,
    float* values,
    size_t current_frame,
    float value,
    unsigned write_index) {
  auto number_of_values = current_state.number_of_values;
  auto fill_to_frame = current_state.fill_to_frame;
  auto time1 = current_state.time1;
  auto sample_rate = current_state.sample_rate;
  auto start_frame = current_state.start_frame;
  auto end_frame = current_state.end_frame;
  auto fill_to_end_frame = current_state.fill_to_end_frame;
  auto* event = current_state.event;

  const Vector<float> curve = event->Curve();
  const float* curve_data = curve.data();
  unsigned number_of_curve_points = curve.size();

  float curve_end_value = event->CurveEndValue();

  // Curve events have duration, so don't just use next event time.
  double duration = event->Duration();
  // How much to step the curve index for each frame.  This is basically
  // the term (N - 1)/Td in the specification.
  double curve_points_per_frame = event->CurvePointsPerSecond() / sample_rate;

  if (!number_of_curve_points || duration <= 0 || sample_rate <= 0) {
    // Error condition - simply propagate previous value.
    current_frame = fill_to_end_frame;
    for (; write_index < fill_to_frame; ++write_index)
      values[write_index] = value;
    return std::make_tuple(current_frame, value, write_index);
  }

  // Save old values and recalculate information based on the curve's
  // duration instead of the next event time.
  size_t next_event_fill_to_frame = fill_to_frame;

  // fillToEndFrame = min(endFrame,
  //                      ceil(sampleRate * (time1 + duration))),
  // but compute this carefully in case sampleRate*(time1 + duration) is
  // huge.  fillToEndFrame is an exclusive upper bound of the last frame
  // to be computed, so ceil is used.
  {
    double curve_end_frame = ceil(sample_rate * (time1 + duration));
    if (end_frame > curve_end_frame)
      fill_to_end_frame = static_cast<size_t>(curve_end_frame);
    else
      fill_to_end_frame = end_frame;
  }

  // |fillToFrame| can be less than |startFrame| when the end of the
  // setValueCurve automation has been reached, but the next automation
  // has not yet started. In this case, |fillToFrame| is clipped to
  // |time1|+|duration| above, but |startFrame| will keep increasing
  // (because the current time is increasing).
  fill_to_frame = (fill_to_end_frame < start_frame)
                      ? 0
                      : static_cast<unsigned>(fill_to_end_frame - start_frame);
  fill_to_frame = std::min(fill_to_frame, number_of_values);

  // Index into the curve data using a floating-point value.
  // We're scaling the number of curve points by the duration (see
  // curvePointsPerFrame).
  double curve_virtual_index = 0;
  if (time1 < current_frame / sample_rate) {
    // Index somewhere in the middle of the curve data.
    // Don't use timeToSampleFrame() since we want the exact
    // floating-point frame.
    double frame_offset = current_frame - time1 * sample_rate;
    curve_virtual_index = curve_points_per_frame * frame_offset;
  }

  // Set the default value in case fillToFrame is 0.
  value = curve_end_value;

  // Render the stretched curve data using linear interpolation.
  // Oversampled curve data can be provided if sharp discontinuities are
  // desired.
  unsigned k = 0;
#if defined(ARCH_CPU_X86_FAMILY)
  if (fill_to_frame > write_index) {
    const __m128 v_curve_virtual_index = _mm_set_ps1(curve_virtual_index);
    const __m128 v_curve_points_per_frame = _mm_set_ps1(curve_points_per_frame);
    const __m128 v_number_of_curve_points_m1 =
        _mm_set_ps1(number_of_curve_points - 1);
    const __m128 v_n1 = _mm_set_ps1(1.0f);
    const __m128 v_n4 = _mm_set_ps1(4.0f);

    __m128 v_k = _mm_set_ps(3, 2, 1, 0);
    int a_curve_index0[4];
    int a_curve_index1[4];

    // Truncate loop steps to multiple of 4
    unsigned truncated_steps = ((fill_to_frame - write_index) / 4) * 4;
    unsigned fill_to_frame_trunc = write_index + truncated_steps;
    DCHECK_LE(fill_to_frame_trunc, number_of_values);

    for (; write_index < fill_to_frame_trunc; write_index += 4) {
      // Compute current index this way to minimize round-off that would
      // have occurred by incrementing the index by curvePointsPerFrame.
      __m128 v_current_virtual_index = _mm_add_ps(
          v_curve_virtual_index, _mm_mul_ps(v_k, v_curve_points_per_frame));
      v_k = _mm_add_ps(v_k, v_n4);

      // Clamp index to the last element of the array.
      __m128i v_curve_index0 = _mm_cvttps_epi32(
          _mm_min_ps(v_current_virtual_index, v_number_of_curve_points_m1));
      __m128i v_curve_index1 =
          _mm_cvttps_epi32(_mm_min_ps(_mm_add_ps(v_current_virtual_index, v_n1),
                                      v_number_of_curve_points_m1));

      // Linearly interpolate between the two nearest curve points.
      // |delta| is clamped to 1 because currentVirtualIndex can exceed
      // curveIndex0 by more than one.  This can happen when we reached
      // the end of the curve but still need values to fill out the
      // current rendering quantum.
      _mm_storeu_si128((__m128i*)a_curve_index0, v_curve_index0);
      _mm_storeu_si128((__m128i*)a_curve_index1, v_curve_index1);
      __m128 v_c0 = _mm_set_ps(
          curve_data[a_curve_index0[3]], curve_data[a_curve_index0[2]],
          curve_data[a_curve_index0[1]], curve_data[a_curve_index0[0]]);
      __m128 v_c1 = _mm_set_ps(
          curve_data[a_curve_index1[3]], curve_data[a_curve_index1[2]],
          curve_data[a_curve_index1[1]], curve_data[a_curve_index1[0]]);
      __m128 v_delta = _mm_min_ps(
          _mm_sub_ps(v_current_virtual_index, _mm_cvtepi32_ps(v_curve_index0)),
          v_n1);

      __m128 v_value =
          _mm_add_ps(v_c0, _mm_mul_ps(_mm_sub_ps(v_c1, v_c0), v_delta));

      _mm_storeu_ps(values + write_index, v_value);
    }
    // Pass along k to the serial loop.
    k = truncated_steps;
  }
  if (write_index >= 1)
    value = values[write_index - 1];
#endif
  for (; write_index < fill_to_frame; ++write_index, ++k) {
    // Compute current index this way to minimize round-off that would
    // have occurred by incrementing the index by curvePointsPerFrame.
    double current_virtual_index =
        curve_virtual_index + k * curve_points_per_frame;
    unsigned curve_index0;

    // Clamp index to the last element of the array.
    if (current_virtual_index < number_of_curve_points) {
      curve_index0 = static_cast<unsigned>(current_virtual_index);
    } else {
      curve_index0 = number_of_curve_points - 1;
    }

    unsigned curve_index1 =
        std::min(curve_index0 + 1, number_of_curve_points - 1);

    // Linearly interpolate between the two nearest curve points.
    // |delta| is clamped to 1 because currentVirtualIndex can exceed
    // curveIndex0 by more than one.  This can happen when we reached
    // the end of the curve but still need values to fill out the
    // current rendering quantum.
    DCHECK_LT(curve_index0, number_of_curve_points);
    DCHECK_LT(curve_index1, number_of_curve_points);
    float c0 = curve_data[curve_index0];
    float c1 = curve_data[curve_index1];
    double delta = std::min(current_virtual_index - curve_index0, 1.0);

    value = c0 + (c1 - c0) * delta;

    values[write_index] = value;
  }

  // If there's any time left after the duration of this event and the
  // start of the next, then just propagate the last value of the
  // curveData. Don't modify |value| unless there is time left.
  if (write_index < next_event_fill_to_frame) {
    value = curve_end_value;
    for (; write_index < next_event_fill_to_frame; ++write_index)
      values[write_index] = value;
  }

  // Re-adjust current time
  current_frame += next_event_fill_to_frame;

  return std::make_tuple(current_frame, value, write_index);
}

std::tuple<size_t, float, unsigned> AudioParamTimeline::ProcessCancelValues(
    const AutomationState& current_state,
    float* values,
    size_t current_frame,
    float value,
    unsigned write_index) {
  auto fill_to_frame = current_state.fill_to_frame;
  auto time1 = current_state.time1;
  auto sample_rate = current_state.sample_rate;
  auto control_rate = current_state.control_rate;
  auto fill_to_end_frame = current_state.fill_to_end_frame;
  auto* event = current_state.event;
  auto event_index = current_state.event_index;

  // If the previous event was a SetTarget or ExponentialRamp
  // event, the current value is one sample behind.  Update
  // the sample value by one sample, but only at the start of
  // this CancelValues event.
  if (event->HasDefaultCancelledValue()) {
    value = event->Value();
  } else {
    double cancel_frame = time1 * sample_rate;
    if (event_index >= 1 && cancel_frame <= current_frame &&
        current_frame < cancel_frame + 1) {
      ParamEvent::Type last_event_type = events_[event_index - 1]->GetType();
      if (last_event_type == ParamEvent::kSetTarget) {
        float target = events_[event_index - 1]->Value();
        float time_constant = events_[event_index - 1]->TimeConstant();
        float discrete_time_constant = static_cast<float>(
            audio_utilities::DiscreteTimeConstantForSampleRate(time_constant,
                                                               control_rate));
        value += (target - value) * discrete_time_constant;
      }
    }
  }

  // Simply stay at the current value.
  for (; write_index < fill_to_frame; ++write_index)
    values[write_index] = value;

  current_frame = fill_to_end_frame;

  return std::make_tuple(current_frame, value, write_index);
}

uint32_t AudioParamTimeline::FillWithDefault(float* values,
                                             float default_value,
                                             uint32_t end_frame,
                                             uint32_t write_index) {
  uint32_t index = write_index;

  for (; index < end_frame; ++index)
    values[index] = default_value;

  return index;
}

void AudioParamTimeline::RemoveCancelledEvents(
    wtf_size_t first_event_to_remove) {
  // For all the events that are being removed, also remove that event
  // from |new_events_|.
  if (new_events_.size() > 0) {
    for (wtf_size_t k = first_event_to_remove; k < events_.size(); ++k) {
      new_events_.erase(events_[k].get());
    }
  }

  // Now we can remove the cancelled events from the list.
  events_.EraseAt(first_event_to_remove,
                  events_.size() - first_event_to_remove);
}

void AudioParamTimeline::RemoveOldEvents(wtf_size_t event_count) {
  wtf_size_t n_events = events_.size();
  DCHECK(event_count <= n_events);

  // Always leave at least one event in the event list!
  if (n_events > 1) {
    events_.EraseAt(0, std::min(event_count, n_events - 1));
  }
}

}  // namespace blink

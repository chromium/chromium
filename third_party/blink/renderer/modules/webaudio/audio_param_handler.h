// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_PARAM_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_PARAM_HANDLER_H_

#include <sys/types.h>

#include <atomic>
#include <tuple>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webaudio/audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_summing_junction.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/inspector_helper_mixin.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// AudioParamHandler is an actual implementation of web-exposed AudioParam
// interface. Each of AudioParam object creates and owns an AudioParamHandler,
// and it is responsible for all of AudioParam tasks. An AudioParamHandler
// object is owned by the originator AudioParam object, and some audio
// processing classes have additional references. An AudioParamHandler can
// outlive the owner AudioParam, and it never dies before the owner AudioParam
// dies.
//
// Connected to AudioNodeOutput using AudioNodeWiring.
class AudioParamHandler final : public ThreadSafeRefCounted<AudioParamHandler>,
                                public AudioSummingJunction {
 public:
  // Each AudioParam gets an identifier here.  This is mostly for instrospection
  // if warnings or other messages need to be printed. It's useful to know what
  // the AudioParam represents.  The name should include the node type and the
  // name of the AudioParam.
  enum class AudioParamType {
    kParamTypeAudioBufferSourcePlaybackRate,
    kParamTypeAudioBufferSourceDetune,
    kParamTypeBiquadFilterFrequency,
    kParamTypeBiquadFilterQ,
    kParamTypeBiquadFilterGain,
    kParamTypeBiquadFilterDetune,
    kParamTypeDelayDelayTime,
    kParamTypeDynamicsCompressorThreshold,
    kParamTypeDynamicsCompressorKnee,
    kParamTypeDynamicsCompressorRatio,
    kParamTypeDynamicsCompressorAttack,
    kParamTypeDynamicsCompressorRelease,
    kParamTypeGainGain,
    kParamTypeOscillatorFrequency,
    kParamTypeOscillatorDetune,
    kParamTypeStereoPannerPan,
    kParamTypePannerPositionX,
    kParamTypePannerPositionY,
    kParamTypePannerPositionZ,
    kParamTypePannerOrientationX,
    kParamTypePannerOrientationY,
    kParamTypePannerOrientationZ,
    kParamTypeAudioListenerPositionX,
    kParamTypeAudioListenerPositionY,
    kParamTypeAudioListenerPositionZ,
    kParamTypeAudioListenerForwardX,
    kParamTypeAudioListenerForwardY,
    kParamTypeAudioListenerForwardZ,
    kParamTypeAudioListenerUpX,
    kParamTypeAudioListenerUpY,
    kParamTypeAudioListenerUpZ,
    kParamTypeConstantSourceOffset,
    kParamTypeAudioWorklet,
  };

  // Automation rate of the AudioParam
  enum class AutomationRate {
    // a-rate
    kAudio,
    // k-rate
    kControl
  };

  // Indicates whether automation rate can be changed.
  enum class AutomationRateMode {
    // Rate can't be changed after construction
    kFixed,
    // Rate can be selected
    kVariable
  };

  static scoped_refptr<AudioParamHandler> Create(BaseAudioContext& context,
                                                 AudioParamType param_type,
                                                 double default_value,
                                                 AutomationRate rate,
                                                 AutomationRateMode rate_mode,
                                                 float min_value,
                                                 float max_value) {
    return base::AdoptRef(new AudioParamHandler(context, param_type,
                                                default_value, rate, rate_mode,
                                                min_value, max_value));
  }

  // AudioSummingJunction
  void DidUpdate() override {}

  float Value();
  void SetValue(float value);
  AutomationRate GetAutomationRate() const {
    base::AutoLock rate_locker(RateLock());
    return automation_rate_;
  }
  void SetAutomationRate(AutomationRate automation_rate) {
    base::AutoLock rate_locker(RateLock());
    automation_rate_ = automation_rate;
  }
  float DefaultValue() const { return default_value_; }
  float MinValue() const { return min_value_; }
  float MaxValue() const { return max_value_; }
  void SetValueAtTime(float value,
                      double start_time,
                      ExceptionState& exception_state);
  void LinearRampToValueAtTime(float value,
                               double end_time,
                               float initial_value,
                               double call_time,
                               ExceptionState& exception_state);
  void ExponentialRampToValueAtTime(float value,
                                    double end_time,
                                    float initial_value,
                                    double call_time,
                                    ExceptionState& exception_state);
  void SetTargetAtTime(float target,
                       double start_time,
                       double time_constant,
                       ExceptionState& exception_state);
  void SetValueCurveAtTime(const Vector<float>& curve,
                           double start_time,
                           double duration,
                           ExceptionState& exception_state);
  void CancelScheduledValues(double cancel_time,
                             ExceptionState& exception_state);
  void CancelAndHoldAtTime(double cancel_time, ExceptionState& exception_state);

  // Return a nice name for the AudioParam.
  String GetParamName() const;
  // Set the parameter name for an AudioWorklet.
  void SetCustomParamName(const String name);

  // This should be used only in audio rendering thread.
  AudioDestinationHandler& DestinationHandler() const;

  bool IsAutomationRateFixed() const {
    return rate_mode_ == AutomationRateMode::kFixed;
  }

  // Final value for k-rate parameters, otherwise use
  // calculateSampleAccurateValues() for a-rate.
  // Must be called in the audio thread.
  float FinalValue();

  // An AudioParam needs sample accurate processing if there are
  // automations scheduled or if there are connections.
  bool HasSampleAccurateValues() const;

  bool IsAudioRate() const {
    return automation_rate_ == AutomationRate::kAudio;
  }

  // Calculates parameter values starting at the context's current time.  Must
  // be called in the context's render thread.
  void CalculateSampleAccurateValues(base::span<float> values);

  float IntrinsicValue() const {
    return intrinsic_value_.load(std::memory_order_relaxed);
  }

  base::Lock& RateLock() const { return rate_lock_; }

 private:
  class ParamEvent {
   public:
    enum class Type {
      kSetValue,
      kLinearRampToValue,
      kExponentialRampToValue,
      kSetTarget,
      kSetValueCurve,
      // For cancelValuesAndHold
      kCancelValues,
      // Special marker for the end of a `kSetValueCurve` event.
      kSetValueCurveEnd,
      kLastType
    };

    static std::unique_ptr<ParamEvent> CreateLinearRampEvent(
        float value,
        double time,
        float initial_value,
        double call_time);
    static std::unique_ptr<ParamEvent> CreateExponentialRampEvent(
        float value,
        double time,
        float initial_value,
        double call_time);
    static std::unique_ptr<ParamEvent> CreateSetValueEvent(float value,
                                                           double time);
    static std::unique_ptr<ParamEvent>
    CreateSetTargetEvent(float value, double time, double time_constant);
    static std::unique_ptr<ParamEvent> CreateSetValueCurveEvent(
        const Vector<float>& curve,
        double time,
        double duration);
    static std::unique_ptr<ParamEvent> CreateSetValueCurveEndEvent(float value,
                                                                   double time);
    static std::unique_ptr<ParamEvent> CreateCancelValuesEvent(
        double time,
        std::unique_ptr<ParamEvent> saved_event);
    // Needed for creating a saved event where we want to supply all
    // the possible parameters because we're mostly copying an
    // existing event.
    static std::unique_ptr<ParamEvent> CreateGeneralEvent(
        Type,
        float value,
        double time,
        float initial_value,
        double call_time,
        double time_constant,
        double duration,
        Vector<float>& curve,
        double curve_points_per_second,
        float curve_end_value,
        std::unique_ptr<ParamEvent> saved_event);

    static bool EventPrecedes(const std::unique_ptr<ParamEvent>& a,
                              const std::unique_ptr<ParamEvent>& b) {
      return a->Time() < b->Time();
    }

    Type GetType() const { return type_; }
    float Value() const { return value_; }
    double Time() const { return time_; }
    void SetTime(double new_time) { time_ = new_time; }
    double TimeConstant() const { return time_constant_; }
    double Duration() const { return duration_; }
    const Vector<float>& Curve() const { return curve_; }
    Vector<float>& Curve() { return curve_; }
    float InitialValue() const { return initial_value_; }
    double CallTime() const { return call_time_; }

    double CurvePointsPerSecond() const { return curve_points_per_second_; }
    float CurveEndValue() const { return curve_end_value_; }

    // For CancelValues events. Not valid for any other event.
    ParamEvent* SavedEvent() const;
    bool HasDefaultCancelledValue() const;
    void SetCancelledValue(float);

   private:
    // General event
    ParamEvent(Type type,
               float value,
               double time,
               float initial_value,
               double call_time,
               double time_constant,
               double duration,
               const Vector<float>& curve,
               double curve_points_per_second,
               float curve_end_value,
               std::unique_ptr<ParamEvent> saved_event);

    // Create simplest event needing just a value and time, like
    // setValueAtTime.
    ParamEvent(Type, float value, double time);

    // Create a linear or exponential ramp that requires an initial
    // value and time in case there is no actual event that precedes
    // this event.
    ParamEvent(Type,
               float value,
               double time,
               float initial_value,
               double call_time);

    // Create an event needing a time constant (setTargetAtTime)
    ParamEvent(Type, float value, double time, double time_constant);

    // Create a setValueCurve event
    ParamEvent(Type,
               double time,
               double duration,
               const Vector<float>& curve,
               double curve_points_per_second,
               float curve_end_value);

    // Create CancelValues event
    ParamEvent(Type, double time, std::unique_ptr<ParamEvent> saved_event);

    const Type type_;

    // The value for the event.  The interpretation of this depends on
    // the event type. Not used for SetValueCurve. For CancelValues,
    // it is the end value to use when cancelling a LinearRampToValue
    // or ExponentialRampToValue event.
    float value_;

    // The time for the event. The interpretation of this depends on
    // the event type.
    double time_;

    // Initial value and time to use for linear and exponential ramps that
    // don't have a preceding event.
    const float initial_value_;
    const double call_time_;

    // Only used for SetTarget events
    const double time_constant_;

    // The following items are only used for SetValueCurve events.
    //
    // The duration of the curve.
    const double duration_;
    // The array of curve points.
    Vector<float> curve_;
    // The number of curve points per second. it is used to compute
    // the curve index step when running the automation.
    const double curve_points_per_second_;
    // The default value to use at the end of the curve.  Normally
    // it's the last entry in m_curve, but cancelling a SetValueCurve
    // will set this to a new value.
    const float curve_end_value_;

    // For CancelValues. If CancelValues is in the middle of an event, this
    // holds the event that is being cancelled, so that processing can
    // continue as if the event still existed up until we reach the actual
    // scheduled cancel time.
    const std::unique_ptr<ParamEvent> saved_event_;

    // True if a default value has been assigned to the CancelValues event.
    bool has_default_cancelled_value_;
  };

  friend class AudioNodeWiring;

  AudioParamHandler(BaseAudioContext&,
                    AudioParamType,
                    double default_value,
                    AutomationRate rate,
                    AutomationRateMode rate_mode,
                    float min,
                    float max);

  // Compute the value from this AudioParamHandler at the current context
  // frame. Returns two values:
  //
  //   bool has_value - to indicate if the value could be computed from the
  //                    timeline
  //   float value    - the timeline value if `has_value` is true; otherwise
  //                    `default_value` is returned.
  std::tuple<bool, float> ValueForContextTime(AudioDestinationHandler&,
                                              float default_value,
                                              float min_value,
                                              float max_value,
                                              unsigned render_quantum_frames);

  // Given the time range in frames, calculates parameter values into the
  // values buffer and returns the last parameter value calculated for
  // "values" or the defaultValue if none were calculated.  controlRate is
  // the rate (number per second) at which parameter values will be
  // calculated. It should equal sampleRate for sample-accurate parameter
  // changes, and otherwise will usually match the render quantum size such
  // that the parameter value changes once per render quantum.
  float ValuesForFrameRange(size_t start_frame,
                            size_t end_frame,
                            float default_value,
                            base::span<float> values,
                            double sample_rate,
                            double control_rate,
                            float min_value,
                            float max_value,
                            unsigned render_quantum_frames);

  // Returns true if the event was inserted, false if an exception occurred and
  // the event was not inserted.
  bool InsertEvent(std::unique_ptr<ParamEvent>, ExceptionState&)
      EXCLUSIVE_LOCKS_REQUIRED(events_lock_);
  float ValuesForFrameRangeImpl(const size_t start_frame,
                                const size_t end_frame,
                                float default_value,
                                base::span<float> values,
                                const double sample_rate,
                                const double control_rate,
                                unsigned render_quantum_frames)
      EXCLUSIVE_LOCKS_REQUIRED(events_lock_);

  // Produce a nice string describing the event in human-readable form.
  String EventToString(const ParamEvent&) const;

  // Handles the special case where the first event in the timeline
  // starts after `start_frame`.  These initial values are filled using
  // `default_value`.  The updated `current_frame` and `write_index` is
  // returned.
  std::tuple<size_t, unsigned> HandleFirstEvent(base::span<float> values,
                                                float default_value,
                                                size_t start_frame,
                                                size_t end_frame,
                                                double sample_rate,
                                                size_t current_frame,
                                                unsigned write_index)
      EXCLUSIVE_LOCKS_REQUIRED(events_lock_);

  // Return true if `current_event` starts after `current_frame`, but
  // also takes into account the `next_event` if any.
  bool IsEventCurrent(const ParamEvent* current_event,
                      const ParamEvent* next_event,
                      size_t current_frame,
                      double sample_rate) const;

  // Clamp times to current time, if needed for any new events.  Note,
  // this method can mutate `events_`, so do call this only in safe
  // places.
  void ClampNewEventsToCurrentTime(double current_time)
      EXCLUSIVE_LOCKS_REQUIRED(events_lock_);

  // Handle the case where the last event in the timeline is in the
  // past.  Returns false if any event is not in the past. Otherwise,
  // return true and also fill in `values` with `default_value`.
  // `default_value` may be updated with a new value.
  bool HandleAllEventsInThePast(double current_time,
                                double sample_rate,
                                float& default_value,
                                base::span<float> values,
                                unsigned render_quantum_frames)
      EXCLUSIVE_LOCKS_REQUIRED(events_lock_);

  // Handle processing of CancelValue event. If cancellation happens,
  // value2, time2, and nextEventType will be updated with the new value due
  // to cancellation.  Note that `next_event` or its member can be null.
  std::tuple<float, double, ParamEvent::Type> HandleCancelValues(
      const ParamEvent* current_event,
      ParamEvent* next_event,
      float value2,
      double time2) EXCLUSIVE_LOCKS_REQUIRED(events_lock_);

  // Process a SetTarget event and the next event is a
  // LinearRampToValue or ExponentialRampToValue event.  This requires
  // special handling because the ramp should start at whatever value
  // the SetTarget event has reached at this time, instead of using
  // the value of the SetTarget event.
  void ProcessSetTargetFollowedByRamp(int event_index,
                                      ParamEvent*& current_event,
                                      ParamEvent::Type next_event_type,
                                      size_t current_frame,
                                      double sample_rate,
                                      double control_rate,
                                      float& value)
      EXCLUSIVE_LOCKS_REQUIRED(events_lock_);

  // Handle processing of LinearRampEvent, writing the appropriate
  // values to `values`.  Returns the updated `current_frame`, last
  // computed `value`, and the updated `write_index`.
  std::tuple<size_t, float, unsigned> ProcessLinearRamp(
      const size_t fill_to_frame,
      const double time1,
      const double time2,
      const float value1,
      const float value2,
      const double sample_rate,
      base::span<float> values,
      size_t current_frame,
      float value,
      unsigned write_index);

  // Handle processing of ExponentialRampEvent, writing the appropriate
  // values to `values`.  Returns the updated `current_frame`, last
  // computed `value`, and the updated `write_index`.
  std::tuple<size_t, float, unsigned> ProcessExponentialRamp(
      const size_t fill_to_frame,
      const double time1,
      const double time2,
      const float value1,
      const float value2,
      const double sample_rate,
      base::span<float> values,
      size_t current_frame,
      float value,
      unsigned write_index);

  // Handle processing of SetTargetEvent, writing the appropriate
  // values to `values`.  Returns the updated `current_frame`, last
  // computed `value`, and the updated `write_index`.
  std::tuple<size_t, float, unsigned> ProcessSetTarget(
      const size_t fill_to_frame,
      const double time1,
      const float value1,
      const double sample_rate,
      const double control_rate,
      const size_t fill_to_end_frame,
      const ParamEvent* const event,
      base::span<float> values,
      size_t current_frame,
      float value,
      unsigned write_index);

  // Handle processing of SetValueCurveEvent, writing the appropriate
  // values to `values`.  Returns the updated `current_frame`, last
  // computed `value`, and the updated `write_index`.
  std::tuple<size_t, float, unsigned> ProcessSetValueCurve(
      size_t fill_to_frame,
      const double time1,
      const double sample_rate,
      const size_t start_frame,
      const size_t end_frame,
      size_t fill_to_end_frame,
      const ParamEvent* const event,
      base::span<float> values,
      size_t current_frame,
      float value,
      unsigned write_index);

  // Handle processing of CancelValuesEvent, writing the appropriate
  // values to `values`.  Returns the updated `current_frame`, last
  // computed `value`, and the updated `write_index`.
  std::tuple<size_t, float, unsigned> ProcessCancelValues(
      const size_t fill_to_frame,
      const double time1,
      const double sample_rate,
      const double control_rate,
      const size_t fill_to_end_frame,
      const ParamEvent* const event,
      const int event_index,
      base::span<float> values,
      size_t current_frame,
      float value,
      unsigned write_index) EXCLUSIVE_LOCKS_REQUIRED(events_lock_);

  // Fill the output vector `values` with the value `default_value`,
  // starting at `write_index` and continuing up to `end_frame`
  // (exclusive).  `write_index` is updated with the new index.
  uint32_t FillWithDefault(float* values,
                           float default_value,
                           uint32_t end_frame,
                           uint32_t write_index);

  // When cancelling events, remove the items from `events_` starting
  // at the given index.
  void RemoveCancelledEvents(wtf_size_t first_event_to_remove)
      EXCLUSIVE_LOCKS_REQUIRED(events_lock_);

  // Remove old events, but always leave at least one event in the timeline.
  // This is needed in case a new event is added (like linearRamp) that
  // would use a previous event to compute the automation.
  void RemoveOldEvents(wtf_size_t n_events)
      EXCLUSIVE_LOCKS_REQUIRED(events_lock_);

  // sampleAccurate corresponds to a-rate (audio rate) vs. k-rate in the Web
  // Audio specification.
  void CalculateFinalValues(base::span<float> values, bool sample_accurate);
  void CalculateTimelineValues(base::span<float> values);

  // Returns time clamped to current time, if needed for any new events.
  double ClampedToCurrentTime(double time);

  // The type of AudioParam, indicating what this AudioParam represents and
  // what node it belongs to.  Mostly for informational purposes and doesn't
  // affect implementation.
  const AudioParamType param_type_;
  // Name of the AudioParam. This is only used for printing out more
  // informative warnings, and only used for AudioWorklets.  All others have a
  // name derived from the `param_type_`.  Worklets need custom names because
  // they're defined by the user.
  String custom_param_name_;

  std::atomic<float> intrinsic_value_;

  const float default_value_;

  // Protects `automation_rate_`.
  mutable base::Lock rate_lock_;

  // The automation rate of the AudioParam (k-rate or a-rate)
  AutomationRate automation_rate_;

  // `rate_mode_` determines if the user can change the automation rate to a
  // different value.
  const AutomationRateMode rate_mode_;

  // Nominal range for the value
  const float min_value_;
  const float max_value_;

  // Vector of all automation events for the AudioParam.
  Vector<std::unique_ptr<ParamEvent>> events_ GUARDED_BY(events_lock_);

  // Vector of raw pointers to the actual ParamEvent that was
  // inserted.  As new events are added, `new_events_` is updated with
  // the new event.  When the timline is processed, these events are
  // clamped to current time by `ClampNewEventsToCurrentTime`. Access
  // must be locked via `events_lock_`.  Must be maintained together
  // with `events_`.
  HashSet<ParamEvent*> new_events_ GUARDED_BY(events_lock_);

  mutable base::Lock events_lock_;

  // The destination node used to get necessary information like the sample
  // rate and context time.
  scoped_refptr<AudioDestinationHandler> destination_handler_;

  // Audio bus to sum in any connections to the AudioParam.
  scoped_refptr<AudioBus> summing_bus_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_PARAM_HANDLER_H_

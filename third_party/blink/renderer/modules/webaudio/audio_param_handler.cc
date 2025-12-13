// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_param_handler.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/fdlibm/ieee754.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <emmintrin.h>
#include <xmmintrin.h>
#elif defined(CPU_ARM_NEON)
#include <arm_neon.h>
#endif

namespace blink {

namespace {

// For a SetTarget event, we want the event to terminate eventually so that we
// can stop using the timeline to compute the values.  See
// `HasSetTargetConverged()` for the algorithm.  `kSetTargetThreshold` is
// exp(-`kTimeConstantsToConverge`).
constexpr float kTimeConstantsToConverge = 10.0f;
constexpr float kSetTargetThreshold = 4.539992976248485e-05f;

// Replace NaN values in `values` with `default_value`.
void HandleNaNValues(base::span<float> values, float default_value) {
  unsigned k = 0;
#if defined(ARCH_CPU_X86_FAMILY)
  if (values.size() >= 4) {
    __m128 defaults = _mm_set1_ps(default_value);
    for (k = 0; k < values.size(); k += 4) {
      // SAFETY: The for loop condition has been checked k < values.size().
      __m128 v = _mm_loadu_ps(UNSAFE_BUFFERS(values.data() + k));
      // cmpuord returns all 1's if v is NaN for each elmeent of v.
      __m128 isnan = _mm_cmpunord_ps(v, v);
      // Replace NaN parts with default.
      __m128 result = _mm_and_ps(isnan, defaults);
      // Merge in the parts that aren't NaN
      result = _mm_or_ps(_mm_andnot_ps(isnan, v), result);
      // SAFETY: The for loop condition has been checked k < values.size().
      _mm_storeu_ps(UNSAFE_BUFFERS(values.data() + k), result);
    }
  }
#elif defined(CPU_ARM_NEON)
  if (values.size() >= 4) {
    uint32x4_t defaults =
        reinterpret_cast<uint32x4_t>(vdupq_n_f32(default_value));
    for (k = 0; k < values.size(); k += 4) {
      // SAFETY: The for loop condition has been checked k < values.size().
      float32x4_t v = vld1q_f32(UNSAFE_BUFFERS(values.data() + k));
      // Returns true (all ones) if v is not NaN
      uint32x4_t is_not_nan = vceqq_f32(v, v);
      // Get the parts that are not NaN
      uint32x4_t result =
          vandq_u32(is_not_nan, reinterpret_cast<uint32x4_t>(v));
      // Replace the parts that are NaN with the default and merge with previous
      // result.  (Note: vbic_u32(x, y) = x and not y)
      result = vorrq_u32(result, vbicq_u32(defaults, is_not_nan));
      // SAFETY: The for loop condition has been checked k < values.size().
      vst1q_f32(UNSAFE_BUFFERS(values.data() + k),
                reinterpret_cast<float32x4_t>(result));
    }
  }
#endif

  std::ranges::replace_if(
      values.subspan(k), [](float value) { return std::isnan(value); },
      default_value);
}

bool IsNonNegativeAudioParamTime(double time,
                                 ExceptionState& exception_state,
                                 String message = "Time") {
  if (time >= 0) {
    return true;
  }

  exception_state.ThrowRangeError(StrCat(
      {message,
       " must be a finite non-negative number: ", String::Number(time)}));
  return false;
}

bool IsPositiveAudioParamTime(double time,
                              ExceptionState& exception_state,
                              String message) {
  if (time > 0) {
    return true;
  }

  exception_state.ThrowRangeError(StrCat(
      {message, " must be a finite positive number: ", String::Number(time)}));
  return false;
}

// Test that for a SetTarget event, the current value is close enough
// to the target value that we can consider the event to have
// converged to the target.
bool HasSetTargetConverged(float value,
                           float target,
                           double current_time,
                           double start_time,
                           double time_constant) {
  // Converged if enough time constants (`kTimeConstantsToConverge`) have passed
  // since the start of the event.
  if (current_time > start_time + kTimeConstantsToConverge * time_constant) {
    return true;
  }

  // If `target` is 0, converged if |`value`| is less than
  // `kSetTargetThreshold`.
  if (target == 0 && fabs(value) < kSetTargetThreshold) {
    return true;
  }

  // If `target` is not zero, converged if relative difference between `value`
  // and `target` is small.  That is |`target`-`value`|/|`value`| <
  // `kSetTargetThreshold`.
  if (target != 0 && fabs(target - value) < kSetTargetThreshold * fabs(value)) {
    return true;
  }

  return false;
}

// Computes the value of a linear ramp event at time t with the given event
// parameters.
float LinearRampAtTime(double t,
                       float value1,
                       double time1,
                       float value2,
                       double time2) {
  return value1 + (value2 - value1) * (t - time1) / (time2 - time1);
}

// Computes the value of an exponential ramp event at time t with the given
// event parameters.
float ExponentialRampAtTime(double t,
                            float value1,
                            double time1,
                            float value2,
                            double time2) {
  DCHECK(!std::isnan(value1) && std::isfinite(value1));
  DCHECK(!std::isnan(value2) && std::isfinite(value2));

  return (value1 == 0.0f || std::signbit(value1) != std::signbit(value2))
             ? value1
             : value1 *
                   fdlibm::pow(value2 / value1, (t - time1) / (time2 - time1));
}

// Compute the value of a set curve event at time t with the given event
// parameters.
float ValueCurveAtTime(double t,
                       double time1,
                       double duration,
                       base::span<const float> curve_data) {
  double curve_index = (curve_data.size() - 1) / duration * (t - time1);
  size_t k = std::min(static_cast<size_t>(curve_index), curve_data.size() - 1);
  size_t k1 = std::min(k + 1, curve_data.size() - 1);
  float c0 = curve_data[k];
  float c1 = curve_data[k1];
  float delta = std::min(curve_index - k, 1.0);

  return c0 + (c1 - c0) * delta;
}

}  // namespace

AudioParamHandler::AudioParamHandler(BaseAudioContext& context,
                                     AudioParamType param_type,
                                     double default_value,
                                     AutomationRate rate,
                                     AutomationRateMode rate_mode,
                                     float min_value,
                                     float max_value)
    : AudioSummingJunction(context.GetDeferredTaskHandler()),
      param_type_(param_type),
      intrinsic_value_(default_value),
      default_value_(default_value),
      automation_rate_(rate),
      rate_mode_(rate_mode),
      min_value_(min_value),
      max_value_(max_value),
      summing_bus_(AudioBus::Create(1, RenderQuantumFrames(), false)) {
  // An AudioParam needs the destination handler to run the timeline.  But the
  // destination may have been destroyed (e.g. page gone), so the destination is
  // null.  However, if the destination is gone, the AudioParam will never get
  // pulled, so this is ok.  We have checks for the destination handler existing
  // when the AudioParam want to use it.
  if (context.destination()) {
    destination_handler_ = &context.destination()->GetAudioDestinationHandler();
  }
}

AudioDestinationHandler& AudioParamHandler::DestinationHandler() const {
  CHECK(destination_handler_);
  return *destination_handler_;
}

void AudioParamHandler::SetCustomParamName(const String name) {
  DCHECK(param_type_ == AudioParamType::kParamTypeAudioWorklet);
  custom_param_name_ = name;
}

String AudioParamHandler::GetParamName() const {
  switch (param_type_) {
    case AudioParamType::kParamTypeAudioBufferSourcePlaybackRate:
      return "AudioBufferSource.playbackRate";
    case AudioParamType::kParamTypeAudioBufferSourceDetune:
      return "AudioBufferSource.detune";
    case AudioParamType::kParamTypeBiquadFilterFrequency:
      return "BiquadFilter.frequency";
    case AudioParamType::kParamTypeBiquadFilterQ:
      return "BiquadFilter.Q";
    case AudioParamType::kParamTypeBiquadFilterGain:
      return "BiquadFilter.gain";
    case AudioParamType::kParamTypeBiquadFilterDetune:
      return "BiquadFilter.detune";
    case AudioParamType::kParamTypeDelayDelayTime:
      return "Delay.delayTime";
    case AudioParamType::kParamTypeDynamicsCompressorThreshold:
      return "DynamicsCompressor.threshold";
    case AudioParamType::kParamTypeDynamicsCompressorKnee:
      return "DynamicsCompressor.knee";
    case AudioParamType::kParamTypeDynamicsCompressorRatio:
      return "DynamicsCompressor.ratio";
    case AudioParamType::kParamTypeDynamicsCompressorAttack:
      return "DynamicsCompressor.attack";
    case AudioParamType::kParamTypeDynamicsCompressorRelease:
      return "DynamicsCompressor.release";
    case AudioParamType::kParamTypeGainGain:
      return "Gain.gain";
    case AudioParamType::kParamTypeOscillatorFrequency:
      return "Oscillator.frequency";
    case AudioParamType::kParamTypeOscillatorDetune:
      return "Oscillator.detune";
    case AudioParamType::kParamTypeStereoPannerPan:
      return "StereoPanner.pan";
    case AudioParamType::kParamTypePannerPositionX:
      return "Panner.positionX";
    case AudioParamType::kParamTypePannerPositionY:
      return "Panner.positionY";
    case AudioParamType::kParamTypePannerPositionZ:
      return "Panner.positionZ";
    case AudioParamType::kParamTypePannerOrientationX:
      return "Panner.orientationX";
    case AudioParamType::kParamTypePannerOrientationY:
      return "Panner.orientationY";
    case AudioParamType::kParamTypePannerOrientationZ:
      return "Panner.orientationZ";
    case AudioParamType::kParamTypeAudioListenerPositionX:
      return "AudioListener.positionX";
    case AudioParamType::kParamTypeAudioListenerPositionY:
      return "AudioListener.positionY";
    case AudioParamType::kParamTypeAudioListenerPositionZ:
      return "AudioListener.positionZ";
    case AudioParamType::kParamTypeAudioListenerForwardX:
      return "AudioListener.forwardX";
    case AudioParamType::kParamTypeAudioListenerForwardY:
      return "AudioListener.forwardY";
    case AudioParamType::kParamTypeAudioListenerForwardZ:
      return "AudioListener.forwardZ";
    case AudioParamType::kParamTypeAudioListenerUpX:
      return "AudioListener.upX";
    case AudioParamType::kParamTypeAudioListenerUpY:
      return "AudioListener.upY";
    case AudioParamType::kParamTypeAudioListenerUpZ:
      return "AudioListener.upZ";
    case AudioParamType::kParamTypeConstantSourceOffset:
      return "ConstantSource.offset";
    case AudioParamType::kParamTypeAudioWorklet:
      return custom_param_name_;
    default:
      NOTREACHED();
  }
}

float AudioParamHandler::Value() {
  // Update value for timeline.
  float v = IntrinsicValue();
  if (IsAudioThread()) {
    auto [has_value, timeline_value] = ValueForContextTime(
        DestinationHandler(), v, MinValue(), MaxValue(), RenderQuantumFrames());

    if (has_value) {
      v = timeline_value;
    }
  }

  SetValue(v);
  return v;
}

void AudioParamHandler::SetValue(float value) {
  value = ClampTo(value, min_value_, max_value_);
  intrinsic_value_.store(value, std::memory_order_relaxed);
}

float AudioParamHandler::FinalValue() {
  float value = IntrinsicValue();
  CalculateFinalValues(base::span_from_ref(value), false);
  return value;
}

void AudioParamHandler::CalculateSampleAccurateValues(
    base::span<float> values) {
  DCHECK(IsAudioThread());
  DCHECK(!values.empty());

  CalculateFinalValues(values, IsAudioRate());
}

void AudioParamHandler::CalculateFinalValues(base::span<float> values,
                                             bool sample_accurate) {
  DCHECK(IsAudioThread());
  DCHECK(!values.empty());

  // The calculated result will be the "intrinsic" value summed with all
  // audio-rate connections.

  if (sample_accurate) {
    // Calculate sample-accurate (a-rate) intrinsic values.
    CalculateTimelineValues(values);
  } else {
    // Calculate control-rate (k-rate) intrinsic value.
    float value = IntrinsicValue();
    auto [has_value, timeline_value] =
        ValueForContextTime(DestinationHandler(), value, MinValue(), MaxValue(),
                            RenderQuantumFrames());

    if (has_value) {
      value = timeline_value;
    }

    std::ranges::fill(values, value);
    SetValue(value);
  }

  // If there are any connections, sum all of the audio-rate connections
  // together (unity-gain summing junction).  Note that connections would
  // normally be mono, but we mix down to mono if necessary.
  if (NumberOfRenderingConnections() > 0) {
    DCHECK_LE(values.size(), RenderQuantumFrames());

    // If we're not sample accurate, we only need one value, so make the summing
    // bus have length 1.  When the connections are added in, only the first
    // value will be added.  Which is exactly what we want.
    summing_bus_->SetChannelMemory(0, values.data(),
                                   sample_accurate ? values.size() : 1);

    for (unsigned i = 0; i < NumberOfRenderingConnections(); ++i) {
      AudioNodeOutput* output = RenderingOutput(i);
      DCHECK(output);

      // Render audio from this output.
      AudioBus* connection_bus = output->Pull(nullptr, RenderQuantumFrames());

      // Sum, with unity-gain.
      summing_bus_->SumFrom(*connection_bus);
    }

    // If we're not sample accurate, duplicate the first element of `values` to
    // all of the elements.
    if (!sample_accurate) {
      std::ranges::fill(values, values[0]);
    }

    float min_value = MinValue();
    float max_value = MaxValue();

    if (NumberOfRenderingConnections() > 0) {
      // AudioParams by themselves don't produce NaN because of the finite min
      // and max values.  But an input to an AudioParam could have NaNs.
      //
      // NaN values in AudioParams must be replaced by the AudioParam's
      // defaultValue.  Then these values must be clamped to lie in the nominal
      // range between the AudioParam's minValue and maxValue.
      //
      // See https://webaudio.github.io/web-audio-api/#computation-of-value.
      HandleNaNValues(values, DefaultValue());
    }

    vector_math::Vclip(values, 1, &min_value, &max_value, values, 1);
  }
}

void AudioParamHandler::CalculateTimelineValues(base::span<float> values) {
  // Calculate values for this render quantum.  Normally
  // `number_of_values` will equal to
  // RenderQuantumFrames() (the render quantum size).
  double sample_rate = DestinationHandler().SampleRate();
  size_t start_frame = DestinationHandler().CurrentSampleFrame();
  size_t end_frame = start_frame + values.size();

  // Note we're running control rate at the sample-rate.
  // Pass in the current value as default value.
  SetValue(ValuesForFrameRange(start_frame, end_frame, IntrinsicValue(), values,
                               sample_rate, sample_rate, MinValue(), MaxValue(),
                               RenderQuantumFrames()));
}

double AudioParamHandler::ClampedToCurrentTime(double time) {
  return std::max(time, DestinationHandler().CurrentTime());
}

String AudioParamHandler::EventToString(const ParamEvent& event) const {
  // The default arguments for most automation methods is the value and the
  // time.
  String args = StrCat(
      {String::Number(event.Value()), ", ", String::Number(event.Time(), 16)});

  // Get a nice printable name for the event and update the args if necessary.
  String s;
  switch (event.GetType()) {
    case ParamEvent::Type::kSetValue:
      s = "setValueAtTime";
      break;
    case ParamEvent::Type::kLinearRampToValue:
      s = "linearRampToValueAtTime";
      break;
    case ParamEvent::Type::kExponentialRampToValue:
      s = "exponentialRampToValue";
      break;
    case ParamEvent::Type::kSetTarget:
      s = "setTargetAtTime";
      // This has an extra time constant arg
      args = StrCat({args, ", ", String::Number(event.TimeConstant(), 16)});
      break;
    case ParamEvent::Type::kSetValueCurve:
      s = "setValueCurveAtTime";
      // Replace the default arg, using "..." to denote the curve argument.
      args = StrCat({"..., ", String::Number(event.Time(), 16), ", ",
                     String::Number(event.Duration(), 16)});
      break;
    case ParamEvent::Type::kCancelValues:
    case ParamEvent::Type::kSetValueCurveEnd:
    // Fall through; we should never have to print out the internal
    // `kCancelValues` or `kSetValueCurveEnd` event.
    case ParamEvent::Type::kLastType:
      NOTREACHED();
  };

  return StrCat({s, "(", args, ")"});
}

std::unique_ptr<AudioParamHandler::ParamEvent>
AudioParamHandler::ParamEvent::CreateSetValueEvent(float value, double time) {
  return base::WrapUnique(
      new ParamEvent(ParamEvent::Type::kSetValue, value, time));
}

std::unique_ptr<AudioParamHandler::ParamEvent>
AudioParamHandler::ParamEvent::CreateLinearRampEvent(float value,
                                                     double time,
                                                     float initial_value,
                                                     double call_time) {
  return base::WrapUnique(new ParamEvent(ParamEvent::Type::kLinearRampToValue,
                                         value, time, initial_value,
                                         call_time));
}

std::unique_ptr<AudioParamHandler::ParamEvent>
AudioParamHandler::ParamEvent::CreateExponentialRampEvent(float value,
                                                          double time,
                                                          float initial_value,
                                                          double call_time) {
  return base::WrapUnique(
      new ParamEvent(ParamEvent::Type::kExponentialRampToValue, value, time,
                     initial_value, call_time));
}

std::unique_ptr<AudioParamHandler::ParamEvent>
AudioParamHandler::ParamEvent::CreateSetTargetEvent(float value,
                                                    double time,
                                                    double time_constant) {
  // The time line code does not expect a timeConstant of 0. (IT
  // returns NaN or Infinity due to division by zero.  The caller
  // should have converted this to a SetValueEvent.
  DCHECK_NE(time_constant, 0);
  return base::WrapUnique(
      new ParamEvent(ParamEvent::Type::kSetTarget, value, time, time_constant));
}

std::unique_ptr<AudioParamHandler::ParamEvent>
AudioParamHandler::ParamEvent::CreateSetValueCurveEvent(
    const Vector<float>& curve,
    double time,
    double duration) {
  double curve_points = (curve.size() - 1) / duration;
  float end_value = curve.back();

  return base::WrapUnique(new ParamEvent(ParamEvent::Type::kSetValueCurve, time,
                                         duration, curve, curve_points,
                                         end_value));
}

std::unique_ptr<AudioParamHandler::ParamEvent>
AudioParamHandler::ParamEvent::CreateSetValueCurveEndEvent(float value,
                                                           double time) {
  return base::WrapUnique(
      new ParamEvent(ParamEvent::Type::kSetValueCurveEnd, value, time));
}

std::unique_ptr<AudioParamHandler::ParamEvent>
AudioParamHandler::ParamEvent::CreateCancelValuesEvent(
    double time,
    std::unique_ptr<ParamEvent> saved_event) {
  if (saved_event) {
    // The savedEvent can only have certain event types.  Verify that.
    ParamEvent::Type saved_type = saved_event->GetType();

    DCHECK_NE(saved_type, ParamEvent::Type::kLastType);
    DCHECK(saved_type == ParamEvent::Type::kLinearRampToValue ||
           saved_type == ParamEvent::Type::kExponentialRampToValue ||
           saved_type == ParamEvent::Type::kSetValueCurve);
  }

  return base::WrapUnique(new ParamEvent(ParamEvent::Type::kCancelValues, time,
                                         std::move(saved_event)));
}

std::unique_ptr<AudioParamHandler::ParamEvent>
AudioParamHandler::ParamEvent::CreateGeneralEvent(
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

AudioParamHandler::ParamEvent* AudioParamHandler::ParamEvent::SavedEvent()
    const {
  DCHECK_EQ(GetType(), ParamEvent::Type::kCancelValues);
  return saved_event_.get();
}

bool AudioParamHandler::ParamEvent::HasDefaultCancelledValue() const {
  DCHECK_EQ(GetType(), ParamEvent::Type::kCancelValues);
  return has_default_cancelled_value_;
}

void AudioParamHandler::ParamEvent::SetCancelledValue(float value) {
  DCHECK_EQ(GetType(), ParamEvent::Type::kCancelValues);
  value_ = value;
  has_default_cancelled_value_ = true;
}

// General event
AudioParamHandler::ParamEvent::ParamEvent(
    ParamEvent::Type type,
    float value,
    double time,
    float initial_value,
    double call_time,
    double time_constant,
    double duration,
    const Vector<float>& curve,
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
AudioParamHandler::ParamEvent::ParamEvent(ParamEvent::Type type,
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
  DCHECK(type == ParamEvent::Type::kSetValue ||
         type == ParamEvent::Type::kSetValueCurveEnd);
}

// Create a linear or exponential ramp that requires an initial value and
// time in case
// there is no actual event that precedes this event.
AudioParamHandler::ParamEvent::ParamEvent(ParamEvent::Type type,
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
  DCHECK(type == ParamEvent::Type::kLinearRampToValue ||
         type == ParamEvent::Type::kExponentialRampToValue);
}

// Create an event needing a time constant (setTargetAtTime)
AudioParamHandler::ParamEvent::ParamEvent(ParamEvent::Type type,
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
  DCHECK_EQ(type, ParamEvent::Type::kSetTarget);
}

// Create a setValueCurve event
AudioParamHandler::ParamEvent::ParamEvent(ParamEvent::Type type,
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
  DCHECK_EQ(type, ParamEvent::Type::kSetValueCurve);
  unsigned curve_length = curve.size();
  curve_.resize(curve_length);
  base::span(curve_).copy_from(curve);
}

// Create CancelValues event
AudioParamHandler::ParamEvent::ParamEvent(
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
  DCHECK_EQ(type, ParamEvent::Type::kCancelValues);
}

void AudioParamHandler::SetValueAtTime(float value,
                                       double start_time,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(start_time, exception_state)) {
    return;
  }

  start_time = ClampedToCurrentTime(start_time);

  base::AutoLock locker(events_lock_);
  InsertEvent(ParamEvent::CreateSetValueEvent(value, start_time),
              exception_state);
}

void AudioParamHandler::LinearRampToValueAtTime(
    float value,
    double end_time,
    float initial_value,
    double call_time,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(end_time, exception_state)) {
    return;
  }

  end_time = ClampedToCurrentTime(end_time);

  base::AutoLock locker(events_lock_);
  InsertEvent(ParamEvent::CreateLinearRampEvent(value, end_time, initial_value,
                                                call_time),
              exception_state);
}

void AudioParamHandler::ExponentialRampToValueAtTime(
    float value,
    double end_time,
    float initial_value,
    double call_time,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(end_time, exception_state)) {
    return;
  }

  if (!value) {
    exception_state.ThrowRangeError(StrCat(
        {"The float target value provided (", String::Number(value),
         ") should not be in the range (",
         String::Number(-std::numeric_limits<float>::denorm_min()), ", ",
         String::Number(std::numeric_limits<float>::denorm_min()), ")."}));
    return;
  }

  end_time = ClampedToCurrentTime(end_time);

  base::AutoLock locker(events_lock_);
  InsertEvent(ParamEvent::CreateExponentialRampEvent(value, end_time,
                                                     initial_value, call_time),
              exception_state);
}

void AudioParamHandler::SetTargetAtTime(float target,
                                        double start_time,
                                        double time_constant,
                                        ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(start_time, exception_state) ||
      !IsNonNegativeAudioParamTime(time_constant, exception_state,
                                   "Time constant")) {
    return;
  }

  start_time = ClampedToCurrentTime(start_time);

  base::AutoLock locker(events_lock_);

  // If timeConstant = 0, we instantly jump to the target value, so
  // insert a SetValueEvent instead of SetTargetEvent.
  if (time_constant == 0) {
    InsertEvent(ParamEvent::CreateSetValueEvent(target, start_time),
                exception_state);
  } else {
    InsertEvent(
        ParamEvent::CreateSetTargetEvent(target, start_time, time_constant),
        exception_state);
  }
}

void AudioParamHandler::SetValueCurveAtTime(const Vector<float>& curve,
                                            double start_time,
                                            double duration,
                                            ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(start_time, exception_state) ||
      !IsPositiveAudioParamTime(duration, exception_state, "Duration")) {
    return;
  }

  if (curve.size() < 2) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        ExceptionMessages::IndexExceedsMinimumBound("curve length",
                                                    curve.size(), 2u));
    return;
  }

  start_time = ClampedToCurrentTime(start_time);

  base::AutoLock locker(events_lock_);
  bool result = InsertEvent(
      ParamEvent::CreateSetValueCurveEvent(curve, start_time, duration),
      exception_state);
  // `InsertEvent` will have already thrown an exception for us if `result` is
  // false.
  if (!result) {
    return;
  }
  // Insert a setValueAtTime event too to establish an event so that all
  // following events will process from the end of the curve instead of the
  // beginning.
  InsertEvent(ParamEvent::CreateSetValueCurveEndEvent(curve.back(),
                                                      start_time + duration),
              exception_state);
}

bool AudioParamHandler::InsertEvent(std::unique_ptr<ParamEvent> event,
                                    ExceptionState& exception_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "AudioParamHandler::InsertEvent");

  DCHECK(IsMainThread());

  // Sanity check the event. Be super careful we're not getting infected with
  // NaN or Inf. These should have been handled by the caller.
  DCHECK_LT(event->GetType(), ParamEvent::Type::kLastType);
  DCHECK(std::isfinite(event->Value()));
  DCHECK(std::isfinite(event->Time()));
  DCHECK(std::isfinite(event->TimeConstant()));
  DCHECK(std::isfinite(event->Duration()));
  DCHECK_GE(event->Duration(), 0);

  double insert_time = event->Time();

  if (events_.empty() &&
      (event->GetType() == ParamEvent::Type::kLinearRampToValue ||
       event->GetType() == ParamEvent::Type::kExponentialRampToValue)) {
    // There are no events preceding these ramps.  Insert a new
    // setValueAtTime event to set the starting point for these
    // events.
    events_.insert(
        0, AudioParamHandler::ParamEvent::CreateSetValueEvent(
               event->InitialValue(), DestinationHandler().CurrentTime()));
    new_events_.insert(events_[0].get());
  }

  if (events_.empty()) {
    events_.insert(0, std::move(event));
    new_events_.insert(events_[0].get());
    return true;
  }

  // Most of the time, we must insert after the last event. If the time of the
  // last event is greater than the insert_time, use binary search to find the
  // insertion point.
  wtf_size_t insertion_idx = events_.size();
  DCHECK_GT(insertion_idx, wtf_size_t{0});
  wtf_size_t ub = insertion_idx - 1;  // upper bound of events that can overlap.
  if (events_.back()->Time() > insert_time) {
    auto it = std::upper_bound(
        events_.begin(), events_.end(), insert_time,
        [](const double value, const std::unique_ptr<ParamEvent>& entry) {
          return value < entry->Time();
        });
    insertion_idx = static_cast<wtf_size_t>(std::distance(events_.begin(), it));
    DCHECK_LT(insertion_idx, events_.size());
    ub = insertion_idx;
  }
  DCHECK_LT(ub, static_cast<wtf_size_t>(std::numeric_limits<int>::max()));

  if (event->GetType() == ParamEvent::Type::kSetValueCurve) {
    double end_time = event->Time() + event->Duration();
    for (int i = ub; i >= 0; i--) {
      ParamEvent::Type test_type = events_[i]->GetType();
      // Events of type `kSetValueCurveEnd` or `kCancelValues` never conflict.
      if (test_type == ParamEvent::Type::kSetValueCurveEnd ||
          test_type == ParamEvent::Type::kCancelValues) {
        continue;
      }
      if (test_type == ParamEvent::Type::kSetValueCurve) {
        // A SetValueCurve overlapping an existing SetValueCurve requires
        // special care.
        double test_end_time = events_[i]->Time() + events_[i]->Duration();
        // Events are overlapped if the new event starts before the old event
        // ends and the old event starts before the new event ends.
        bool overlap =
            event->Time() < test_end_time && events_[i]->Time() < end_time;
        if (overlap) {
          // If the start time of the event overlaps the start/end of an
          // existing event or if the existing event end overlaps the
          // start/end of the event, it's an error.
          exception_state.ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              StrCat({EventToString(*event), " overlaps ",
                      EventToString(*events_[i])}));
          return false;
        }
      } else {
        // Here we handle existing events of types other than
        // `kSetValueCurveEnd`, `kCancelValues` and `kSetValueCurve`.
        // Throw an error if an existing event starts in the middle of this
        // SetValueCurve event.
        if (events_[i]->Time() > event->Time() &&
            events_[i]->Time() < end_time) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              StrCat({EventToString(*event), " overlaps ",
                      EventToString(*events_[i])}));
          return false;
        }
      }
      if (events_[i]->Time() < insert_time) {
        // We found an existing event, E, of type other than `kSetValueCurveEnd`
        // or `kCancelValues` that starts before the new event of type
        // `kSetValueCurve` that we want to insert. No earlier existing event
        // can overlap with the new event. An overlapping `kSetValueCurve` would
        // have overlapped with E too, so one of them would not be inserted.
        // Other event types overlap with the new `kSetValueCurve` event only if
        // they start in the middle of the new event, which is not the case.
        break;
      }
    }
  } else {
    // Not a `SetValueCurve` new event. Make sure this new event doesn't overlap
    // any existing `SetValueCurve` event.
    for (int i = ub; i >= 0; i--) {
      ParamEvent::Type test_type = events_[i]->GetType();
      // Events of type `kSetValueCurveEnd` or `kCancelValues` never conflict.
      if (test_type == ParamEvent::Type::kSetValueCurveEnd ||
          test_type == ParamEvent::Type::kCancelValues) {
        continue;
      }
      if (test_type == ParamEvent::Type::kSetValueCurve) {
        double end_time = events_[i]->Time() + events_[i]->Duration();
        if (event->GetType() != ParamEvent::Type::kSetValueCurveEnd &&
            event->Time() >= events_[i]->Time() && event->Time() < end_time) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              StrCat({EventToString(*event), " overlaps ",
                      EventToString(*events_[i])}));
          return false;
        }
      }
      if (events_[i]->Time() < insert_time) {
        // We found an existing event, E, of type other than `kSetValueCurveEnd`
        // or `kCancelValues` that starts before the new event that we want to
        // insert. No earlier event of type `kSetValueCurve` can overlap with
        // the new event, because it would have overlapped with E too.
        break;
      }
    }
  }

  events_.insert(insertion_idx, std::move(event));
  new_events_.insert(events_[insertion_idx].get());
  return true;
}

bool AudioParamHandler::HasSampleAccurateValues() const {
  if (NumberOfRenderingConnections()) {
    return true;
  }

  base::AutoTryLock try_locker(events_lock_);

  if (try_locker.is_acquired()) {
    // Return true if the AudioParam timeline needs to run in this rendering
    // quantum.  This means some automation is already running or is scheduled
    // to run in the current rendering quantum.
    const unsigned n_events = events_.size();

    // Clearly, if there are no scheduled events, we have no timeline values.
    if (n_events == 0) {
      return false;
    }

    const size_t current_frame = destination_handler_->CurrentSampleFrame();
    const double sample_rate = destination_handler_->SampleRate();
    const unsigned render_quantum_frames = RenderQuantumFrames();

    // Handle the case where the first event (of certain types) is in the
    // future.  Then, no sample-accurate processing is needed because the event
    // hasn't started.
    if (events_[0]->Time() >
        (current_frame + render_quantum_frames) / sample_rate) {
      switch (events_[0]->GetType()) {
        case ParamEvent::Type::kSetTarget:
        case ParamEvent::Type::kSetValue:
        case ParamEvent::Type::kSetValueCurve:
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
    // complicated and keeping this consistent with `ValuesForFrameRangeImpl()`
    // will be hard, so it's probably best to let the general timeline handle
    // this until the events are in the past.
    if (n_events >= 2) {
      return true;
    }

    // We have exactly one event in the timeline.
    switch (events_[0]->GetType()) {
      case ParamEvent::Type::kSetTarget:
        // Need automation if the event starts somewhere before the
        // end of the current render quantum.
        return events_[0]->Time() <=
               (current_frame + render_quantum_frames) / sample_rate;
      case ParamEvent::Type::kSetValue:
      case ParamEvent::Type::kLinearRampToValue:
      case ParamEvent::Type::kExponentialRampToValue:
      case ParamEvent::Type::kCancelValues:
      case ParamEvent::Type::kSetValueCurveEnd:
        // If these events are in the past, we don't need any automation; the
        // value is a constant.
        return !(events_[0]->Time() < current_frame / sample_rate);
      case ParamEvent::Type::kSetValueCurve: {
        double curve_end_time = events_[0]->Time() + events_[0]->Duration();
        double current_time = current_frame / sample_rate;

        return (events_[0]->Time() <= current_time) &&
               (current_time < curve_end_time);
      }
      case ParamEvent::Type::kLastType:
        NOTREACHED();
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

void AudioParamHandler::CancelScheduledValues(double cancel_time,
                                              ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(cancel_time, exception_state)) {
    return;
  }

  cancel_time = ClampedToCurrentTime(cancel_time);

  base::AutoLock locker(events_lock_);

  // Remove all events starting at startTime.
  for (wtf_size_t i = 0; i < events_.size(); ++i) {
    // Removal all events whose event time (start) is greater than or
    // equal to the cancel time.  And also handle the special case
    // where the cancel time lies in the middle of a setValueCurve
    // event.
    //
    // This critically depends on the fact that no event can be
    // scheduled in the middle of the curve or at the same start time.
    // Then removing the setValueCurve doesn't remove any events that
    // shouldn't have been.
    double start_time = events_[i]->Time();

    if (start_time >= cancel_time ||
        ((events_[i]->GetType() == ParamEvent::Type::kSetValueCurve) &&
         start_time <= cancel_time &&
         (start_time + events_[i]->Duration() > cancel_time))) {
      RemoveCancelledEvents(i);
      break;
    }
  }
}

void AudioParamHandler::CancelAndHoldAtTime(double cancel_time,
                                            ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!IsNonNegativeAudioParamTime(cancel_time, exception_state)) {
    return;
  }

  cancel_time = ClampedToCurrentTime(cancel_time);

  base::AutoLock locker(events_lock_);

  wtf_size_t i;
  // Find the first event at or just past `cancel_time`.
  for (i = 0; i < events_.size(); ++i) {
    if (events_[i]->Time() > cancel_time) {
      break;
    }
  }

  // The event that is being cancelled.  This is the event just past
  // `cancel_time`, if any.
  wtf_size_t cancelled_event_index = i;

  // If the event just before `cancel_time` is a SetTarget or SetValueCurve
  // event, we need to handle that event specially instead of the event after.
  if (i > 0 &&
      ((events_[i - 1]->GetType() == ParamEvent::Type::kSetTarget) ||
       (events_[i - 1]->GetType() == ParamEvent::Type::kSetValueCurve))) {
    cancelled_event_index = i - 1;
  } else if (i >= events_.size()) {
    // If there were no events occurring after `cancel_time` (and the
    // previous event is not SetTarget or SetValueCurve, we're done.
    return;
  }

  // cancelledEvent is the event that is being cancelled.
  ParamEvent* cancelled_event = events_[cancelled_event_index].get();
  ParamEvent::Type event_type = cancelled_event->GetType();

  // New event to be inserted, if any, and a SetValueEvent if needed.
  std::unique_ptr<ParamEvent> new_event;
  std::unique_ptr<ParamEvent> new_set_value_event;

  switch (event_type) {
    case ParamEvent::Type::kLinearRampToValue:
    case ParamEvent::Type::kExponentialRampToValue: {
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
    case ParamEvent::Type::kSetTarget: {
      if (cancelled_event->Time() < cancel_time) {
        // Don't want to remove the SetTarget event if it started before the
        // cancel time, so bump the index.  But we do want to insert a
        // cancelEvent so that we stop this automation and hold the value when
        // we get there.
        ++cancelled_event_index;

        new_event = ParamEvent::CreateCancelValuesEvent(cancel_time, nullptr);
      }
    } break;
    case ParamEvent::Type::kSetValueCurve: {
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
              cancelled_event->Curve());

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
    case ParamEvent::Type::kSetValue:
    case ParamEvent::Type::kSetValueCurveEnd:
    case ParamEvent::Type::kCancelValues:
      // Nothing needs to be done for a SetValue or CancelValues event.
      break;
    case ParamEvent::Type::kLastType:
      NOTREACHED();
  }

  // Now remove all the following events from the timeline.
  if (cancelled_event_index < events_.size()) {
    RemoveCancelledEvents(cancelled_event_index);
  }

  // Insert the new event, if any.
  if (new_event) {
    InsertEvent(std::move(new_event), exception_state);
    if (new_set_value_event) {
      InsertEvent(std::move(new_set_value_event), exception_state);
    }
  }
}

std::tuple<bool, float> AudioParamHandler::ValueForContextTime(
    AudioDestinationHandler& audio_destination,
    float default_value,
    float min_value,
    float max_value,
    unsigned render_quantum_frames) {
  {
    base::AutoTryLock try_locker(events_lock_);
    if (!try_locker.is_acquired() || !events_.size() ||
        audio_destination.CurrentTime() < events_[0]->Time()) {
      return std::make_tuple(false, default_value);
    }
  }

  // Ask for just a single value.
  float value;
  double sample_rate = audio_destination.SampleRate();
  size_t start_frame = audio_destination.CurrentSampleFrame();
  // One parameter change per render quantum.
  double control_rate = sample_rate / render_quantum_frames;
  value = ValuesForFrameRange(
      start_frame, start_frame + 1, default_value, base::span_from_ref(value),
      sample_rate, control_rate, min_value, max_value, render_quantum_frames);

  return std::make_tuple(true, value);
}

float AudioParamHandler::ValuesForFrameRange(size_t start_frame,
                                             size_t end_frame,
                                             float default_value,
                                             base::span<float> values,
                                             double sample_rate,
                                             double control_rate,
                                             float min_value,
                                             float max_value,
                                             unsigned render_quantum_frames) {
  // We can't contend the lock in the realtime audio thread.
  base::AutoTryLock try_locker(events_lock_);
  if (!try_locker.is_acquired()) {
    std::ranges::fill(values, default_value);
    return default_value;
  }

  float last_value =
      ValuesForFrameRangeImpl(start_frame, end_frame, default_value, values,
                              sample_rate, control_rate, render_quantum_frames);

  // Clamp the values now to the nominal range
  vector_math::Vclip(values, 1, &min_value, &max_value, values, 1);

  return last_value;
}

float AudioParamHandler::ValuesForFrameRangeImpl(
    const size_t start_frame,
    const size_t end_frame,
    float default_value,
    base::span<float> values,
    const double sample_rate,
    const double control_rate,
    unsigned render_quantum_frames) {
  DCHECK_GE(values.size(), 1u);

  // Return default value if there are no events matching the desired time
  // range.
  if (!events_.size() || (end_frame / sample_rate <= events_[0]->Time())) {
    std::ranges::fill(values, default_value);
    return default_value;
  }

  int number_of_events = events_.size();

  // MUST clamp event before `events_` is possibly mutated because
  // `new_events_` has raw pointers to objects in `events_`.  Clamping
  // will clear out all of these pointers before `events_` is
  // potentially modified.
  //
  // TODO(rtoy): Consider making `events_` be scoped_refptr instead of
  // unique_ptr.
  if (new_events_.size() > 0) {
    ClampNewEventsToCurrentTime(start_frame / sample_rate);
  }

  if (number_of_events > 0) {
    double current_time = start_frame / sample_rate;

    if (HandleAllEventsInThePast(current_time, sample_rate, default_value,
                                 values, render_quantum_frames)) {
      return default_value;
    }
  }

  // Maintain a running time (frame) and index for writing the values buffer.
  // If first event is after startFrame then fill initial part of values buffer
  // with defaultValue until we reach the first event time.
  auto [current_frame, write_index] =
      HandleFirstEvent(values, default_value, start_frame, end_frame,
                       sample_rate, start_frame, 0);

  float value = default_value;

  // Go through each event and render the value buffer where the times overlap,
  // stopping when we've rendered all the requested values.
  int last_skipped_event_index = 0;
  for (int i = 0; i < number_of_events && write_index < values.size(); ++i) {
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
    ProcessSetTargetFollowedByRamp(
        i, event,
        next_event ? static_cast<ParamEvent::Type>(next_event->GetType())
                   : ParamEvent::Type::kLastType,
        current_frame, sample_rate, control_rate, value);

    const float value1 = event->Value();
    const double time1 = event->Time();

    // Check to see if an event was cancelled.
    const auto [value2, time2, next_event_type] = HandleCancelValues(
        event, next_event, next_event ? next_event->Value() : value1,
        next_event ? next_event->Time() : end_frame / sample_rate + 1);

    DCHECK(!std::isnan(value1));
    DCHECK(!std::isnan(value2));
    CHECK_GE(time2, time1);

    // `fill_to_end_frame` is the exclusive upper bound of the last frame to be
    // computed for this event.  It's either the last desired frame
    // (`end_frame`) or derived from the end time of the next event
    // (`time2`). We compute ceil(`time2`*`sample_rate`) because
    // `fill_to_end_frame` is the exclusive upper bound.  Consider the case
    // where `start_frame` = 128 and `time2` = 128.1 (assuming `sample_rate` =
    // 1).  Since `time2` is greater than 128, we want to output a value for
    // frame 128.  This requires that `fill_to_end_frame` be at least 129.  This
    // is achieved by ceil(`time2`).
    //
    // However, `time2` can be very large, so compute this carefully in the case
    // where `time2` exceeds the size of a size_t.

    const size_t fill_to_end_frame =
        end_frame > time2 * sample_rate
            ? static_cast<size_t>(ceil(time2 * sample_rate))
            : end_frame;

    DCHECK_GE(fill_to_end_frame, start_frame);
    const size_t fill_to_frame =
        std::min(fill_to_end_frame - start_frame, values.size());
    // Time should be monotonically forward. So `fill_to_frame` should be
    // greater than or equal to `write_index`. We have ensured that the time
    // does not overlap when inserting events.
    CHECK_GE(fill_to_frame, write_index);

    // First handle linear and exponential ramps which require looking ahead to
    // the next event.
    if (next_event_type == ParamEvent::Type::kLinearRampToValue) {
      std::tie(current_frame, value, write_index) = ProcessLinearRamp(
          fill_to_frame, time1, time2, value1, value2, sample_rate, values,
          current_frame, value, write_index);
    } else if (next_event_type == ParamEvent::Type::kExponentialRampToValue) {
      std::tie(current_frame, value, write_index) = ProcessExponentialRamp(
          fill_to_frame, time1, time2, value1, value2, sample_rate, values,
          current_frame, value, write_index);
    } else {
      // Handle event types not requiring looking ahead to the next event.
      switch (event->GetType()) {
        case ParamEvent::Type::kSetValue:
        case ParamEvent::Type::kSetValueCurveEnd:
        case ParamEvent::Type::kLinearRampToValue: {
          current_frame = fill_to_end_frame;

          // Simply stay at a constant value.
          value = event->Value();
          std::ranges::fill(
              values.subspan(write_index, fill_to_frame - write_index), value);
          write_index = fill_to_frame;
          break;
        }

        case ParamEvent::Type::kCancelValues: {
          std::tie(current_frame, value, write_index) =
              ProcessCancelValues(fill_to_frame, time1, sample_rate,
                                  control_rate, fill_to_end_frame, event, i,
                                  values, current_frame, value, write_index);
          break;
        }

        case ParamEvent::Type::kExponentialRampToValue: {
          current_frame = fill_to_end_frame;

          // If we're here, we've reached the end of the ramp.  For
          // the values after the end of the ramp, we just want to
          // continue with the ramp end value.
          value = event->Value();
          std::ranges::fill(
              values.subspan(write_index, fill_to_frame - write_index), value);
          write_index = fill_to_frame;
          break;
        }

        case ParamEvent::Type::kSetTarget: {
          std::tie(current_frame, value, write_index) =
              ProcessSetTarget(fill_to_frame, time1, value1, sample_rate,
                               control_rate, fill_to_end_frame, event, values,
                               current_frame, value, write_index);
          break;
        }

        case ParamEvent::Type::kSetValueCurve: {
          std::tie(current_frame, value, write_index) = ProcessSetValueCurve(
              fill_to_frame, time1, sample_rate, start_frame, end_frame,
              fill_to_end_frame, event, values, current_frame, value,
              write_index);
          break;
        }
        case ParamEvent::Type::kLastType:
          NOTREACHED();
      }
    }
  }

  // If we skipped over any events (because they are in the past), we can
  // remove them so we don't have to check them ever again.  (This MUST be
  // running with the m_events lock so we can safely modify the m_events
  // array.)
  if (last_skipped_event_index > 0) {
    // `new_events_` should be empty here so we don't have to
    // do any updates due to this mutation of `events_`.
    DCHECK_EQ(new_events_.size(), 0u);

    RemoveOldEvents(last_skipped_event_index - 1);
  }

  // If there's any time left after processing the last event then just
  // propagate the last value to the end of the values buffer.
  std::ranges::fill(values.subspan(write_index), value);

  // This value is used to set the `.value` attribute of the AudioParam.  it
  // should be the last computed value.
  return values.back();
}

std::tuple<size_t, unsigned> AudioParamHandler::HandleFirstEvent(
    base::span<float> values,
    float default_value,
    size_t start_frame,
    size_t end_frame,
    double sample_rate,
    size_t current_frame,
    unsigned write_index) {
  double first_event_time = events_[0]->Time();
  if (first_event_time > start_frame / sample_rate) {
    // `fill_to_frame` is an exclusive upper bound, so use ceil() to compute the
    // bound from the `first_event_time`.
    size_t fill_to_end_frame = end_frame;
    double first_event_frame = ceil(first_event_time * sample_rate);
    if (end_frame > first_event_frame) {
      fill_to_end_frame = first_event_frame;
    }
    DCHECK_GE(fill_to_end_frame, start_frame);

    size_t fill_to_frame = fill_to_end_frame - start_frame;
    fill_to_frame = std::min(fill_to_frame, values.size());
    std::ranges::fill(values.subspan(write_index, fill_to_frame - write_index),
                      default_value);
    write_index = fill_to_frame;
    current_frame += fill_to_frame;
  }

  return std::make_tuple(current_frame, write_index);
}

bool AudioParamHandler::IsEventCurrent(const ParamEvent* event,
                                       const ParamEvent* next_event,
                                       size_t current_frame,
                                       double sample_rate) const {
  // WARNING: due to round-off it might happen that `next_event->Time()` is just
  // larger than `current_frame`/`sample_rate`.  This means that we will end up
  // running the `event` again.  The code below had better be prepared for this
  // case!  What should happen is the fillToFrame should be 0 so that while the
  // event is actually run again, nothing actually gets computed, and we move on
  // to the next event.
  //
  // An example of this case is `SetValueCurveAtTime()`.  The time at which
  // `SetValueCurveAtTime()` ends (and the `SetValueAtTime()` begins) might be
  // just past `current_time`/`sample_rate`.  Then `SetValueCurveAtTime()` will
  // be processed again before advancing to `SetValueAtTime()`.  The number of
  // frames to be processed should be zero in this case.
  if (next_event && next_event->Time() < current_frame / sample_rate) {
    // But if the current event is a SetValue event and the event time is
    // between currentFrame - 1 and currentFrame (in time). we don't want to
    // skip it.  If we do skip it, the SetValue event is completely skipped
    // and not applied, which is wrong.  Other events don't have this problem.
    // (Because currentFrame is unsigned, we do the time check in this funny,
    // but equivalent way.)
    double event_frame = event->Time() * sample_rate;

    // Condition is currentFrame - 1 < eventFrame <= currentFrame, but
    // currentFrame is unsigned and could be 0, so use
    // currentFrame < eventFrame + 1 instead.
    if (!(((event->GetType() == ParamEvent::Type::kSetValue ||
            event->GetType() == ParamEvent::Type::kSetValueCurveEnd) &&
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

void AudioParamHandler::ClampNewEventsToCurrentTime(double current_time) {
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
    std::stable_sort(events_.begin(), events_.end(), ParamEvent::EventPrecedes);
  }

  new_events_.clear();
}

bool AudioParamHandler::HandleAllEventsInThePast(
    double current_time,
    double sample_rate,
    float& default_value,
    base::span<float> values,
    unsigned render_quantum_frames) {
  // Optimize the case where the last event is in the past.
  ParamEvent* last_event = events_[events_.size() - 1].get();
  ParamEvent::Type last_event_type = last_event->GetType();
  double last_event_time = last_event->Time();

  // If the last event is in the past and the event has ended, then we can
  // just propagate the same value.  Except for SetTarget which lasts
  // "forever".  SetValueCurve also has an explicit SetValue at the end of
  // the curve, so we don't need to worry that SetValueCurve time is a
  // start time, not an end time.
  if (last_event_time + 1.5 * render_quantum_frames / sample_rate <
      current_time) {
    // If the last event is SetTarget, make sure we've converged and, that
    // we're at least 5 time constants past the start of the event.  If not, we
    // have to continue processing it.
    if (last_event_type == ParamEvent::Type::kSetTarget) {
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

    // `events_` is being mutated.  `new_events_` better be empty because
    // there are raw pointers there.
    DCHECK_EQ(new_events_.size(), 0U);

    // The event has finished, so just copy the default value out.
    // Since all events are now also in the past, we can just remove all
    // timeline events too because `default_value` has the expected
    // value.
    std::ranges::fill(values, default_value);
    RemoveOldEvents(events_.size());

    return true;
  }

  return false;
}

void AudioParamHandler::ProcessSetTargetFollowedByRamp(
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
  if (event->GetType() == ParamEvent::Type::kSetTarget &&
      (next_event_type == ParamEvent::Type::kLinearRampToValue ||
       next_event_type == ParamEvent::Type::kExponentialRampToValue)) {
    // Replace the SetTarget with a SetValue to set the starting time and
    // value for the ramp using the current frame.  We need to update `value`
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
                  fdlibm::exp(-(current_frame / sample_rate - event->Time()) /
                              event->TimeConstant());
    } else {
      // SetTarget has already started.  Update `value` one frame because it's
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

std::tuple<float, double, AudioParamHandler::ParamEvent::Type>
AudioParamHandler::HandleCancelValues(const ParamEvent* current_event,
                                      ParamEvent* next_event,
                                      float value2,
                                      double time2) {
  DCHECK(current_event);

  ParamEvent::Type next_event_type =
      next_event ? next_event->GetType() : ParamEvent::Type::kLastType;

  if (next_event && next_event->GetType() == ParamEvent::Type::kCancelValues &&
      next_event->SavedEvent()) {
    float value1 = current_event->Value();
    double time1 = current_event->Time();

    switch (current_event->GetType()) {
      case ParamEvent::Type::kCancelValues:
      case ParamEvent::Type::kLinearRampToValue:
      case ParamEvent::Type::kExponentialRampToValue:
      case ParamEvent::Type::kSetValueCurveEnd:
      case ParamEvent::Type::kSetValue: {
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
            case ParamEvent::Type::kLinearRampToValue:
              value2 =
                  LinearRampAtTime(next_event->Time(), value1, time1,
                                   saved_event->Value(), saved_event->Time());
              break;
            case ParamEvent::Type::kExponentialRampToValue:
              value2 = ExponentialRampAtTime(next_event->Time(), value1, time1,
                                             saved_event->Value(),
                                             saved_event->Time());
              DCHECK(!std::isnan(value1));
              break;
            case ParamEvent::Type::kSetValueCurve:
            case ParamEvent::Type::kSetValueCurveEnd:
            case ParamEvent::Type::kSetValue:
            case ParamEvent::Type::kSetTarget:
            case ParamEvent::Type::kCancelValues:
              // These cannot be possible types for the saved event
              // because they can't be created.
              // createCancelValuesEvent doesn't allow them (SetValue,
              // SetTarget, CancelValues) or cancelScheduledValues()
              // doesn't create such an event (SetValueCurve).
              NOTREACHED();
            case ParamEvent::Type::kLastType:
              // Illegal event type.
              NOTREACHED();
          }

          // Cache the new value so we don't keep computing it over and over.
          next_event->SetCancelledValue(value2);
        }
      } break;
      case ParamEvent::Type::kSetValueCurve:
        // Everything needed for this was handled when cancelling was
        // done.
        break;
      case ParamEvent::Type::kSetTarget:
        // Nothing special needs to be done for SetTarget
        // followed by CancelValues.
        break;
      case ParamEvent::Type::kLastType:
        NOTREACHED();
    }
  }

  return std::make_tuple(value2, time2, next_event_type);
}

std::tuple<size_t, float, unsigned> AudioParamHandler::ProcessLinearRamp(
    const size_t fill_to_frame,
    const double time1,
    const double time2,
    const float value1,
    const float value2,
    const double sample_rate,
    base::span<float> values,
    size_t current_frame,
    float value,
    unsigned write_index) {
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
    // `fill_to_frame_trunc` should be less than or equal to `fill_to_frame`.
    CHECK_LE(fill_to_frame_trunc, fill_to_frame);
    current_frame += fill_to_frame_trunc - write_index;

    // Process 4 loop steps.
    for (; write_index < fill_to_frame_trunc; write_index += 4) {
      // SAFETY: DCHECK previously checked that `fill_to_frame_trunc <
      // values.size()`. In the for loop, `write_index < fill_to_frame_trunc` so
      // this is safe.
      _mm_storeu_ps(UNSAFE_BUFFERS(values.data() + write_index), v_value);
      v_value = _mm_add_ps(v_value, v_inc);
    }
  }
  // Update `value` with the last value computed so that the
  // `.value` attribute of the AudioParam gets the correct linear
  // ramp value, in case the following loop doesn't execute.
  if (write_index >= 1) {
    value = values[write_index - 1];
  }
#endif

  DCHECK_GE(fill_to_frame, write_index);
  // Serially process remaining values.
  std::ranges::generate(
      values.subspan(write_index, fill_to_frame - write_index),
      [=, &current_frame, &value]() {
        float x = (current_frame / sample_rate - time1) * k;
        // value = (1 - x) * value1 + x * value2;
        value = value1 + x * value_delta;
        ++current_frame;
        return value;
      });

  return std::make_tuple(current_frame, value, fill_to_frame);
}

std::tuple<size_t, float, unsigned> AudioParamHandler::ProcessExponentialRamp(
    const size_t fill_to_frame,
    const double time1,
    const double time2,
    const float value1,
    const float value2,
    const double sample_rate,
    base::span<float> values,
    size_t current_frame,
    float value,
    unsigned write_index) {
  DCHECK_GE(fill_to_frame, write_index);
  if (value1 * value2 <= 0 || time1 >= time2) {
    // It's an error 1) if `value1` and `value2` have opposite signs or if one
    // of them is zero, or 2) if `time1` is greater than or equal to `time2`.
    // Handle this by propagating the previous value.
    value = value1;
    std::ranges::fill(values.subspan(write_index, fill_to_frame - write_index),
                      value);
    write_index = fill_to_frame;
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
    double multiplier = fdlibm::pow(value2 / value1, 1.0 / num_sample_frames);
    // Set the starting value of the exponential ramp.  Do not attempt
    // to optimize pow to powf.  See crbug.com/771306.
    value = value1 *
            fdlibm::pow(value2 / static_cast<double>(value1),
                        (current_frame / sample_rate - time1) / delta_time);
    double accumulator = value;
    std::ranges::generate(
        values.subspan(write_index, fill_to_frame - write_index), [&]() {
          value = accumulator;
          accumulator *= multiplier;
          ++current_frame;
          return value;
        });
    write_index = fill_to_frame;
    // Due to roundoff it's possible that value exceeds value2.  Clip value
    // to value2 if we are within 1/2 frame of time2.
    if (current_frame > time2 * sample_rate - 0.5) {
      value = value2;
    }
  }

  return std::make_tuple(current_frame, value, write_index);
}

std::tuple<size_t, float, unsigned> AudioParamHandler::ProcessSetTarget(
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
    unsigned write_index) {
  DCHECK_GE(fill_to_frame, write_index);

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
      value = target + (value - target) *
                           fdlibm::exp(-(current_frame / sample_rate - time1) /
                                       time_constant);
    } else {
      // Otherwise, need to compute a new value because `value` is the
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
    std::ranges::fill(values.subspan(write_index, fill_to_frame - write_index),
                      target);
    write_index = fill_to_frame;
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
      // `fill_to_frame_trunc` should be less than or equal to `fill_to_frame`.
      CHECK_LE(fill_to_frame_trunc, fill_to_frame);

      for (; write_index < fill_to_frame_trunc; write_index += 4) {
        delta = target - value;
        v_delta = _mm_set_ps1(delta);
        v_value = _mm_set_ps1(value);

        v_result = _mm_add_ps(v_value, _mm_mul_ps(v_delta, v_c));
        // SAFETY: DCHECK previously checked that `fill_to_frame_trunc <
        // values.size()`. In the for loop, `write_index < fill_to_frame_trunc`
        // so this is safe.
        _mm_storeu_ps(UNSAFE_BUFFERS(values.data() + write_index), v_result);

        // Update value for next iteration.
        value += delta * c3;
      }
    }
#endif

    // Serially process remaining values
    std::ranges::generate(
        values.subspan(write_index, fill_to_frame - write_index), [&]() {
          float v = value;
          value += (target - value) * discrete_time_constant;
          return v;
        });
    write_index = fill_to_frame;
    // The previous loops may have updated `value` one extra time.
    // Reset it to the last computed value.
    if (write_index >= 1) {
      value = values[write_index - 1];
    }
    current_frame = fill_to_end_frame;
  }

  return std::make_tuple(current_frame, value, write_index);
}

std::tuple<size_t, float, unsigned> AudioParamHandler::ProcessSetValueCurve(
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
    unsigned write_index) {
  DCHECK_GE(fill_to_frame, write_index);

  base::span<const float> curve_data(event->Curve());

  float curve_end_value = event->CurveEndValue();

  // Curve events have duration, so don't just use next event time.
  double duration = event->Duration();
  // How much to step the curve index for each frame.  This is basically
  // the term (N - 1)/Td in the specification.
  double curve_points_per_frame = event->CurvePointsPerSecond() / sample_rate;

  if (curve_data.empty() || duration <= 0 || sample_rate <= 0) {
    // Error condition - simply propagate previous value.
    current_frame = fill_to_end_frame;
    std::ranges::fill(values.subspan(write_index, fill_to_frame - write_index),
                      value);
    return std::make_tuple(current_frame, value, fill_to_frame);
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
    if (end_frame > curve_end_frame) {
      fill_to_end_frame = static_cast<size_t>(curve_end_frame);
    } else {
      fill_to_end_frame = end_frame;
    }
  }

  // `fill_to_frame` can be less than `start_frame` when the end of the
  // setValueCurve automation has been reached, but the next automation
  // has not yet started. In this case, `fill_to_frame` is clipped to
  // `time1`+`duration` above, but `start_frame` will keep increasing
  // (because the current time is increasing).
  fill_to_frame = (fill_to_end_frame < start_frame)
                      ? 0
                      : static_cast<unsigned>(fill_to_end_frame - start_frame);
  fill_to_frame = std::min(fill_to_frame, values.size());

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
        _mm_set_ps1(curve_data.size() - 1);
    const __m128 v_n1 = _mm_set_ps1(1.0f);
    const __m128 v_n4 = _mm_set_ps1(4.0f);

    __m128 v_k = _mm_set_ps(3, 2, 1, 0);
    int a_curve_index0[4];
    int a_curve_index1[4];

    // Truncate loop steps to multiple of 4
    unsigned truncated_steps = ((fill_to_frame - write_index) / 4) * 4;
    unsigned fill_to_frame_trunc = write_index + truncated_steps;
    // `fill_to_frame_trunc` should be less than or equal to `fill_to_frame`.
    CHECK_LE(fill_to_frame_trunc, fill_to_frame);

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
      // `delta` is clamped to 1 because `current_virtual_index` can exceed
      // `curve_index0` by more than one.  This can happen when we reached
      // the end of the curve but still need values to fill out the
      // current rendering quantum.
      _mm_storeu_si128(reinterpret_cast<__m128i*>(a_curve_index0),
                       v_curve_index0);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(a_curve_index1),
                       v_curve_index1);
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

      // SAFETY: DCHECK previously checked that `fill_to_frame_trunc <
      // values.size()`. In the for loop, `write_index < fill_to_frame_trunc` so
      // this is safe.
      _mm_storeu_ps(UNSAFE_BUFFERS(values.data() + write_index), v_value);
    }
    // Pass along k to the serial loop.
    k = truncated_steps;
  }
  if (write_index >= 1) {
    value = values[write_index - 1];
  }
#endif

  DCHECK_GE(fill_to_frame, write_index);
  std::ranges::generate(
      values.subspan(write_index, fill_to_frame - write_index), [&]() {
        // Compute current index this way to minimize round-off that would have
        // occurred by incrementing the index by curvePointsPerFrame.
        double current_virtual_index =
            curve_virtual_index + k * curve_points_per_frame;
        size_t curve_index0;

        // Clamp index to the last element of the array.
        if (current_virtual_index < curve_data.size()) {
          curve_index0 = static_cast<unsigned>(current_virtual_index);
        } else {
          curve_index0 = curve_data.size() - 1;
        }

        size_t curve_index1 = std::min(curve_index0 + 1, curve_data.size() - 1);

        // Linearly interpolate between the two nearest curve points.  `delta`
        // is clamped to 1 because `current_virtual_index` can exceed
        // `curve_index0` by more than one.  This can happen when we reached
        // the end of the curve but still need values to fill out the current
        // rendering quantum.
        DCHECK_LT(curve_index0, curve_data.size());
        DCHECK_LT(curve_index1, curve_data.size());
        float c0 = curve_data[curve_index0];
        float c1 = curve_data[curve_index1];
        double delta = std::min(current_virtual_index - curve_index0, 1.0);

        ++k;
        value = c0 + (c1 - c0) * delta;
        return value;
      });

  write_index = fill_to_frame;

  // If there's any time left after the duration of this event and the
  // start of the next, then just propagate the last value of the
  // `curve_data`. Don't modify `value` unless there is time left.
  if (write_index < next_event_fill_to_frame) {
    value = curve_end_value;
    std::ranges::fill(
        values.subspan(write_index, next_event_fill_to_frame - write_index),
        value);
    write_index = next_event_fill_to_frame;
  }

  // Re-adjust current time
  current_frame += next_event_fill_to_frame;

  return std::make_tuple(current_frame, value, write_index);
}

std::tuple<size_t, float, unsigned> AudioParamHandler::ProcessCancelValues(
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
    unsigned write_index) {
  DCHECK_GE(fill_to_frame, write_index);

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
      if (last_event_type == ParamEvent::Type::kSetTarget) {
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
  std::ranges::fill(values.subspan(write_index, fill_to_frame - write_index),
                    value);
  write_index = fill_to_frame;
  current_frame = fill_to_end_frame;

  return std::make_tuple(current_frame, value, write_index);
}

void AudioParamHandler::RemoveCancelledEvents(
    wtf_size_t first_event_to_remove) {
  // For all the events that are being removed, also remove that event
  // from `new_events_`.
  if (new_events_.size() > 0) {
    for (wtf_size_t k = first_event_to_remove; k < events_.size(); ++k) {
      new_events_.erase(events_[k].get());
    }
  }

  // Remove the cancelled events from the list.
  events_.EraseAt(first_event_to_remove,
                  events_.size() - first_event_to_remove);
}

void AudioParamHandler::RemoveOldEvents(wtf_size_t event_count) {
  wtf_size_t n_events = events_.size();
  DCHECK(event_count <= n_events);

  // Always leave at least one event in the event list!
  if (n_events > 1) {
    events_.EraseAt(0, std::min(event_count, n_events - 1));
  }
}

}  // namespace blink

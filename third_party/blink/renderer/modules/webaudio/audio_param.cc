/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/audio_param.h"

#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

const double AudioParamHandler::kDefaultSmoothingConstant = 0.05;
const double AudioParamHandler::kSnapThreshold = 0.001;

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
      summing_bus_(
          AudioBus::Create(1, audio_utilities::kRenderQuantumFrames, false)) {
  // An AudioParam needs the destination handler to run the timeline.  But the
  // destination may have been destroyed (e.g. page gone), so the destination is
  // null.  However, if the destination is gone, the AudioParam will never get
  // pulled, so this is ok.  We have checks for the destination handler existing
  // when the AudioParam want to use it.
  if (context.destination()) {
    destination_handler_ = &context.destination()->GetAudioDestinationHandler();
  }

  timeline_.SetSmoothedValue(default_value);
}

AudioDestinationHandler& AudioParamHandler::DestinationHandler() const {
  CHECK(destination_handler_);
  return *destination_handler_;
}

void AudioParamHandler::SetParamType(AudioParamType param_type) {
  param_type_ = param_type;
}

void AudioParamHandler::SetCustomParamName(const String name) {
  DCHECK(param_type_ == kParamTypeAudioWorklet);
  custom_param_name_ = name;
}

String AudioParamHandler::GetParamName() const {
  switch (GetParamType()) {
    case kParamTypeAudioBufferSourcePlaybackRate:
      return "AudioBufferSource.playbackRate";
    case kParamTypeAudioBufferSourceDetune:
      return "AudioBufferSource.detune";
    case kParamTypeBiquadFilterFrequency:
      return "BiquadFilter.frequency";
    case kParamTypeBiquadFilterQ:
      return "BiquadFilter.Q";
    case kParamTypeBiquadFilterGain:
      return "BiquadFilter.gain";
    case kParamTypeBiquadFilterDetune:
      return "BiquadFilter.detune";
    case kParamTypeDelayDelayTime:
      return "Delay.delayTime";
    case kParamTypeDynamicsCompressorThreshold:
      return "DynamicsCompressor.threshold";
    case kParamTypeDynamicsCompressorKnee:
      return "DynamicsCompressor.knee";
    case kParamTypeDynamicsCompressorRatio:
      return "DynamicsCompressor.ratio";
    case kParamTypeDynamicsCompressorAttack:
      return "DynamicsCompressor.attack";
    case kParamTypeDynamicsCompressorRelease:
      return "DynamicsCompressor.release";
    case kParamTypeGainGain:
      return "Gain.gain";
    case kParamTypeOscillatorFrequency:
      return "Oscillator.frequency";
    case kParamTypeOscillatorDetune:
      return "Oscillator.detune";
    case kParamTypeStereoPannerPan:
      return "StereoPanner.pan";
    case kParamTypePannerPositionX:
      return "Panner.positionX";
    case kParamTypePannerPositionY:
      return "Panner.positionY";
    case kParamTypePannerPositionZ:
      return "Panner.positionZ";
    case kParamTypePannerOrientationX:
      return "Panner.orientationX";
    case kParamTypePannerOrientationY:
      return "Panner.orientationY";
    case kParamTypePannerOrientationZ:
      return "Panner.orientationZ";
    case kParamTypeAudioListenerPositionX:
      return "AudioListener.positionX";
    case kParamTypeAudioListenerPositionY:
      return "AudioListener.positionY";
    case kParamTypeAudioListenerPositionZ:
      return "AudioListener.positionZ";
    case kParamTypeAudioListenerForwardX:
      return "AudioListener.forwardX";
    case kParamTypeAudioListenerForwardY:
      return "AudioListener.forwardY";
    case kParamTypeAudioListenerForwardZ:
      return "AudioListener.forwardZ";
    case kParamTypeAudioListenerUpX:
      return "AudioListener.upX";
    case kParamTypeAudioListenerUpY:
      return "AudioListener.upY";
    case kParamTypeAudioListenerUpZ:
      return "AudioListener.upZ";
    case kParamTypeConstantSourceOffset:
      return "ConstantSource.offset";
    case kParamTypeAudioWorklet:
      return custom_param_name_;
    default:
      NOTREACHED();
  }
}

float AudioParamHandler::Value() {
  // Update value for timeline.
  float v = IntrinsicValue();
  if (GetDeferredTaskHandler().IsAudioThread()) {
    bool has_value;
    float timeline_value;
    std::tie(has_value, timeline_value) = timeline_.ValueForContextTime(
        DestinationHandler(), v, MinValue(), MaxValue());

    if (has_value)
      v = timeline_value;
  }

  SetIntrinsicValue(v);
  return v;
}

void AudioParamHandler::SetIntrinsicValue(float new_value) {
  new_value = clampTo(new_value, min_value_, max_value_);
  intrinsic_value_.store(new_value, std::memory_order_relaxed);
}

void AudioParamHandler::SetValue(float value) {
  SetIntrinsicValue(value);
}

float AudioParamHandler::SmoothedValue() {
  return timeline_.SmoothedValue();
}

bool AudioParamHandler::Smooth() {
  // If values have been explicitly scheduled on the timeline, then use the
  // exact value.  Smoothing effectively is performed by the timeline.
  bool use_timeline_value = false;
  float value;
  std::tie(use_timeline_value, value) = timeline_.ValueForContextTime(
      DestinationHandler(), IntrinsicValue(), MinValue(), MaxValue());

  float smoothed_value = timeline_.SmoothedValue();
  if (smoothed_value == value) {
    // Smoothed value has already approached and snapped to value.
    SetIntrinsicValue(value);
    return true;
  }

  if (use_timeline_value) {
    timeline_.SetSmoothedValue(value);
  } else {
    // Dezipper - exponential approach.
    smoothed_value += (value - smoothed_value) * kDefaultSmoothingConstant;

    // If we get close enough then snap to actual value.
    // FIXME: the threshold needs to be adjustable depending on range - but
    // this is OK general purpose value.
    if (fabs(smoothed_value - value) < kSnapThreshold)
      smoothed_value = value;
    timeline_.SetSmoothedValue(smoothed_value);
  }

  SetIntrinsicValue(value);
  return false;
}

float AudioParamHandler::FinalValue() {
  float value = IntrinsicValue();
  CalculateFinalValues(&value, 1, false);
  return value;
}

void AudioParamHandler::CalculateSampleAccurateValues(
    float* values,
    unsigned number_of_values) {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  DCHECK(values);
  DCHECK_GT(number_of_values, 0u);

  CalculateFinalValues(values, number_of_values, IsAudioRate());
}

void AudioParamHandler::CalculateFinalValues(float* values,
                                             unsigned number_of_values,
                                             bool sample_accurate) {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  DCHECK(values);
  DCHECK_GT(number_of_values, 0u);

  // The calculated result will be the "intrinsic" value summed with all
  // audio-rate connections.

  if (sample_accurate) {
    // Calculate sample-accurate (a-rate) intrinsic values.
    CalculateTimelineValues(values, number_of_values);
  } else {
    // Calculate control-rate (k-rate) intrinsic value.
    bool has_value;
    float value = IntrinsicValue();
    float timeline_value;
    std::tie(has_value, timeline_value) = timeline_.ValueForContextTime(
        DestinationHandler(), value, MinValue(), MaxValue());

    if (has_value)
      value = timeline_value;

    for (unsigned k = 0; k < number_of_values; ++k) {
      values[k] = value;
    }
    SetIntrinsicValue(value);
  }

  // If there are any connections, sum all of the audio-rate connections
  // together (unity-gain summing junction).  Note that connections would
  // normally be mono, but we mix down to mono if necessary.
  if (NumberOfRenderingConnections() > 0) {
    DCHECK_LE(number_of_values, audio_utilities::kRenderQuantumFrames);

    summing_bus_->SetChannelMemory(0, values, number_of_values);

    for (unsigned i = 0; i < NumberOfRenderingConnections(); ++i) {
      AudioNodeOutput* output = RenderingOutput(i);
      DCHECK(output);

      // Render audio from this output.
      AudioBus* connection_bus =
          output->Pull(nullptr, audio_utilities::kRenderQuantumFrames);

      // Sum, with unity-gain.
      summing_bus_->SumFrom(*connection_bus);
    }

    // Clamp the values now to the nominal range
    float min_value = MinValue();
    float max_value = MaxValue();

    vector_math::Vclip(values, 1, &min_value, &max_value, values, 1,
                       number_of_values);
  }
}

void AudioParamHandler::CalculateTimelineValues(float* values,
                                                unsigned number_of_values) {
  // Calculate values for this render quantum.  Normally
  // |numberOfValues| will equal to
  // audio_utilities::kRenderQuantumFrames (the render quantum size).
  double sample_rate = DestinationHandler().SampleRate();
  size_t start_frame = DestinationHandler().CurrentSampleFrame();
  size_t end_frame = start_frame + number_of_values;

  // Note we're running control rate at the sample-rate.
  // Pass in the current value as default value.
  SetIntrinsicValue(timeline_.ValuesForFrameRange(
      start_frame, end_frame, IntrinsicValue(), values, number_of_values,
      sample_rate, sample_rate, MinValue(), MaxValue()));
}

// ----------------------------------------------------------------

AudioParam::AudioParam(BaseAudioContext& context,
                       const String& parent_uuid,
                       AudioParamHandler::AudioParamType param_type,
                       double default_value,
                       AudioParamHandler::AutomationRate rate,
                       AudioParamHandler::AutomationRateMode rate_mode,
                       float min_value,
                       float max_value)
    : InspectorHelperMixin(context.GraphTracer(), parent_uuid),
      handler_(AudioParamHandler::Create(context,
                                         param_type,
                                         default_value,
                                         rate,
                                         rate_mode,
                                         min_value,
                                         max_value)),
      context_(context),
      deferred_task_handler_(&context.GetDeferredTaskHandler()) {}

AudioParam* AudioParam::Create(BaseAudioContext& context,
                               const String& parent_uuid,
                               AudioParamHandler::AudioParamType param_type,
                               double default_value,
                               AudioParamHandler::AutomationRate rate,
                               AudioParamHandler::AutomationRateMode rate_mode,
                               float min_value,
                               float max_value) {
  DCHECK_LE(min_value, max_value);

  return MakeGarbageCollected<AudioParam>(context, parent_uuid, param_type,
                                          default_value, rate, rate_mode,
                                          min_value, max_value);
}

AudioParam::~AudioParam() {
  // The graph lock is required to destroy the handler. And we can't use
  // |context_| to touch it, since that object may also be a dead heap object.
  {
    DeferredTaskHandler::GraphAutoLocker locker(*deferred_task_handler_);
    handler_ = nullptr;
  }
}

void AudioParam::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  InspectorHelperMixin::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

float AudioParam::value() const {
  return Handler().Value();
}

void AudioParam::WarnIfOutsideRange(const String& param_method, float value) {
  if (value < minValue() || value > maxValue()) {
    Context()->GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning,
        Handler().GetParamName() + "." + param_method + " " +
            String::Number(value) + " outside nominal range [" +
            String::Number(minValue()) + ", " + String::Number(maxValue()) +
            "]; value will be clamped."));
  }
}

void AudioParam::setValue(float value) {
  WarnIfOutsideRange("value", value);
  Handler().SetValue(value);
}

void AudioParam::setValue(float value, ExceptionState& exception_state) {
  WarnIfOutsideRange("value", value);

  // This is to signal any errors, if necessary, about conflicting
  // automations.
  setValueAtTime(value, Context()->currentTime(), exception_state);
  // This is to change the value so that an immediate query for the
  // value returns the expected values.
  Handler().SetValue(value);
}

float AudioParam::defaultValue() const {
  return Handler().DefaultValue();
}

float AudioParam::minValue() const {
  return Handler().MinValue();
}

float AudioParam::maxValue() const {
  return Handler().MaxValue();
}

void AudioParam::SetParamType(AudioParamHandler::AudioParamType param_type) {
  Handler().SetParamType(param_type);
}

void AudioParam::SetCustomParamName(const String name) {
  Handler().SetCustomParamName(name);
}

String AudioParam::automationRate() const {
  switch (Handler().GetAutomationRate()) {
    case AudioParamHandler::AutomationRate::kAudio:
      return "a-rate";
    case AudioParamHandler::AutomationRate::kControl:
      return "k-rate";
    default:
      NOTREACHED();
      return "a-rate";
  }
}

void AudioParam::setAutomationRate(const String& rate,
                                   ExceptionState& exception_state) {
  if (Handler().IsAutomationRateFixed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        Handler().GetParamName() +
            ".automationRate is fixed and cannot be changed to \"" + rate +
            "\"");
    return;
  }

  if (rate == "a-rate") {
    Handler().SetAutomationRate(AudioParamHandler::AutomationRate::kAudio);
  } else if (rate == "k-rate") {
    Handler().SetAutomationRate(AudioParamHandler::AutomationRate::kControl);
  }
}

AudioParam* AudioParam::setValueAtTime(float value,
                                       double time,
                                       ExceptionState& exception_state) {
  WarnIfOutsideRange("setValueAtTime value", value);
  Handler().Timeline().SetValueAtTime(value, time, exception_state);
  return this;
}

AudioParam* AudioParam::linearRampToValueAtTime(
    float value,
    double time,
    ExceptionState& exception_state) {
  WarnIfOutsideRange("linearRampToValueAtTime value", value);
  Handler().Timeline().LinearRampToValueAtTime(
      value, time, Handler().IntrinsicValue(), Context()->currentTime(),
      exception_state);

  return this;
}

AudioParam* AudioParam::exponentialRampToValueAtTime(
    float value,
    double time,
    ExceptionState& exception_state) {
  WarnIfOutsideRange("exponentialRampToValue value", value);
  Handler().Timeline().ExponentialRampToValueAtTime(
      value, time, Handler().IntrinsicValue(), Context()->currentTime(),
      exception_state);

  return this;
}

AudioParam* AudioParam::setTargetAtTime(float target,
                                        double time,
                                        double time_constant,
                                        ExceptionState& exception_state) {
  WarnIfOutsideRange("setTargetAtTime value", target);
  Handler().Timeline().SetTargetAtTime(target, time, time_constant,
                                       exception_state);

  // Don't update the histogram here.  It's not clear in normal usage if the
  // parameter value will actually reach |target|.
  return this;
}

AudioParam* AudioParam::setValueCurveAtTime(const Vector<float>& curve,
                                            double time,
                                            double duration,
                                            ExceptionState& exception_state) {
  float min = minValue();
  float max = maxValue();

  // Find the first value in the curve (if any) that is outside the
  // nominal range.  It's probably not necessary to produce a warning
  // on every value outside the nominal range.
  for (unsigned k = 0; k < curve.size(); ++k) {
    float value = curve[k];

    if (value < min || value > max) {
      WarnIfOutsideRange("setValueCurveAtTime value", value);
      break;
    }
  }

  Handler().Timeline().SetValueCurveAtTime(curve, time, duration,
                                           exception_state);

  // We could update the histogram with every value in the curve, due to
  // interpolation, we'll probably be missing many values.  So we don't update
  // the histogram.  setValueCurveAtTime is probably a fairly rare method
  // anyway.
  return this;
}

AudioParam* AudioParam::cancelScheduledValues(double start_time,
                                              ExceptionState& exception_state) {
  Handler().Timeline().CancelScheduledValues(start_time, exception_state);
  return this;
}

AudioParam* AudioParam::cancelAndHoldAtTime(double start_time,
                                            ExceptionState& exception_state) {
  Handler().Timeline().CancelAndHoldAtTime(start_time, exception_state);
  return this;
}

}  // namespace blink

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_param_handler.h"

#include <algorithm>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <xmmintrin.h>
#elif defined(CPU_ARM_NEON)
#include <arm_neon.h>
#endif

namespace blink {

namespace {

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
      summing_bus_(
          AudioBus::Create(1,
                           GetDeferredTaskHandler().RenderQuantumFrames(),
                           false)) {
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

void AudioParamHandler::SetParamType(AudioParamType param_type) {
  param_type_ = param_type;
}

void AudioParamHandler::SetCustomParamName(const String name) {
  DCHECK(param_type_ == AudioParamType::kParamTypeAudioWorklet);
  custom_param_name_ = name;
}

String AudioParamHandler::GetParamName() const {
  switch (GetParamType()) {
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
  if (GetDeferredTaskHandler().IsAudioThread()) {
    auto [has_value, timeline_value] = timeline_.ValueForContextTime(
        DestinationHandler(), v, MinValue(), MaxValue(),
        GetDeferredTaskHandler().RenderQuantumFrames());

    if (has_value) {
      v = timeline_value;
    }
  }

  SetIntrinsicValue(v);
  return v;
}

void AudioParamHandler::SetIntrinsicValue(float new_value) {
  new_value = ClampTo(new_value, min_value_, max_value_);
  intrinsic_value_.store(new_value, std::memory_order_relaxed);
}

void AudioParamHandler::SetValue(float value) {
  SetIntrinsicValue(value);
}

float AudioParamHandler::FinalValue() {
  float value = IntrinsicValue();
  CalculateFinalValues(base::span_from_ref(value), false);
  return value;
}

void AudioParamHandler::CalculateSampleAccurateValues(
    base::span<float> values) {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  DCHECK(!values.empty());

  CalculateFinalValues(values, IsAudioRate());
}

void AudioParamHandler::CalculateFinalValues(base::span<float> values,
                                             bool sample_accurate) {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  DCHECK(!values.empty());

  // The calculated result will be the "intrinsic" value summed with all
  // audio-rate connections.

  if (sample_accurate) {
    // Calculate sample-accurate (a-rate) intrinsic values.
    CalculateTimelineValues(values);
  } else {
    // Calculate control-rate (k-rate) intrinsic value.
    float value = IntrinsicValue();
    auto [has_value, timeline_value] = timeline_.ValueForContextTime(
        DestinationHandler(), value, MinValue(), MaxValue(),
        GetDeferredTaskHandler().RenderQuantumFrames());

    if (has_value) {
      value = timeline_value;
    }

    std::ranges::fill(values, value);
    SetIntrinsicValue(value);
  }

  // If there are any connections, sum all of the audio-rate connections
  // together (unity-gain summing junction).  Note that connections would
  // normally be mono, but we mix down to mono if necessary.
  if (NumberOfRenderingConnections() > 0) {
    DCHECK_LE(values.size(), GetDeferredTaskHandler().RenderQuantumFrames());

    // If we're not sample accurate, we only need one value, so make the summing
    // bus have length 1.  When the connections are added in, only the first
    // value will be added.  Which is exactly what we want.
    summing_bus_->SetChannelMemory(0, values.data(),
                                   sample_accurate ? values.size() : 1);

    for (unsigned i = 0; i < NumberOfRenderingConnections(); ++i) {
      AudioNodeOutput* output = RenderingOutput(i);
      DCHECK(output);

      // Render audio from this output.
      AudioBus* connection_bus =
          output->Pull(nullptr, GetDeferredTaskHandler().RenderQuantumFrames());

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
  // GetDeferredTaskHandler().RenderQuantumFrames() (the render quantum size).
  double sample_rate = DestinationHandler().SampleRate();
  size_t start_frame = DestinationHandler().CurrentSampleFrame();
  size_t end_frame = start_frame + values.size();

  // Note we're running control rate at the sample-rate.
  // Pass in the current value as default value.
  SetIntrinsicValue(timeline_.ValuesForFrameRange(
      start_frame, end_frame, IntrinsicValue(), values, sample_rate,
      sample_rate, MinValue(), MaxValue(),
      GetDeferredTaskHandler().RenderQuantumFrames()));
}

}  // namespace blink

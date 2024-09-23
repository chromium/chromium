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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/dynamics_compressor.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/fdlibm/ieee754.h"

namespace blink {

namespace {

// Metering hits peaks instantly, but releases this fast (in seconds).
constexpr float kMeteringReleaseTimeConstant = 0.325f;

constexpr float kUninitializedValue = -1;

constexpr float kPreDelay = 0.006f;  // seconds

// Release zone values 0 -> 1.
constexpr float kReleaseZone1 = 0.09f;
constexpr float kReleaseZone2 = 0.16f;
constexpr float kReleaseZone3 = 0.42f;
constexpr float kReleaseZone4 = 0.98f;

constexpr float kABase = 0.9999999999999998f * kReleaseZone1 +
                         1.8432219684323923e-16f * kReleaseZone2 -
                         1.9373394351676423e-16f * kReleaseZone3 +
                         8.824516011816245e-18f * kReleaseZone4;
constexpr float kBBase =
    -1.5788320352845888f * kReleaseZone1 + 2.3305837032074286f * kReleaseZone2 -
    0.9141194204840429f * kReleaseZone3 + 0.1623677525612032f * kReleaseZone4;
constexpr float kCBase =
    0.5334142869106424f * kReleaseZone1 - 1.272736789213631f * kReleaseZone2 +
    0.9258856042207512f * kReleaseZone3 - 0.18656310191776226f * kReleaseZone4;
constexpr float kDBase =
    0.08783463138207234f * kReleaseZone1 - 0.1694162967925622f * kReleaseZone2 +
    0.08588057951595272f * kReleaseZone3 - 0.00429891410546283f * kReleaseZone4;
constexpr float kEBase = -0.042416883008123074f * kReleaseZone1 +
                         0.1115693827987602f * kReleaseZone2 -
                         0.09764676325265872f * kReleaseZone3 +
                         0.028494263462021576f * kReleaseZone4;

// Detector release time.
constexpr float kSatReleaseTime = 0.0025f;

// Returns x if x is finite (not NaN or infinite), otherwise returns
// default_value
float EnsureFinite(float x, float default_value) {
  DCHECK(!std::isnan(x));
  DCHECK(!std::isinf(x));
  return std::isfinite(x) ? x : default_value;
}

}  // namespace

DynamicsCompressor::DynamicsCompressor(float sample_rate,
                                       unsigned number_of_channels)
    : number_of_channels_(number_of_channels),
      sample_rate_(sample_rate),
      ratio_(kUninitializedValue),
      slope_(kUninitializedValue),
      linear_threshold_(kUninitializedValue),
      db_threshold_(kUninitializedValue),
      db_knee_(kUninitializedValue),
      knee_threshold_(kUninitializedValue),
      db_knee_threshold_(kUninitializedValue),
      db_yknee_threshold_(kUninitializedValue),
      knee_(kUninitializedValue) {
  SetNumberOfChannels(number_of_channels);
  // Initializes most member variables
  Reset();
  metering_release_k_ =
      static_cast<float>(audio_utilities::DiscreteTimeConstantForSampleRate(
          kMeteringReleaseTimeConstant, sample_rate));
  InitializeParameters();
}

void DynamicsCompressor::Process(const AudioBus* source_bus,
                                 AudioBus* destination_bus,
                                 unsigned frames_to_process) {
  // Though number_of_channels is retrieved from destination_bus, we still name
  // it number_of_channels instead of number_of_destination_channels.  It's
  // because we internally match source_channels's size to destination_bus by
  // channel up/down mix. Thus we need number_of_channels to do the loop work
  // for both source_channels_ and destination_channels_.

  const unsigned number_of_channels = destination_bus->NumberOfChannels();
  const unsigned number_of_source_channels = source_bus->NumberOfChannels();

  DCHECK_EQ(number_of_channels, number_of_channels_);
  DCHECK(number_of_source_channels);

  switch (number_of_channels) {
    case 2:  // stereo
      source_channels_[0] = source_bus->Channel(0)->Data();

      if (number_of_source_channels > 1) {
        source_channels_[1] = source_bus->Channel(1)->Data();
      } else {
        // Simply duplicate mono channel input data to right channel for stereo
        // processing.
        source_channels_[1] = source_channels_[0];
      }

      break;
    default:
      // FIXME : support other number of channels.
      NOTREACHED_IN_MIGRATION();
      destination_bus->Zero();
      return;
  }

  for (unsigned i = 0; i < number_of_channels; ++i) {
    destination_channels_[i] = destination_bus->Channel(i)->MutableData();
  }

  const float db_threshold = ParameterValue(kParamThreshold);
  const float db_knee = ParameterValue(kParamKnee);
  const float ratio = ParameterValue(kParamRatio);
  const float attack_time = ParameterValue(kParamAttack);
  const float release_time = ParameterValue(kParamRelease);

  // Apply compression to the source signal.
  const float** source_channels = source_channels_.get();
  float** destination_channels = destination_channels_.get();

  DCHECK_EQ(pre_delay_buffers_.size(), number_of_channels);

  const float sample_rate = SampleRate();

  const float k = UpdateStaticCurveParameters(db_threshold, db_knee, ratio);

  // Makeup gain with empirical/perceptual tuning.
  const float linear_post_gain = fdlibm::powf(1 / Saturate(1, k), 0.6f);

  // Attack parameters.
  const float attack_frames = std::max(0.001f, attack_time) * sample_rate;

  // Release parameters.
  const float release_frames = sample_rate * release_time;

  const float sat_release_frames = kSatReleaseTime * sample_rate;

  // Create a smooth function which passes through four points.

  // Polynomial of the form
  // y = a + b*x + c*x^2 + d*x^3 + e*x^4;
  // All of these coefficients were derived for 4th order polynomial curve
  // fitting where the y values match the evenly spaced x values as follows:
  // (y1 : x == 0, y2 : x == 1, y3 : x == 2, y4 : x == 3)
  const float a = release_frames * kABase;
  const float b = release_frames * kBBase;
  const float c = release_frames * kCBase;
  const float d = release_frames * kDBase;
  const float e = release_frames * kEBase;

  // x ranges from 0 -> 3       0    1    2   3
  //                           -15  -10  -5   0db

  // y calculates adaptive release frames depending on the amount of
  // compression.

  SetPreDelayTime(kPreDelay);

  constexpr int kNumberOfDivisionFrames = 32;

  const int number_of_divisions = frames_to_process / kNumberOfDivisionFrames;

  unsigned frame_index = 0;
  for (int i = 0; i < number_of_divisions; ++i) {
    // Calculate desired gain

    detector_average_ = EnsureFinite(detector_average_, 1);

    const float desired_gain = detector_average_;

    // Pre-warp so we get desired_gain after sin() warp below.
    const float scaled_desired_gain =
        fdlibm::asinf(desired_gain) / kPiOverTwoFloat;

    // Deal with envelopes

    // envelope_rate is the rate we slew from current compressor level to the
    // desired level.  The exact rate depends on if we're attacking or
    // releasing and by how much.
    float envelope_rate;

    const bool is_releasing = scaled_desired_gain > compressor_gain_;

    // compression_diff_db is the difference between current compression level
    // and the desired level.
    float db_compression_diff;

    if (scaled_desired_gain == 0) {
      db_compression_diff = is_releasing ? -1 : 1;
    } else {
      db_compression_diff = audio_utilities::LinearToDecibels(
          compressor_gain_ / scaled_desired_gain);
    }

    if (is_releasing) {
      // Release mode - db_compression_diff should be negative dB
      db_max_attack_compression_diff_ = -1;

      db_compression_diff = EnsureFinite(db_compression_diff, -1);

      // Adaptive release - higher compression (lower db_compression_diff)
      // releases faster.

      // Contain within range: -12 -> 0 then scale to go from 0 -> 3
      float x = db_compression_diff;
      x = ClampTo(x, -12.0f, 0.0f);
      x = 0.25f * (x + 12);

      // Compute adaptive release curve using 4th order polynomial.
      // Normal values for the polynomial coefficients would create a
      // monotonically increasing function.
      const float x2 = x * x;
      const float x3 = x2 * x;
      const float x4 = x2 * x2;
      const float calc_release_frames = a + b * x + c * x2 + d * x3 + e * x4;

      constexpr float kDbSpacing = 5;
      const float db_per_frame = kDbSpacing / calc_release_frames;

      envelope_rate = audio_utilities::DecibelsToLinear(db_per_frame);
    } else {
      // Attack mode - db_compression_diff should be positive dB

      db_compression_diff = EnsureFinite(db_compression_diff, 1);

      // As long as we're still in attack mode, use a rate based off
      // the largest db_compression_diff we've encountered so far.
      if (db_max_attack_compression_diff_ == -1 ||
          db_max_attack_compression_diff_ < db_compression_diff) {
        db_max_attack_compression_diff_ = db_compression_diff;
      }

      const float db_eff_atten_diff =
          std::max(0.5f, db_max_attack_compression_diff_);

      const float x = 0.25f / db_eff_atten_diff;
      envelope_rate = 1 - fdlibm::powf(x, 1 / attack_frames);
    }

    // Inner loop - calculate shaped power average - apply compression.
    int pre_delay_read_index = pre_delay_read_index_;
    int pre_delay_write_index = pre_delay_write_index_;
    float detector_average = detector_average_;
    float compressor_gain = compressor_gain_;

    int loop_frames = kNumberOfDivisionFrames;
    while (loop_frames--) {
      float compressor_input = 0;

      // Predelay signal, computing compression amount from un-delayed
      // version.
      for (unsigned j = 0; j < number_of_channels; ++j) {
        float* const delay_buffer = pre_delay_buffers_[j]->Data();
        const float undelayed_source = source_channels[j][frame_index];
        delay_buffer[pre_delay_write_index] = undelayed_source;

        const float abs_undelayed_source =
            undelayed_source > 0 ? undelayed_source : -undelayed_source;
        if (compressor_input < abs_undelayed_source) {
          compressor_input = abs_undelayed_source;
        }
      }

      // Calculate shaped power on undelayed input.

      const float scaled_input = compressor_input;
      const float abs_input = scaled_input > 0 ? scaled_input : -scaled_input;

      // Put through shaping curve.
      // This is linear up to the threshold, then enters a "knee" portion
      // followed by the "ratio" portion.  The transition from the threshold
      // to the knee is smooth (1st derivative matched).  The transition
      // from the knee to the ratio portion is smooth (1st derivative
      // matched).
      const float shaped_input = Saturate(abs_input, k);

      const float attenuation =
          abs_input <= 0.0001f ? 1 : shaped_input / abs_input;

      const float db_attenuation =
          std::max(2.0f, -audio_utilities::LinearToDecibels(attenuation));

      const float db_per_frame = db_attenuation / sat_release_frames;

      const float sat_release_rate =
          audio_utilities::DecibelsToLinear(db_per_frame) - 1;

      const bool is_release = (attenuation > detector_average);
      const float rate = is_release ? sat_release_rate : 1;

      detector_average += (attenuation - detector_average) * rate;
      detector_average = std::min(1.0f, detector_average);

      detector_average = EnsureFinite(detector_average, 1);

      // Exponential approach to desired gain.
      if (envelope_rate < 1) {
        // Attack - reduce gain to desired.
        compressor_gain +=
            (scaled_desired_gain - compressor_gain) * envelope_rate;
      } else {
        // Release - exponentially increase gain to 1.0
        compressor_gain *= envelope_rate;
        compressor_gain = std::min(1.0f, compressor_gain);
      }

      // Warp pre-compression gain to smooth out sharp exponential transition
      // points.
      const float post_warp_compressor_gain = static_cast<float>(
          sin(static_cast<double>(kPiOverTwoFloat * compressor_gain)));

      // Calculate total gain using the linear post-gain.
      const float total_gain = linear_post_gain * post_warp_compressor_gain;

      // Calculate metering.
      const float db_real_gain =
          audio_utilities::LinearToDecibels(post_warp_compressor_gain);
      if (db_real_gain < metering_gain_) {
        metering_gain_ = db_real_gain;
      } else {
        metering_gain_ += (db_real_gain - metering_gain_) * metering_release_k_;
      }

      // Apply final gain.
      for (unsigned j = 0; j < number_of_channels; ++j) {
        const float* const delay_buffer = pre_delay_buffers_[j]->Data();
        destination_channels[j][frame_index] =
            delay_buffer[pre_delay_read_index] * total_gain;
      }

      frame_index++;
      pre_delay_read_index =
          (pre_delay_read_index + 1) & kMaxPreDelayFramesMask;
      pre_delay_write_index =
          (pre_delay_write_index + 1) & kMaxPreDelayFramesMask;
    }

    // Locals back to member variables.
    pre_delay_read_index_ = pre_delay_read_index;
    pre_delay_write_index_ = pre_delay_write_index;
    detector_average_ =
        DenormalDisabler::FlushDenormalFloatToZero(detector_average);
    compressor_gain_ =
        DenormalDisabler::FlushDenormalFloatToZero(compressor_gain);
  }

  // Update the compression amount.
  SetParameterValue(kParamReduction, metering_gain_);
}

void DynamicsCompressor::Reset() {
  detector_average_ = 0;
  compressor_gain_ = 1;
  metering_gain_ = 1;

  // Predelay section.
  for (auto& pre_delay_buffer : pre_delay_buffers_) {
    pre_delay_buffer->Zero();
  }

  pre_delay_read_index_ = 0;
  pre_delay_write_index_ = kDefaultPreDelayFrames;

  db_max_attack_compression_diff_ = -1;  // uninitialized state
}

void DynamicsCompressor::SetNumberOfChannels(unsigned number_of_channels) {
  source_channels_ = std::make_unique<const float*[]>(number_of_channels);
  destination_channels_ = std::make_unique<float*[]>(number_of_channels);

  if (pre_delay_buffers_.size() == number_of_channels) {
    return;
  }

  pre_delay_buffers_.clear();
  for (unsigned i = 0; i < number_of_channels; ++i) {
    pre_delay_buffers_.push_back(
        std::make_unique<AudioFloatArray>(kMaxPreDelayFrames));
  }

  number_of_channels_ = number_of_channels;
}

void DynamicsCompressor::SetParameterValue(unsigned parameter_id, float value) {
  DCHECK_LT(parameter_id, static_cast<unsigned>(kParamLast));
  parameters_[parameter_id] = value;
}

float DynamicsCompressor::ParameterValue(unsigned parameter_id) const {
  DCHECK_LT(parameter_id, static_cast<unsigned>(kParamLast));
  return parameters_[parameter_id];
}

float DynamicsCompressor::SampleRate() const {
  return sample_rate_;
}

float DynamicsCompressor::Nyquist() const {
  return sample_rate_ / 2;
}

double DynamicsCompressor::TailTime() const {
  // The reduction value of the compressor is computed from the gain
  // using an exponential filter with a time constant of
  // |kMeteringReleaseTimeConstant|.  We need to keep he compressor
  // running for some time after the inputs go away so that the
  // reduction value approaches 0.  This is a tradeoff between how
  // long we keep the node alive and how close we approach the final
  // value.  A value of 5 to 10 times the time constant is a
  // reasonable trade-off.
  return 5 * kMeteringReleaseTimeConstant;
}

double DynamicsCompressor::LatencyTime() const {
  return last_pre_delay_frames_ / static_cast<double>(SampleRate());
}

bool DynamicsCompressor::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be zero.
  return true;
}

void DynamicsCompressor::InitializeParameters() {
  // Initializes compressor to default values.
  parameters_[kParamThreshold] = -24;   // dB
  parameters_[kParamKnee] = 30;         // dB
  parameters_[kParamRatio] = 12;        // unit-less
  parameters_[kParamAttack] = 0.003f;   // seconds
  parameters_[kParamRelease] = 0.250f;  // seconds
  parameters_[kParamReduction] = 0;     // dB
}

void DynamicsCompressor::SetPreDelayTime(float pre_delay_time) {
  // Re-configure look-ahead section pre-delay if delay time has changed.
  unsigned pre_delay_frames = pre_delay_time * SampleRate();
  if (pre_delay_frames > kMaxPreDelayFrames - 1) {
    pre_delay_frames = kMaxPreDelayFrames - 1;
  }

  if (last_pre_delay_frames_ != pre_delay_frames) {
    last_pre_delay_frames_ = pre_delay_frames;
    for (auto& pre_delay_buffer : pre_delay_buffers_) {
      pre_delay_buffer->Zero();
    }

    pre_delay_read_index_ = 0;
    pre_delay_write_index_ = pre_delay_frames;
  }
}

// Exponential curve for the knee.
// It is 1st derivative matched at linear_threshold_ and asymptotically
// approaches the value linear_threshold_ + 1 / k.
float DynamicsCompressor::KneeCurve(float x, float k) const {
  // Linear up to threshold.
  if (x < linear_threshold_) {
    return x;
  }

  return linear_threshold_ + (1 - static_cast<float>(exp(static_cast<double>(
                                      -k * (x - linear_threshold_))))) /
                                 k;
}

// Full compression curve with constant ratio after knee.
float DynamicsCompressor::Saturate(float x, float k) const {
  if (x < knee_threshold_) {
    return KneeCurve(x, k);
  }
  // Constant ratio after knee.
  const float db_x = audio_utilities::LinearToDecibels(x);
  const float db_y = db_yknee_threshold_ + slope_ * (db_x - db_knee_threshold_);
  return audio_utilities::DecibelsToLinear(db_y);
}

float DynamicsCompressor::KAtSlope(float desired_slope) const {
  const float db_x = db_threshold_ + db_knee_;
  const float x = audio_utilities::DecibelsToLinear(db_x);
  float x2 = 1;
  float db_x2 = 0;

  if (!(x < linear_threshold_)) {
    x2 = x * 1.001;
    db_x2 = audio_utilities::LinearToDecibels(x2);
  }

  // Approximate k given initial values.
  float min_k = 0.1;
  float max_k = 10000;
  float k = 5;

  float slope = 1;
  for (int i = 0; i < 15; ++i) {
    // A high value for k will more quickly asymptotically approach a slope of
    // 0.

    // Approximate 1st derivative with input and output expressed in dB.
    // This slope is equal to the inverse of the compression "ratio".
    // In other words, a compression ratio of 20 would be a slope of 1/20.
    if (!(x < linear_threshold_)) {
      const float db_y = audio_utilities::LinearToDecibels(KneeCurve(x, k));
      const float db_y2 = audio_utilities::LinearToDecibels(KneeCurve(x2, k));
      slope = (db_y2 - db_y) / (db_x2 - db_x);
    }

    if (slope < desired_slope) {
      // k is too high.
      max_k = k;
    } else {
      // k is too low.
      min_k = k;
    }

    // Re-calculate based on geometric mean.
    k = sqrtf(min_k * max_k);
  }

  return k;
}

float DynamicsCompressor::UpdateStaticCurveParameters(float db_threshold,
                                                      float db_knee,
                                                      float ratio) {
  if (db_threshold != db_threshold_ || db_knee != db_knee_ || ratio != ratio_) {
    // Threshold and knee.
    db_threshold_ = db_threshold;
    linear_threshold_ = audio_utilities::DecibelsToLinear(db_threshold);
    db_knee_ = db_knee;

    // Compute knee parameters.
    ratio_ = ratio;
    slope_ = 1 / ratio_;

    const float k = KAtSlope(1 / ratio_);

    db_knee_threshold_ = db_threshold + db_knee;
    knee_threshold_ = audio_utilities::DecibelsToLinear(db_knee_threshold_);

    db_yknee_threshold_ =
        audio_utilities::LinearToDecibels(KneeCurve(knee_threshold_, k));

    knee_ = k;
  }
  return knee_;
}

}  // namespace blink

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

#include "third_party/blink/renderer/platform/audio/dynamics_compressor_kernel.h"

#include <algorithm>
#include <cmath>
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

// Metering hits peaks instantly, but releases this fast (in seconds).
const float kMeteringReleaseTimeConstant = 0.325f;

const float kUninitializedValue = -1;

DynamicsCompressorKernel::DynamicsCompressorKernel(float sample_rate,
                                                   unsigned number_of_channels)
    : sample_rate_(sample_rate),
      last_pre_delay_frames_(kDefaultPreDelayFrames),
      pre_delay_read_index_(0),
      pre_delay_write_index_(kDefaultPreDelayFrames),
      ratio_(kUninitializedValue),
      slope_(kUninitializedValue),
      linear_threshold_(kUninitializedValue),
      db_threshold_(kUninitializedValue),
      db_knee_(kUninitializedValue),
      knee_threshold_(kUninitializedValue),
      knee_threshold_db_(kUninitializedValue),
      yknee_threshold_db_(kUninitializedValue),
      knee_(kUninitializedValue) {
  SetNumberOfChannels(number_of_channels);

  // Initializes most member variables
  Reset();

  metering_release_k_ =
      float{audio_utilities::DiscreteTimeConstantForSampleRate(
          kMeteringReleaseTimeConstant, sample_rate)};
}

void DynamicsCompressorKernel::SetNumberOfChannels(
    unsigned number_of_channels) {
  if (pre_delay_buffers_.size() == number_of_channels)
    return;

  pre_delay_buffers_.clear();
  for (unsigned i = 0; i < number_of_channels; ++i) {
    pre_delay_buffers_.push_back(
        std::make_unique<AudioFloatArray>(kMaxPreDelayFrames));
  }
}

void DynamicsCompressorKernel::SetPreDelayTime(float pre_delay_time) {
  // Re-configure look-ahead section pre-delay if delay time has changed.
  unsigned pre_delay_frames = pre_delay_time * SampleRate();
  if (pre_delay_frames > kMaxPreDelayFrames - 1)
    pre_delay_frames = kMaxPreDelayFrames - 1;

  if (last_pre_delay_frames_ != pre_delay_frames) {
    last_pre_delay_frames_ = pre_delay_frames;
    for (unsigned i = 0; i < pre_delay_buffers_.size(); ++i)
      pre_delay_buffers_[i]->Zero();

    pre_delay_read_index_ = 0;
    pre_delay_write_index_ = pre_delay_frames;
  }
}

// Exponential curve for the knee.
// It is 1st derivative matched at m_linearThreshold and asymptotically
// approaches the value m_linearThreshold + 1 / k.
float DynamicsCompressorKernel::KneeCurve(float x, float k) {
  // Linear up to threshold.
  if (x < linear_threshold_)
    return x;

  return linear_threshold_ + (1 - expf(-k * (x - linear_threshold_))) / k;
}

// Full compression curve with constant ratio after knee.
float DynamicsCompressorKernel::Saturate(float x, float k) {
  float y;

  if (x < knee_threshold_)
    y = KneeCurve(x, k);
  else {
    // Constant ratio after knee.
    float x_db = audio_utilities::LinearToDecibels(x);
    float y_db = yknee_threshold_db_ + slope_ * (x_db - knee_threshold_db_);

    y = audio_utilities::DecibelsToLinear(y_db);
  }

  return y;
}

// Approximate 1st derivative with input and output expressed in dB.
// This slope is equal to the inverse of the compression "ratio".
// In other words, a compression ratio of 20 would be a slope of 1/20.
float DynamicsCompressorKernel::SlopeAt(float x, float k) {
  if (x < linear_threshold_)
    return 1;

  float x2 = x * 1.001;

  float x_db = audio_utilities::LinearToDecibels(x);
  float x2_db = audio_utilities::LinearToDecibels(x2);

  float y_db = audio_utilities::LinearToDecibels(KneeCurve(x, k));
  float y2_db = audio_utilities::LinearToDecibels(KneeCurve(x2, k));

  float m = (y2_db - y_db) / (x2_db - x_db);

  return m;
}

float DynamicsCompressorKernel::KAtSlope(float desired_slope) {
  float x_db = db_threshold_ + db_knee_;
  float x = audio_utilities::DecibelsToLinear(x_db);

  // Approximate k given initial values.
  float min_k = 0.1;
  float max_k = 10000;
  float k = 5;

  for (int i = 0; i < 15; ++i) {
    // A high value for k will more quickly asymptotically approach a slope of
    // 0.
    float slope = SlopeAt(x, k);

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

float DynamicsCompressorKernel::UpdateStaticCurveParameters(float db_threshold,
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

    float k = KAtSlope(1 / ratio_);

    knee_threshold_db_ = db_threshold + db_knee;
    knee_threshold_ = audio_utilities::DecibelsToLinear(knee_threshold_db_);

    yknee_threshold_db_ =
        audio_utilities::LinearToDecibels(KneeCurve(knee_threshold_, k));

    knee_ = k;
  }
  return knee_;
}

void DynamicsCompressorKernel::Process(
    const float* source_channels[],
    float* destination_channels[],
    unsigned number_of_channels,
    unsigned frames_to_process,

    float db_threshold,
    float db_knee,
    float ratio,
    float attack_time,
    float release_time,
    float pre_delay_time,
    float db_post_gain,
    float effect_blend, /* equal power crossfade */

    float release_zone1,
    float release_zone2,
    float release_zone3,
    float release_zone4) {
  DCHECK_EQ(pre_delay_buffers_.size(), number_of_channels);

  float sample_rate = this->SampleRate();

  float dry_mix = 1 - effect_blend;
  float wet_mix = effect_blend;

  float k = UpdateStaticCurveParameters(db_threshold, db_knee, ratio);

  // Makeup gain.
  float full_range_gain = Saturate(1, k);
  float full_range_makeup_gain = 1 / full_range_gain;

  // Empirical/perceptual tuning.
  full_range_makeup_gain = powf(full_range_makeup_gain, 0.6f);

  float master_linear_gain =
      audio_utilities::DecibelsToLinear(db_post_gain) * full_range_makeup_gain;

  // Attack parameters.
  attack_time = std::max(0.001f, attack_time);
  float attack_frames = attack_time * sample_rate;

  // Release parameters.
  float release_frames = sample_rate * release_time;

  // Detector release time.
  float sat_release_time = 0.0025f;
  float sat_release_frames = sat_release_time * sample_rate;

  // Create a smooth function which passes through four points.

  // Polynomial of the form
  // y = a + b*x + c*x^2 + d*x^3 + e*x^4;

  float y1 = release_frames * release_zone1;
  float y2 = release_frames * release_zone2;
  float y3 = release_frames * release_zone3;
  float y4 = release_frames * release_zone4;

  // All of these coefficients were derived for 4th order polynomial curve
  // fitting where the y values match the evenly spaced x values as follows:
  // (y1 : x == 0, y2 : x == 1, y3 : x == 2, y4 : x == 3)
  float a = 0.9999999999999998f * y1 + 1.8432219684323923e-16f * y2 -
            1.9373394351676423e-16f * y3 + 8.824516011816245e-18f * y4;
  float b = -1.5788320352845888f * y1 + 2.3305837032074286f * y2 -
            0.9141194204840429f * y3 + 0.1623677525612032f * y4;
  float c = 0.5334142869106424f * y1 - 1.272736789213631f * y2 +
            0.9258856042207512f * y3 - 0.18656310191776226f * y4;
  float d = 0.08783463138207234f * y1 - 0.1694162967925622f * y2 +
            0.08588057951595272f * y3 - 0.00429891410546283f * y4;
  float e = -0.042416883008123074f * y1 + 0.1115693827987602f * y2 -
            0.09764676325265872f * y3 + 0.028494263462021576f * y4;

  // x ranges from 0 -> 3       0    1    2   3
  //                           -15  -10  -5   0db

  // y calculates adaptive release frames depending on the amount of
  // compression.

  SetPreDelayTime(pre_delay_time);

  const int kNDivisionFrames = 32;

  const int n_divisions = frames_to_process / kNDivisionFrames;

  unsigned frame_index = 0;
  for (int i = 0; i < n_divisions; ++i) {
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Calculate desired gain
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // Fix gremlins.
    if (std::isnan(detector_average_))
      detector_average_ = 1;
    if (std::isinf(detector_average_))
      detector_average_ = 1;

    float desired_gain = detector_average_;

    // Pre-warp so we get desiredGain after sin() warp below.
    float scaled_desired_gain = asinf(desired_gain) / kPiOverTwoFloat;

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Deal with envelopes
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // envelopeRate is the rate we slew from current compressor level to the
    // desired level.  The exact rate depends on if we're attacking or
    // releasing and by how much.
    float envelope_rate;

    bool is_releasing = scaled_desired_gain > compressor_gain_;

    // compressionDiffDb is the difference between current compression level and
    // the desired level.
    float compression_diff_db;

    if (scaled_desired_gain == 0) {
      compression_diff_db = is_releasing ? -1 : 1;
    } else {
      compression_diff_db = audio_utilities::LinearToDecibels(
          compressor_gain_ / scaled_desired_gain);
    }

    if (is_releasing) {
      // Release mode - compressionDiffDb should be negative dB
      max_attack_compression_diff_db_ = -1;

      // Fix gremlins.
      // TODO(rtoy): Replace with a DCHECK so we can figure out how NaN can
      // occur.
      if (std::isnan(compression_diff_db))
        compression_diff_db = -1;
      if (std::isinf(compression_diff_db))
        compression_diff_db = -1;

      // Adaptive release - higher compression (lower compressionDiffDb)
      // releases faster.

      // Contain within range: -12 -> 0 then scale to go from 0 -> 3
      float x = compression_diff_db;
      x = clampTo(x, -12.0f, 0.0f);
      x = 0.25f * (x + 12);

      // Compute adaptive release curve using 4th order polynomial.
      // Normal values for the polynomial coefficients would create a
      // monotonically increasing function.
      float x2 = x * x;
      float x3 = x2 * x;
      float x4 = x2 * x2;
      float calc_release_frames = a + b * x + c * x2 + d * x3 + e * x4;

#define kSpacingDb 5
      float db_per_frame = kSpacingDb / calc_release_frames;

      envelope_rate = audio_utilities::DecibelsToLinear(db_per_frame);
    } else {
      // Attack mode - compressionDiffDb should be positive dB

      // Fix gremlins.
      // TODO(rtoy): Replace with a DCHECK so we can figure out how NaN can
      // occur.
      if (std::isnan(compression_diff_db))
        compression_diff_db = 1;
      if (std::isinf(compression_diff_db))
        compression_diff_db = 1;

      // As long as we're still in attack mode, use a rate based off
      // the largest compressionDiffDb we've encountered so far.
      if (max_attack_compression_diff_db_ == -1 ||
          max_attack_compression_diff_db_ < compression_diff_db)
        max_attack_compression_diff_db_ = compression_diff_db;

      float eff_atten_diff_db = std::max(0.5f, max_attack_compression_diff_db_);

      float x = 0.25f / eff_atten_diff_db;
      envelope_rate = 1 - powf(x, 1 / attack_frames);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Inner loop - calculate shaped power average - apply compression.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    {
      int pre_delay_read_index = pre_delay_read_index_;
      int pre_delay_write_index = pre_delay_write_index_;
      float detector_average = detector_average_;
      float compressor_gain = compressor_gain_;

      int loop_frames = kNDivisionFrames;
      while (loop_frames--) {
        float compressor_input = 0;

        // Predelay signal, computing compression amount from un-delayed
        // version.
        for (unsigned j = 0; j < number_of_channels; ++j) {
          float* delay_buffer = pre_delay_buffers_[j]->Data();
          float undelayed_source = source_channels[j][frame_index];
          delay_buffer[pre_delay_write_index] = undelayed_source;

          float abs_undelayed_source =
              undelayed_source > 0 ? undelayed_source : -undelayed_source;
          if (compressor_input < abs_undelayed_source)
            compressor_input = abs_undelayed_source;
        }

        // Calculate shaped power on undelayed input.

        float scaled_input = compressor_input;
        float abs_input = scaled_input > 0 ? scaled_input : -scaled_input;

        // Put through shaping curve.
        // This is linear up to the threshold, then enters a "knee" portion
        // followed by the "ratio" portion.  The transition from the threshold
        // to the knee is smooth (1st derivative matched).  The transition from
        // the knee to the ratio portion is smooth (1st derivative matched).
        float shaped_input = Saturate(abs_input, k);

        float attenuation = abs_input <= 0.0001f ? 1 : shaped_input / abs_input;

        float attenuation_db = -audio_utilities::LinearToDecibels(attenuation);
        attenuation_db = std::max(2.0f, attenuation_db);

        float db_per_frame = attenuation_db / sat_release_frames;

        float sat_release_rate =
            audio_utilities::DecibelsToLinear(db_per_frame) - 1;

        bool is_release = (attenuation > detector_average);
        float rate = is_release ? sat_release_rate : 1;

        detector_average += (attenuation - detector_average) * rate;
        detector_average = std::min(1.0f, detector_average);

        // Fix gremlins.
        if (std::isnan(detector_average))
          detector_average = 1;
        if (std::isinf(detector_average))
          detector_average = 1;

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
        float post_warp_compressor_gain =
            sinf(kPiOverTwoFloat * compressor_gain);

        // Calculate total gain using master gain and effect blend.
        float total_gain =
            dry_mix + wet_mix * master_linear_gain * post_warp_compressor_gain;

        // Calculate metering.
        float db_real_gain = 20 * std::log10(post_warp_compressor_gain);
        if (db_real_gain < metering_gain_)
          metering_gain_ = db_real_gain;
        else
          metering_gain_ +=
              (db_real_gain - metering_gain_) * metering_release_k_;

        // Apply final gain.
        for (unsigned j = 0; j < number_of_channels; ++j) {
          float* delay_buffer = pre_delay_buffers_[j]->Data();
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
  }
}

void DynamicsCompressorKernel::Reset() {
  detector_average_ = 0;
  compressor_gain_ = 1;
  metering_gain_ = 1;

  // Predelay section.
  for (unsigned i = 0; i < pre_delay_buffers_.size(); ++i)
    pre_delay_buffers_[i]->Zero();

  pre_delay_read_index_ = 0;
  pre_delay_write_index_ = kDefaultPreDelayFrames;

  max_attack_compression_diff_db_ = -1;  // uninitialized state
}

double DynamicsCompressorKernel::TailTime() const {
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

}  // namespace blink

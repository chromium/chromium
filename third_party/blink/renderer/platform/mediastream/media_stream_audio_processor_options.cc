// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

#include <stddef.h>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "third_party/webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/modules/audio_processing/typing_detection.h"

namespace blink {
namespace {

using NoiseSuppression = webrtc::AudioProcessing::Config::NoiseSuppression;

absl::optional<double> GetGainControlCompressionGain(
    const base::Value& config) {
  const base::Value* found = config.FindKey("gain_control_compression_gain_db");
  if (!found)
    return absl::nullopt;
  double gain = found->GetDouble();
  DCHECK_GE(gain, 0.f);
  return gain;
}

absl::optional<double> GetPreAmplifierGainFactor(const base::Value& config) {
  const base::Value* found = config.FindKey("pre_amplifier_fixed_gain_factor");
  if (!found)
    return absl::nullopt;
  double factor = found->GetDouble();
  DCHECK_GE(factor, 1.f);
  return factor;
}

absl::optional<NoiseSuppression::Level> GetNoiseSuppressionLevel(
    const base::Value& config) {
  const base::Value* found = config.FindKey("noise_suppression_level");
  if (!found)
    return absl::nullopt;
  int level = found->GetInt();
  DCHECK_GE(level, static_cast<int>(NoiseSuppression::kLow));
  DCHECK_LE(level, static_cast<int>(NoiseSuppression::kVeryHigh));
  return static_cast<NoiseSuppression::Level>(level);
}

void GetExtraConfigFromJson(
    const std::string& audio_processing_platform_config_json,
    absl::optional<double>* gain_control_compression_gain_db,
    absl::optional<double>* pre_amplifier_fixed_gain_factor,
    absl::optional<NoiseSuppression::Level>* noise_suppression_level) {
  auto config = base::JSONReader::Read(audio_processing_platform_config_json);
  if (!config) {
    LOG(ERROR) << "Failed to parse platform config JSON.";
    return;
  }
  *gain_control_compression_gain_db = GetGainControlCompressionGain(*config);
  *pre_amplifier_fixed_gain_factor = GetPreAmplifierGainFactor(*config);
  *noise_suppression_level = GetNoiseSuppressionLevel(*config);
}

using ClippingPredictor = webrtc::AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor;

ClippingPredictor::Mode GetClippingPredictorMode(
    int clipping_predictor_param_mode) {
  switch (clipping_predictor_param_mode) {
    case 1:
      return ClippingPredictor::Mode::kAdaptiveStepClippingPeakPrediction;
    case 2:
      return ClippingPredictor::Mode::kFixedStepClippingPeakPrediction;
    default:
      return ClippingPredictor::Mode::kClippingEventPrediction;
  }
}

}  // namespace

void AudioProcessingProperties::DisableDefaultProperties() {
  echo_cancellation_type = EchoCancellationType::kEchoCancellationDisabled;
  goog_auto_gain_control = false;
  goog_experimental_echo_cancellation = false;
  goog_typing_noise_detection = false;
  goog_noise_suppression = false;
  goog_experimental_noise_suppression = false;
  goog_highpass_filter = false;
  goog_experimental_auto_gain_control = false;
}

bool AudioProcessingProperties::EchoCancellationEnabled() const {
  return echo_cancellation_type !=
         EchoCancellationType::kEchoCancellationDisabled;
}

bool AudioProcessingProperties::EchoCancellationIsWebRtcProvided() const {
  return echo_cancellation_type == EchoCancellationType::kEchoCancellationAec3;
}

bool AudioProcessingProperties::HasSameReconfigurableSettings(
    const AudioProcessingProperties& other) const {
  return echo_cancellation_type == other.echo_cancellation_type;
}

bool AudioProcessingProperties::HasSameNonReconfigurableSettings(
    const AudioProcessingProperties& other) const {
  return disable_hw_noise_suppression == other.disable_hw_noise_suppression &&
         goog_audio_mirroring == other.goog_audio_mirroring &&
         goog_auto_gain_control == other.goog_auto_gain_control &&
         goog_experimental_echo_cancellation ==
             other.goog_experimental_echo_cancellation &&
         goog_typing_noise_detection == other.goog_typing_noise_detection &&
         goog_noise_suppression == other.goog_noise_suppression &&
         goog_experimental_noise_suppression ==
             other.goog_experimental_noise_suppression &&
         goog_highpass_filter == other.goog_highpass_filter &&
         goog_experimental_auto_gain_control ==
             other.goog_experimental_auto_gain_control;
}

media::AudioProcessingSettings
AudioProcessingProperties::ToAudioProcessingSettings() const {
  media::AudioProcessingSettings out;
  auto convert_type =
      [](EchoCancellationType type) -> media::EchoCancellationType {
    switch (type) {
      case EchoCancellationType::kEchoCancellationDisabled:
        return media::EchoCancellationType::kDisabled;
      case EchoCancellationType::kEchoCancellationAec3:
        return media::EchoCancellationType::kAec3;
      case EchoCancellationType::kEchoCancellationSystem:
        return media::EchoCancellationType::kSystemAec;
    }
  };

  out.echo_cancellation = convert_type(echo_cancellation_type);
  out.noise_suppression =
      goog_noise_suppression ? (goog_experimental_noise_suppression
                                    ? media::NoiseSuppressionType::kExperimental
                                    : media::NoiseSuppressionType::kDefault)
                             : media::NoiseSuppressionType::kDisabled;
  out.automatic_gain_control =
      goog_auto_gain_control
          ? (goog_experimental_auto_gain_control
                 ? media::AutomaticGainControlType::kExperimental
                 : media::AutomaticGainControlType::kDefault)
          : media::AutomaticGainControlType::kDisabled;
  out.high_pass_filter = goog_highpass_filter;
  out.typing_detection = goog_typing_noise_detection;
  return out;
}

void EnableTypingDetection(AudioProcessing::Config* apm_config,
                           webrtc::TypingDetection* typing_detector) {
  apm_config->voice_detection.enabled = true;
  // Configure the update period to 1s (100 * 10ms) in the typing detector.
  typing_detector->SetParameters(0, 0, 0, 0, 0, 100);
}

void StartEchoCancellationDump(AudioProcessing* audio_processing,
                               base::File aec_dump_file,
                               rtc::TaskQueue* worker_queue) {
  DCHECK(aec_dump_file.IsValid());

  FILE* stream = base::FileToFILE(std::move(aec_dump_file), "w");
  if (!stream) {
    LOG(DFATAL) << "Failed to open AEC dump file";
    return;
  }

  auto aec_dump = webrtc::AecDumpFactory::Create(
      stream, -1 /* max_log_size_bytes */, worker_queue);
  if (!aec_dump) {
    LOG(ERROR) << "Failed to start AEC debug recording";
    return;
  }
  audio_processing->AttachAecDump(std::move(aec_dump));
}

void StopEchoCancellationDump(AudioProcessing* audio_processing) {
  audio_processing->DetachAecDump();
}

// TODO(bugs.webrtc.org/7494): Remove unused cases, simplify decision logic.
void ConfigAutomaticGainControl(
    const AudioProcessingProperties& properties,
    const absl::optional<WebRtcHybridAgcParams>& hybrid_agc_params,
    const absl::optional<WebRtcAnalogAgcClippingControlParams>&
        clipping_control_params,
    absl::optional<double> compression_gain_db,
    AudioProcessing::Config& apm_config) {
  // If system level gain control is activated, turn off all gain control
  // functionality in WebRTC.
  if (properties.system_gain_control_activated) {
    apm_config.gain_controller1.enabled = false;
    apm_config.gain_controller2.enabled = false;
    return;
  }

  // The AGC2 fixed digital controller is always enabled when automatic gain
  // control is enabled, the experimental analog AGC is disabled and a
  // compression gain is specified.
  // TODO(bugs.webrtc.org/7494): Remove this option since it makes no sense to
  // run a fixed digital gain after AGC1 adaptive digital.
  const bool use_fixed_digital_agc2 =
      properties.goog_auto_gain_control &&
      !properties.goog_experimental_auto_gain_control &&
      compression_gain_db.has_value();
  const bool use_hybrid_agc = hybrid_agc_params.has_value();
  const bool agc1_enabled = properties.goog_auto_gain_control &&
                            (use_hybrid_agc || !use_fixed_digital_agc2);

  // Configure AGC1.
  if (agc1_enabled) {
    apm_config.gain_controller1.enabled = true;
    // TODO(bugs.webrtc.org/7909): Maybe set mode to kFixedDigital also for IOS.
    apm_config.gain_controller1.mode =
#if defined(OS_ANDROID)
        AudioProcessing::Config::GainController1::Mode::kFixedDigital;
#else
        AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog;
#endif
  }

  // Configure AGC2.
  auto& agc2_config = apm_config.gain_controller2;
  if (properties.goog_experimental_auto_gain_control) {
    // Experimental AGC is enabled. Hybrid AGC may or may not be enabled. Config
    // AGC2 with adaptive mode and the given options, while ignoring
    // `use_fixed_digital_agc2`.
    agc2_config.enabled = use_hybrid_agc;
    agc2_config.fixed_digital.gain_db = 0.0f;
    agc2_config.adaptive_digital.enabled = use_hybrid_agc;
    if (use_hybrid_agc) {
      DCHECK(hybrid_agc_params.has_value());
      // Set AGC2 adaptive digital configuration.
      agc2_config.adaptive_digital.dry_run = hybrid_agc_params->dry_run;
      agc2_config.adaptive_digital.vad_reset_period_ms =
          hybrid_agc_params->vad_reset_period_ms;
      agc2_config.adaptive_digital.adjacent_speech_frames_threshold =
          hybrid_agc_params->adjacent_speech_frames_threshold;
      agc2_config.adaptive_digital.max_gain_change_db_per_second =
          hybrid_agc_params->max_gain_change_db_per_second;
      agc2_config.adaptive_digital.max_output_noise_level_dbfs =
          hybrid_agc_params->max_output_noise_level_dbfs;
      agc2_config.adaptive_digital.sse2_allowed =
          hybrid_agc_params->sse2_allowed;
      agc2_config.adaptive_digital.avx2_allowed =
          hybrid_agc_params->avx2_allowed;
      agc2_config.adaptive_digital.neon_allowed =
          hybrid_agc_params->neon_allowed;
      // Enable AGC1 adaptive digital if AGC2 adaptive digital runs in dry-run
      // mode.
      apm_config.gain_controller1.analog_gain_controller
          .enable_digital_adaptive = agc2_config.adaptive_digital.dry_run;
    } else {
      // Enable AGC1 adaptive digital since AGC2 adaptive digital is disabled.
      apm_config.gain_controller1.analog_gain_controller
          .enable_digital_adaptive = true;
    }

    // When experimental AGC is enabled, we enable clipping control given that
    // 1. `clipping_control_params` is not nullopt,
    // 2. AGC1 is used,
    // 3. AGC1 uses analog gain controller.
    if (apm_config.gain_controller1.enabled &&
        apm_config.gain_controller1.analog_gain_controller.enabled &&
        clipping_control_params.has_value()) {
      auto* const analog_gain_controller =
          &apm_config.gain_controller1.analog_gain_controller;
      analog_gain_controller->clipped_level_step =
          clipping_control_params->clipped_level_step;
      analog_gain_controller->clipped_ratio_threshold =
          clipping_control_params->clipped_ratio_threshold;
      analog_gain_controller->clipped_wait_frames =
          clipping_control_params->clipped_wait_frames;

      auto* const clipping_predictor =
          &analog_gain_controller->clipping_predictor;
      clipping_predictor->enabled = true;
      clipping_predictor->mode =
          GetClippingPredictorMode(clipping_control_params->mode);
      clipping_predictor->window_length =
          clipping_control_params->window_length;
      clipping_predictor->reference_window_length =
          clipping_control_params->reference_window_length;
      clipping_predictor->reference_window_delay =
          clipping_control_params->reference_window_delay;
      clipping_predictor->clipping_threshold =
          clipping_control_params->clipping_threshold;
      clipping_predictor->crest_factor_margin =
          clipping_control_params->crest_factor_margin;
    }
  } else if (use_fixed_digital_agc2) {
    // Experimental AGC is disabled, thus hybrid AGC is disabled. Config AGC2
    // with fixed gain mode.
    agc2_config.enabled = true;
    agc2_config.fixed_digital.gain_db = compression_gain_db.value();
    agc2_config.adaptive_digital.enabled = false;
  }
}

void PopulateApmConfig(
    AudioProcessing::Config* apm_config,
    const AudioProcessingProperties& properties,
    const absl::optional<std::string>& audio_processing_platform_config_json,
    absl::optional<double>* gain_control_compression_gain_db) {
  // TODO(crbug.com/895814): When Chrome uses AGC2, handle all JSON config via
  // webrtc::AudioProcessing::Config.
  absl::optional<double> pre_amplifier_fixed_gain_factor;
  absl::optional<NoiseSuppression::Level> noise_suppression_level;
  if (audio_processing_platform_config_json.has_value()) {
    GetExtraConfigFromJson(audio_processing_platform_config_json.value(),
                           gain_control_compression_gain_db,
                           &pre_amplifier_fixed_gain_factor,
                           &noise_suppression_level);
  }

  apm_config->high_pass_filter.enabled = properties.goog_highpass_filter;

  if (pre_amplifier_fixed_gain_factor.has_value()) {
    apm_config->pre_amplifier.enabled = true;
    apm_config->pre_amplifier.fixed_gain_factor =
        pre_amplifier_fixed_gain_factor.value();
  }

  DCHECK(!(!properties.goog_noise_suppression &&
           properties.system_noise_suppression_activated));
  if (properties.goog_noise_suppression &&
      !properties.system_noise_suppression_activated) {
    apm_config->noise_suppression.enabled = true;
    apm_config->noise_suppression.level =
        noise_suppression_level.value_or(NoiseSuppression::kHigh);
  }

  if (properties.EchoCancellationIsWebRtcProvided()) {
    apm_config->echo_canceller.enabled = true;
#if defined(OS_ANDROID)
    apm_config->echo_canceller.mobile_mode = true;
#else
    apm_config->echo_canceller.mobile_mode = false;
#endif
  }
}

}  // namespace blink

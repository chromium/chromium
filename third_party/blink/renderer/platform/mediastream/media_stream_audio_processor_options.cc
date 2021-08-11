// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

#include <stddef.h>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/webrtc_switches.h"
#include "third_party/webrtc/api/audio/echo_canceller3_config.h"
#include "third_party/webrtc/api/audio/echo_canceller3_config_json.h"
#include "third_party/webrtc/api/audio/echo_canceller3_factory.h"
#include "third_party/webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

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

bool Allow48kHzApmProcessing() {
  return base::FeatureList::IsEnabled(
      ::features::kWebRtcAllow48kHzProcessingOnArm);
}

absl::optional<WebRtcHybridAgcParams> GetWebRtcHybridAgcParams() {
  if (!base::FeatureList::IsEnabled(::features::kWebRtcHybridAgc)) {
    return absl::nullopt;
  }
  return WebRtcHybridAgcParams{
      .dry_run = base::GetFieldTrialParamByFeatureAsBool(
          ::features::kWebRtcHybridAgc, "dry_run", false),
      .vad_reset_period_ms = base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcHybridAgc, "vad_reset_period_ms", 1500),
      .adjacent_speech_frames_threshold =
          base::GetFieldTrialParamByFeatureAsInt(
              ::features::kWebRtcHybridAgc, "adjacent_speech_frames_threshold",
              12),
      .max_gain_change_db_per_second =
          static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
              ::features::kWebRtcHybridAgc, "max_gain_change_db_per_second",
              3)),
      .max_output_noise_level_dbfs =
          static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
              ::features::kWebRtcHybridAgc, "max_output_noise_level_dbfs",
              -50)),
      .sse2_allowed = base::GetFieldTrialParamByFeatureAsBool(
          ::features::kWebRtcHybridAgc, "sse2_allowed", true),
      .avx2_allowed = base::GetFieldTrialParamByFeatureAsBool(
          ::features::kWebRtcHybridAgc, "avx2_allowed", true),
      .neon_allowed = base::GetFieldTrialParamByFeatureAsBool(
          ::features::kWebRtcHybridAgc, "neon_allowed", true)};
}

absl::optional<WebRtcAnalogAgcClippingControlParams>
GetWebRtcAnalogAgcClippingControlParams() {
  if (!base::FeatureList::IsEnabled(
          ::features::kWebRtcAnalogAgcClippingControl)) {
    return absl::nullopt;
  }
  return WebRtcAnalogAgcClippingControlParams{
      .mode = base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl, "mode", 0),
      .window_length = base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl, "window_length", 5),
      .reference_window_length = base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl,
          "reference_window_length", 5),
      .reference_window_delay = base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl, "reference_window_delay",
          5),
      .clipping_threshold =
          static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
              ::features::kWebRtcAnalogAgcClippingControl, "clipping_threshold",
              -1.0)),
      .crest_factor_margin =
          static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
              ::features::kWebRtcAnalogAgcClippingControl,
              "crest_factor_margin", 3.0)),
      .clipped_level_step = base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl, "clipped_level_step",
          15),
      .clipped_ratio_threshold =
          static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
              ::features::kWebRtcAnalogAgcClippingControl,
              "clipped_ratio_threshold", 0.1)),
      .clipped_wait_frames = base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl, "clipped_wait_frames",
          300),
      .use_predicted_step = base::GetFieldTrialParamByFeatureAsBool(
          ::features::kWebRtcAnalogAgcClippingControl, "use_predicted_step",
          true)};
}

}  // namespace

void AudioProcessingProperties::DisableDefaultProperties() {
  echo_cancellation_type = EchoCancellationType::kEchoCancellationDisabled;
  goog_auto_gain_control = false;
  goog_experimental_echo_cancellation = false;
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
         goog_noise_suppression == other.goog_noise_suppression &&
         goog_experimental_noise_suppression ==
             other.goog_experimental_noise_suppression &&
         goog_highpass_filter == other.goog_highpass_filter &&
         goog_experimental_auto_gain_control ==
             other.goog_experimental_auto_gain_control;
}

media::AudioProcessingSettings
AudioProcessingProperties::ToAudioProcessingSettings(
    bool multi_channel_capture_processing) const {
  media::AudioProcessingSettings out;
  out.echo_cancellation =
      echo_cancellation_type == EchoCancellationType::kEchoCancellationAec3;
  out.noise_suppression =
      goog_noise_suppression && !system_noise_suppression_activated;
  // TODO(https://bugs.webrtc.org/5298): Also toggle transient suppression when
  // system effects are activated?
  out.transient_noise_suppression = goog_experimental_noise_suppression;

  out.automatic_gain_control =
      goog_auto_gain_control && !system_gain_control_activated;
  out.experimental_automatic_gain_control = goog_experimental_auto_gain_control;

  out.high_pass_filter = goog_highpass_filter;
  out.multi_channel_capture_processing = multi_channel_capture_processing;
  out.stereo_mirroring = goog_audio_mirroring;
  // TODO(https://crbug.com/1215061): Deprecate this behavior. The constraint is
  // no longer meaningful, but sees significant usage, so some care is required.
  out.force_apm_creation = goog_experimental_echo_cancellation;
  return out;
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
    const media::AudioProcessingSettings& settings,
    const absl::optional<WebRtcHybridAgcParams>& hybrid_agc_params,
    const absl::optional<WebRtcAnalogAgcClippingControlParams>&
        clipping_control_params,
    absl::optional<double> compression_gain_db,
    AudioProcessing::Config& apm_config) {
  // The AGC2 fixed digital controller is always enabled when automatic gain
  // control is enabled, the experimental analog AGC is disabled and a
  // compression gain is specified.
  // TODO(bugs.webrtc.org/7494): Remove this option since it makes no sense to
  // run a fixed digital gain after AGC1 adaptive digital.
  const bool use_fixed_digital_agc2 =
      settings.automatic_gain_control &&
      !settings.experimental_automatic_gain_control &&
      compression_gain_db.has_value();
  const bool use_hybrid_agc = hybrid_agc_params.has_value();
  const bool agc1_enabled = settings.automatic_gain_control &&
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
  if (settings.experimental_automatic_gain_control) {
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
      clipping_predictor->use_predicted_step =
          clipping_control_params->use_predicted_step;
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
    const media::AudioProcessingSettings& settings,
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

  apm_config->high_pass_filter.enabled = settings.high_pass_filter;

  if (pre_amplifier_fixed_gain_factor.has_value()) {
    apm_config->pre_amplifier.enabled = true;
    apm_config->pre_amplifier.fixed_gain_factor =
        pre_amplifier_fixed_gain_factor.value();
  }

  apm_config->noise_suppression.enabled = settings.noise_suppression;
  apm_config->noise_suppression.level =
      noise_suppression_level.value_or(NoiseSuppression::kHigh);

  apm_config->echo_canceller.enabled = settings.echo_cancellation;
#if defined(OS_ANDROID)
  apm_config->echo_canceller.mobile_mode = true;
#else
  apm_config->echo_canceller.mobile_mode = false;
#endif
}

rtc::scoped_refptr<webrtc::AudioProcessing> CreateWebRtcAudioProcessingModule(
    const media::AudioProcessingSettings& settings,
    absl::optional<std::string> audio_processing_platform_config_json,
    absl::optional<int> agc_startup_min_volume) {
  // Experimental options provided at creation.
  // TODO(https://bugs.webrtc.org/5298): Replace with equivalent settings in
  // webrtc::AudioProcessing::Config.
  webrtc::Config config;
  config.Set<webrtc::ExperimentalNs>(
      new webrtc::ExperimentalNs(settings.transient_noise_suppression));

  // TODO(bugs.webrtc.org/7494): Move logic below in ConfigAutomaticGainControl.
  // Retrieve the Hybrid AGC experiment parameters.
  // The hybrid AGC setup, that is AGC1 analog and AGC2 adaptive digital,
  // requires gain control including analog AGC to be active.
  absl::optional<WebRtcHybridAgcParams> hybrid_agc_params;
  absl::optional<WebRtcAnalogAgcClippingControlParams> clipping_control_params;
  if (settings.automatic_gain_control &&
      settings.experimental_automatic_gain_control) {
    hybrid_agc_params = GetWebRtcHybridAgcParams();
    clipping_control_params = GetWebRtcAnalogAgcClippingControlParams();
  }
  // If the analog AGC is enabled, check for overridden config params.
  if (settings.experimental_automatic_gain_control) {
    auto* experimental_agc = new webrtc::ExperimentalAgc(
        /*enabled=*/true, agc_startup_min_volume.value_or(0));
    // Disable the AGC1 adaptive digital controller if the hybrid AGC is enabled
    // and it's not running in dry-run mode.
    experimental_agc->digital_adaptive_disabled =
        hybrid_agc_params.has_value() && !hybrid_agc_params->dry_run;
    config.Set<webrtc::ExperimentalAgc>(experimental_agc);
#if BUILDFLAG(IS_CHROMECAST)
  } else {
    // Do not use the analog controller.
    // This should likely be done on non-Chromecast platforms as well, but care
    // is needed since users may be relying on the current behavior.
    // https://crbug.com/918677#c4
    config.Set<webrtc::ExperimentalAgc>(
        new webrtc::ExperimentalAgc(/*enabled=*/false));
#endif  // BUILDFLAG(IS_CHROMECAST)
  }

  // Create and configure the webrtc::AudioProcessing.
  webrtc::AudioProcessingBuilder ap_builder;
  if (settings.echo_cancellation) {
    webrtc::EchoCanceller3Config aec3_config;
    if (audio_processing_platform_config_json) {
      aec3_config = webrtc::Aec3ConfigFromJsonString(
          *audio_processing_platform_config_json);
      bool config_parameters_already_valid =
          webrtc::EchoCanceller3Config::Validate(&aec3_config);
      RTC_DCHECK(config_parameters_already_valid);
    }

    ap_builder.SetEchoControlFactory(
        std::unique_ptr<webrtc::EchoControlFactory>(
            new webrtc::EchoCanceller3Factory(aec3_config)));
  }
  rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing_module =
      ap_builder.Create(config);

  webrtc::AudioProcessing::Config apm_config =
      audio_processing_module->GetConfig();
  apm_config.pipeline.multi_channel_render = true;
  apm_config.pipeline.multi_channel_capture =
      settings.multi_channel_capture_processing;

  absl::optional<double> gain_control_compression_gain_db;
  PopulateApmConfig(&apm_config, settings,
                    audio_processing_platform_config_json,
                    &gain_control_compression_gain_db);

  // Set up gain control functionalities.
  ConfigAutomaticGainControl(settings, hybrid_agc_params,
                             clipping_control_params,
                             gain_control_compression_gain_db, apm_config);

  // Ensure that 48 kHz APM processing is always active. This overrules the
  // default setting in WebRTC of 32 kHz for ARM platforms.
  if (Allow48kHzApmProcessing()) {
    apm_config.pipeline.maximum_internal_processing_rate = 48000;
  }

  apm_config.residual_echo_detector.enabled = false;
  audio_processing_module->ApplyConfig(apm_config);
  return audio_processing_module;
}

}  // namespace blink

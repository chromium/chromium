// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/helpers.h"

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/webrtc/api/audio/echo_canceller3_config.h"
#include "third_party/webrtc/api/audio/echo_canceller3_factory.h"
#include "third_party/webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {
namespace {

// The analog gain controller is not supported on mobile - i.e., Android, iOS.
#if defined(OS_ANDROID) || defined(OS_IOS)
constexpr bool kAnalogAgcSupported = false;
#else
constexpr bool kAnalogAgcSupported = true;
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

// The analog gain controller can only be disabled on Chromecast.
#if BUILDFLAG(IS_CHROMECAST)
constexpr bool kAllowToDisableAnalogAgc = true;
#else
constexpr bool kAllowToDisableAnalogAgc = false;
#endif  // BUILDFLAG(IS_CHROMECAST)

// AGC1 mode.
using Agc1Mode = webrtc::AudioProcessing::Config::GainController1::Mode;
// TODO(bugs.webrtc.org/7909): Maybe set mode to kFixedDigital also for IOS.
#if defined(OS_ANDROID)
constexpr Agc1Mode kAgc1Mode = Agc1Mode::kFixedDigital;
#else
constexpr Agc1Mode kAgc1Mode = Agc1Mode::kAdaptiveAnalog;
#endif

using Agc1AnalagConfig =
    ::webrtc::AudioProcessing::Config::GainController1::AnalogGainController;

Agc1AnalagConfig::ClippingPredictor::Mode GetClippingPredictorMode(int mode) {
  using Mode = Agc1AnalagConfig::ClippingPredictor::Mode;
  switch (mode) {
    case 1:
      return Mode::kAdaptiveStepClippingPeakPrediction;
    case 2:
      return Mode::kFixedStepClippingPeakPrediction;
    default:
      return Mode::kClippingEventPrediction;
  }
}

bool Allow48kHzApmProcessing() {
  return base::FeatureList::IsEnabled(
      ::features::kWebRtcAllow48kHzProcessingOnArm);
}

absl::optional<int> GetAgcStartupMinVolume() {
  if (!base::FeatureList::IsEnabled(
          ::features::kWebRtcAnalogAgcStartupMinVolume)) {
    return absl::nullopt;
  }
  return base::GetFieldTrialParamByFeatureAsInt(
      ::features::kWebRtcAnalogAgcStartupMinVolume, "volume", 0);
}

void ConfigAgc2AdaptiveDigitalForHybridExperiment(
    ::webrtc::AudioProcessing::Config::GainController2::AdaptiveDigital&
        config) {
  config.dry_run = base::GetFieldTrialParamByFeatureAsBool(
      ::features::kWebRtcHybridAgc, "dry_run", false);
  config.vad_reset_period_ms = base::GetFieldTrialParamByFeatureAsInt(
      ::features::kWebRtcHybridAgc, "vad_reset_period_ms", 1500);
  config.adjacent_speech_frames_threshold =
      base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcHybridAgc, "adjacent_speech_frames_threshold", 12);
  config.max_gain_change_db_per_second =
      static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
          ::features::kWebRtcHybridAgc, "max_gain_change_db_per_second", 3));
  config.max_output_noise_level_dbfs =
      static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
          ::features::kWebRtcHybridAgc, "max_output_noise_level_dbfs", -50));
}

void ConfigAgc1AnalogForClippingControlExperiment(Agc1AnalagConfig& config) {
  config.clipped_level_step = base::GetFieldTrialParamByFeatureAsInt(
      ::features::kWebRtcAnalogAgcClippingControl, "clipped_level_step", 15);
  config.clipped_ratio_threshold =
      static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
          ::features::kWebRtcAnalogAgcClippingControl,
          "clipped_ratio_threshold", 0.1));
  config.clipped_wait_frames = base::GetFieldTrialParamByFeatureAsInt(
      ::features::kWebRtcAnalogAgcClippingControl, "clipped_wait_frames", 300);

  config.clipping_predictor.mode =
      GetClippingPredictorMode(base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl, "mode", 0));
  config.clipping_predictor.window_length =
      base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl, "window_length", 5);
  config.clipping_predictor.reference_window_length =
      base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl,
          "reference_window_length", 5);
  config.clipping_predictor.reference_window_delay =
      base::GetFieldTrialParamByFeatureAsInt(
          ::features::kWebRtcAnalogAgcClippingControl, "reference_window_delay",
          5);
  config.clipping_predictor.clipping_threshold =
      static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
          ::features::kWebRtcAnalogAgcClippingControl, "clipping_threshold",
          -1.0));
  config.clipping_predictor.crest_factor_margin =
      static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
          ::features::kWebRtcAnalogAgcClippingControl, "crest_factor_margin",
          3.0));
  config.clipping_predictor.use_predicted_step =
      base::GetFieldTrialParamByFeatureAsBool(
          ::features::kWebRtcAnalogAgcClippingControl, "use_predicted_step",
          true);
}

// Configures automatic gain control in `apm_config`.
// TODO(bugs.webrtc.org/7494): Clean up once hybrid AGC experiment finalized.
// TODO(bugs.webrtc.org/7494): Remove unused cases, simplify decision logic.
void ConfigAutomaticGainControl(const AudioProcessingSettings& settings,
                                webrtc::AudioProcessing::Config& apm_config) {
  // Configure AGC1.
  if (settings.automatic_gain_control) {
    apm_config.gain_controller1.enabled = true;
    apm_config.gain_controller1.mode = kAgc1Mode;
  }
  auto& agc1_analog_config = apm_config.gain_controller1.analog_gain_controller;
  // Enable and configure AGC1 Analog if needed.
  if (kAnalogAgcSupported && settings.experimental_automatic_gain_control) {
    agc1_analog_config.enabled = true;
    absl::optional<int> startup_min_volume = GetAgcStartupMinVolume();
    // TODO(crbug.com/555577): Do not zero if `startup_min_volume` if no
    // override is specified, instead fall back to the config default value.
    agc1_analog_config.startup_min_volume = startup_min_volume.value_or(0);
  }
  // Disable AGC1 Analog.
  if (kAllowToDisableAnalogAgc &&
      !settings.experimental_automatic_gain_control) {
    // This should likely be done on non-Chromecast platforms as well, but care
    // is needed since users may be relying on the current behavior.
    // https://crbug.com/918677#c4
    agc1_analog_config.enabled = false;
  }

  // TODO(bugs.webrtc.org/7909): Consider returning if `kAnalogAgcSupported` is
  // false since the AGC clipping controller and the Hybrid AGC experiments are
  // meant to run when AGC1 Analog is used.
  if (!settings.automatic_gain_control ||
      !settings.experimental_automatic_gain_control ||
      !agc1_analog_config.enabled) {
    // The settings below only apply when AGC is enabled and when the analog
    // controller is supported and enabled.
    return;
  }

  // AGC1 Analog Clipping Controller experiment.
  if (base::FeatureList::IsEnabled(
          ::features::kWebRtcAnalogAgcClippingControl)) {
    agc1_analog_config.clipping_predictor.enabled = true;
    ConfigAgc1AnalogForClippingControlExperiment(agc1_analog_config);
  }

  // Hybrid AGC feature.
  const bool use_hybrid_agc =
      base::FeatureList::IsEnabled(::features::kWebRtcHybridAgc);
  auto& agc2_config = apm_config.gain_controller2;
  agc2_config.enabled = use_hybrid_agc;
  agc2_config.fixed_digital.gain_db = 0.0f;
  if (use_hybrid_agc) {
    agc2_config.adaptive_digital.enabled = true;
    ConfigAgc2AdaptiveDigitalForHybridExperiment(agc2_config.adaptive_digital);
    // Disable AGC1 adaptive digital unless AGC2 adaptive digital runs in
    // dry-run mode.
    agc1_analog_config.enable_digital_adaptive =
        agc2_config.adaptive_digital.dry_run;
  } else {
    // Use the adaptive digital controller of AGC1 and disable that of AGC2.
    agc1_analog_config.enable_digital_adaptive = true;
    agc2_config.adaptive_digital.enabled = false;
  }
}

}  // namespace

webrtc::StreamConfig CreateStreamConfig(const AudioParameters& parameters) {
  int channels = parameters.channels();

  // Mapping all discrete channel layouts to max two channels assuming that any
  // required channel remix takes place in the native audio layer.
  if (parameters.channel_layout() == CHANNEL_LAYOUT_DISCRETE) {
    channels = std::min(parameters.channels(), 2);
  }
  const int rate = parameters.sample_rate();
  const bool has_keyboard =
      parameters.channel_layout() == CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;

  // webrtc::StreamConfig requires that the keyboard mic channel is not included
  // in the channel count. It may still be used.
  if (has_keyboard)
    channels -= 1;
  return webrtc::StreamConfig(rate, channels, has_keyboard);
}

bool LeftAndRightChannelsAreSymmetric(const AudioBus& audio) {
  if (audio.channels() <= 1) {
    return true;
  }
  return std::equal(audio.channel(0), audio.channel(0) + audio.frames(),
                    audio.channel(1));
}

void StartEchoCancellationDump(webrtc::AudioProcessing* audio_processing,
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

void StopEchoCancellationDump(webrtc::AudioProcessing* audio_processing) {
  audio_processing->DetachAecDump();
}

rtc::scoped_refptr<webrtc::AudioProcessing> CreateWebRtcAudioProcessingModule(
    const AudioProcessingSettings& settings) {
  // Create and configure the webrtc::AudioProcessing.
  webrtc::AudioProcessingBuilder ap_builder;
  if (settings.echo_cancellation) {
    ap_builder.SetEchoControlFactory(
        std::make_unique<webrtc::EchoCanceller3Factory>());
  }
  rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing_module =
      ap_builder.Create();

  webrtc::AudioProcessing::Config apm_config =
      audio_processing_module->GetConfig();
  apm_config.pipeline.multi_channel_render = true;
  apm_config.pipeline.multi_channel_capture =
      settings.multi_channel_capture_processing;
  apm_config.high_pass_filter.enabled = settings.high_pass_filter;
  apm_config.noise_suppression.enabled = settings.noise_suppression;
  apm_config.noise_suppression.level =
      webrtc::AudioProcessing::Config::NoiseSuppression::Level::kHigh;
  apm_config.echo_canceller.enabled = settings.echo_cancellation;
#if defined(OS_ANDROID)
  apm_config.echo_canceller.mobile_mode = true;
#else
  apm_config.echo_canceller.mobile_mode = false;
#endif
  apm_config.residual_echo_detector.enabled = false;

#if !(defined(OS_ANDROID) || defined(OS_IOS))
  apm_config.transient_suppression.enabled =
      settings.transient_noise_suppression;
#endif

  ConfigAutomaticGainControl(settings, apm_config);

  // Ensure that 48 kHz APM processing is always active. This overrules the
  // default setting in WebRTC of 32 kHz for ARM platforms.
  if (Allow48kHzApmProcessing()) {
    apm_config.pipeline.maximum_internal_processing_rate = 48000;
  }

  audio_processing_module->ApplyConfig(apm_config);
  return audio_processing_module;
}
}  // namespace media

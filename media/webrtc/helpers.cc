// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/helpers.h"

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {
namespace {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
constexpr bool kUseHybridAgc = true;
#else
constexpr bool kUseHybridAgc = false;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
constexpr bool kUseClippingController = true;
#else
constexpr bool kUseClippingController = false;
#endif

// The analog gain controller is not supported on mobile - i.e., Android, iOS.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
constexpr bool kAnalogAgcSupported = false;
#else
constexpr bool kAnalogAgcSupported = true;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// The analog gain controller can only be disabled on Chromecast.
//
// TODO(crbug.com/1336055): kAllowToDisableAnalogAgc should be removed once AGC2
// is fully launched.
#if BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
constexpr bool kAllowToDisableAnalogAgc = true;
#else
constexpr bool kAllowToDisableAnalogAgc = false;
#endif

// AGC1 mode.
using Agc1Mode = webrtc::AudioProcessing::Config::GainController1::Mode;
// TODO(bugs.webrtc.org/7909): Maybe set mode to kFixedDigital also for IOS.
#if BUILDFLAG(IS_ANDROID)
constexpr Agc1Mode kAgc1Mode = Agc1Mode::kFixedDigital;
#else
constexpr Agc1Mode kAgc1Mode = Agc1Mode::kAdaptiveAnalog;
#endif

bool DisallowInputVolumeAdjustment() {
  return !base::FeatureList::IsEnabled(
      ::features::kWebRtcAllowInputVolumeAdjustment);
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
  agc1_analog_config.clipping_predictor.enabled = kUseClippingController;

  // Use either the AGC1 or the AGC2 adapative digital gain controller.
  agc1_analog_config.enable_digital_adaptive = !kUseHybridAgc;
  auto& agc2_config = apm_config.gain_controller2;
  agc2_config.enabled = kUseHybridAgc;
  agc2_config.fixed_digital.gain_db = 0.0f;
  agc2_config.adaptive_digital.enabled = kUseHybridAgc;

  if (DisallowInputVolumeAdjustment()) {
    if (agc2_config.enabled) {
      // Completely disable AGC1, which is only used as input volume controller.
      apm_config.gain_controller1.enabled = false;
    } else {
      LOG(WARNING) << "Cannot disable input volume adjustment when AGC2 is "
                      "disabled (not implemented).";
    }
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

  return webrtc::StreamConfig(rate, channels);
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
  if (!settings.NeedWebrtcAudioProcessing())
    return nullptr;

  webrtc::AudioProcessingBuilder ap_builder;

  webrtc::AudioProcessing::Config apm_config;
  apm_config.pipeline.multi_channel_render = true;
  apm_config.pipeline.multi_channel_capture =
      settings.multi_channel_capture_processing;
  apm_config.high_pass_filter.enabled = settings.high_pass_filter;
  apm_config.noise_suppression.enabled = settings.noise_suppression;
  apm_config.noise_suppression.level =
      webrtc::AudioProcessing::Config::NoiseSuppression::Level::kHigh;
  apm_config.echo_canceller.enabled = settings.echo_cancellation;
#if BUILDFLAG(IS_ANDROID)
  apm_config.echo_canceller.mobile_mode = true;
#else
  apm_config.echo_canceller.mobile_mode = false;
#endif
#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
  apm_config.transient_suppression.enabled =
      settings.transient_noise_suppression;
#endif
  ConfigAutomaticGainControl(settings, apm_config);
  return ap_builder.SetConfig(apm_config).Create();
}
}  // namespace media

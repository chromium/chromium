// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/helpers.h"

#include <string>

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

using Agc1Mode = webrtc::AudioProcessing::Config::GainController1::Mode;

using DownmixMethod =
    ::webrtc::AudioProcessing::Config::Pipeline::DownmixMethod;
const base::FeatureParam<DownmixMethod>::Option kDownmixMethodOptions[] = {
    {DownmixMethod::kAverageChannels, "average"},
    {DownmixMethod::kUseFirstChannel, "first"}};
constexpr DownmixMethod kDefaultDownmixMethod =
    webrtc::AudioProcessing::Config::Pipeline{}.capture_downmix_method;
const base::FeatureParam<DownmixMethod> kWebRtcApmDownmixMethodParam = {
    &::features::kWebRtcApmDownmixCaptureAudioMethod, "method",
    kDefaultDownmixMethod, &kDownmixMethodOptions};

void ConfigAutomaticGainControl(const AudioProcessingSettings& settings,
                                webrtc::AudioProcessing::Config& apm_config) {
  if (!settings.automatic_gain_control) {
    // Disable AGC.
    apm_config.gain_controller1.enabled = false;
    apm_config.gain_controller2.enabled = false;
    return;
  }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Use the Hybrid AGC setup, which combines the AGC1 input volume controller
  // and the AGC2 digital adaptive controller.

  // TODO(crbug.com/1375239): Remove `kWebRtcAllowInputVolumeAdjustment` safely.
  if (!base::FeatureList::IsEnabled(
          ::features::kWebRtcAllowInputVolumeAdjustment)) {
    // Entirely disable AGC1 to disable input volume adjustment.
    apm_config.gain_controller1.enabled = false;
  } else {
    // Enable the AGC1 input volume controller.
    apm_config.gain_controller1.enabled = true;
    // TODO(bugs.webrtc.org/14685): Remove next line once `.mode` gets
    // deprecated.
    apm_config.gain_controller1.mode = Agc1Mode::kAdaptiveAnalog;
    apm_config.gain_controller1.analog_gain_controller.enabled = true;
    apm_config.gain_controller1.analog_gain_controller.clipping_predictor
        .enabled = true;
    apm_config.gain_controller1.analog_gain_controller.enable_digital_adaptive =
        false;
  }

  apm_config.gain_controller2.enabled = true;
  apm_config.gain_controller2.fixed_digital.gain_db = 0.0f;
  apm_config.gain_controller2.adaptive_digital.enabled = true;
  apm_config.gain_controller2.input_volume_controller.enabled = false;

  return;
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  // Use AGC1 both as input volume and adaptive digital controller.

  // When AGC1 is used both as input volume and digital gain controller, it is
  // not possible to disable the input volume controller since the digital
  // controller also gets disabled. Hence, `kWebRtcAllowInputVolumeAdjustment`
  // is ignored in this case.
  apm_config.gain_controller1.enabled = true;
  // TODO(bugs.webrtc.org/14685): Remove next line once `.mode` gets deprecated.
  apm_config.gain_controller1.mode = Agc1Mode::kAdaptiveAnalog;
  apm_config.gain_controller1.analog_gain_controller.enabled = true;
  apm_config.gain_controller1.analog_gain_controller.clipping_predictor
      .enabled = true;
  apm_config.gain_controller1.analog_gain_controller.enable_digital_adaptive =
      true;
  apm_config.gain_controller2.enabled = false;
  return;
#elif BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
  // Configure AGC for CAST.
  apm_config.gain_controller1.enabled = true;
  // TODO(bugs.webrtc.org/7494): Switch to AGC2 once APM runtime settings ready.
  apm_config.gain_controller1.mode = Agc1Mode::kFixedDigital;
  apm_config.gain_controller1.analog_gain_controller.enabled = false;
  apm_config.gain_controller2.enabled = false;
  return;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Configure AGC for mobile.
  apm_config.gain_controller1.enabled = false;
  apm_config.gain_controller2.enabled = true;
  apm_config.gain_controller2.fixed_digital.gain_db = 6.0f;
  apm_config.gain_controller2.adaptive_digital.enabled = false;
  return;
#else
#error Undefined AGC configuration. Add a case above for the current platform.
#endif
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
  apm_config.pipeline.capture_downmix_method =
      kWebRtcApmDownmixMethodParam.Get();
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

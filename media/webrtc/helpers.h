// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_HELPERS_H_
#define MEDIA_WEBRTC_HELPERS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_processing.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace rtc {
class TaskQueue;
}  // namespace rtc

namespace media {

COMPONENT_EXPORT(MEDIA_WEBRTC)
webrtc::StreamConfig CreateStreamConfig(const AudioParameters& parameters);

// Tests whether the audio bus data can be treated as upmixed mono audio:
// Returns true if there is at most one channel or if each sample is identical
// in the first two channels.
COMPONENT_EXPORT(MEDIA_WEBRTC)
bool LeftAndRightChannelsAreSymmetric(const AudioBus& audio);

// Creates and configures a webrtc::AudioProcessing audio processing module
// (APM), based on the provided parameters. The optional parameter
// |agc_startup_min_volume| exists for legacy reasons. It is preferred to
// instead use field trials for testing new parameters.
COMPONENT_EXPORT(MEDIA_WEBRTC)
rtc::scoped_refptr<webrtc::AudioProcessing> CreateWebRtcAudioProcessingModule(
    const AudioProcessingSettings& settings,
    absl::optional<int> agc_startup_min_volume);

// Starts the echo cancellation dump in
// |audio_processing|. |worker_queue| must be kept alive until either
// |audio_processing| is destroyed, or
// StopEchoCancellationDump(audio_processing) is called.
COMPONENT_EXPORT(MEDIA_WEBRTC)
void StartEchoCancellationDump(webrtc::AudioProcessing* audio_processing,
                               base::File aec_dump_file,
                               rtc::TaskQueue* worker_queue);

// Stops the echo cancellation dump in |audio_processing|.
// This method has no impact if echo cancellation dump has not been started on
// |audio_processing|.
COMPONENT_EXPORT(MEDIA_WEBRTC)
void StopEchoCancellationDump(webrtc::AudioProcessing* audio_processing);

// WebRTC Hybrid AGC experiment parameters.
struct COMPONENT_EXPORT(MEDIA_WEBRTC) WebRtcHybridAgcParams {
  bool dry_run;
  int vad_reset_period_ms;
  int adjacent_speech_frames_threshold;
  float max_gain_change_db_per_second;
  float max_output_noise_level_dbfs;
  bool sse2_allowed;
  bool avx2_allowed;
  bool neon_allowed;
};

// WebRTC analog AGC clipping control parameters.
struct COMPONENT_EXPORT(MEDIA_WEBRTC) WebRtcAnalogAgcClippingControlParams {
  int mode;
  // Mode can be the following:
  // 0: Clipping event prediction,
  // 1: Adaptive step clipping peak prediction,
  // 2: Fixed step clipping peak prediction.

  int window_length;
  int reference_window_length;
  int reference_window_delay;
  float clipping_threshold;
  float crest_factor_margin;
  int clipped_level_step;
  float clipped_ratio_threshold;
  int clipped_wait_frames;
  bool use_predicted_step;
};

// Configures automatic gain control in `apm_config`. If analog gain control is
// enabled and `hybrid_agc_params` is specified, then the hybrid AGC
// configuration will be used - i.e., analog AGC1 and adaptive digital AGC2.
// TODO(bugs.webrtc.org/7494): Clean up once hybrid AGC experiment finalized.
COMPONENT_EXPORT(MEDIA_WEBRTC)
void ConfigAutomaticGainControl(
    const AudioProcessingSettings& settings,
    const absl::optional<WebRtcHybridAgcParams>& hybrid_agc_params,
    const absl::optional<WebRtcAnalogAgcClippingControlParams>&
        clipping_control_params,
    webrtc::AudioProcessing::Config& apm_config);

}  // namespace media

#endif  // MEDIA_WEBRTC_HELPERS_H_

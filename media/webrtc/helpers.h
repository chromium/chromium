// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_HELPERS_H_
#define MEDIA_WEBRTC_HELPERS_H_

#include <optional>
#include <utility>

#include "base/component_export.h"
#include "base/files/file.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_processing.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {

COMPONENT_EXPORT(MEDIA_WEBRTC)
constexpr int MaxWebRtcAnalogGainLevel() {
  return 255;
}

COMPONENT_EXPORT(MEDIA_WEBRTC)
webrtc::StreamConfig CreateStreamConfig(const AudioParameters& parameters);

// Creates and configures a `webrtc::AudioProcessing` audio processing module
// (APM), based on the provided parameters and on features and field trials.
// `residual_echo_estimator_model` is an optional estimator for the echo
// canceler. If specified, it must outlive the webrtc::AudioProcessing object.
// Returns a pair consisting of:
// - A configured `webrtc::AudioProcessing` instance (or nullptr if
//   `settings.NeedWebrtcAudioProcessing()` is false).
// - A `base::TimeDelta` representing any additional processing delay introduced
//   by the configuration if loopback AEC is enabled. This delay will be zero
//   if loopback AEC is not enabled (default).
COMPONENT_EXPORT(MEDIA_WEBRTC)
std::pair<webrtc::scoped_refptr<webrtc::AudioProcessing>, base::TimeDelta>
CreateWebRtcAudioProcessingModule(
    const AudioProcessingSettings& settings,
    const tflite::FlatBufferModel* residual_echo_estimator_model);

// Starts the echo cancellation dump in
// |audio_processing|. |worker_queue| must be kept alive until either
// |audio_processing| is destroyed, or
// StopEchoCancellationDump(audio_processing) is called.
COMPONENT_EXPORT(MEDIA_WEBRTC)
void StartEchoCancellationDump(webrtc::AudioProcessing* audio_processing,
                               base::File aec_dump_file,
                               webrtc::TaskQueueBase* worker_queue);

// Stops the echo cancellation dump in |audio_processing|.
// This method has no impact if echo cancellation dump has not been started on
// |audio_processing|.
COMPONENT_EXPORT(MEDIA_WEBRTC)
void StopEchoCancellationDump(webrtc::AudioProcessing* audio_processing);

}  // namespace media

#endif  // MEDIA_WEBRTC_HELPERS_H_

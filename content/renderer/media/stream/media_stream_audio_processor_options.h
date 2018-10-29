// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_processing.h"
#include "media/base/audio_point.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/media/base/mediachannel.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/rtc_base/task_queue.h"

namespace webrtc {

class TypingDetection;

}

namespace content {

using webrtc::AudioProcessing;

static constexpr int kAudioProcessingSampleRate =
#if defined(OS_ANDROID)
    AudioProcessing::kSampleRate16kHz;
#else
    AudioProcessing::kSampleRate48kHz;
#endif

// Simple struct with audio-processing properties.
struct CONTENT_EXPORT AudioProcessingProperties {
  enum class EchoCancellationType {
    // Echo cancellation disabled.
    kEchoCancellationDisabled,
    // The WebRTC-provided AEC2 echo canceller.
    kEchoCancellationAec2,
    // The WebRTC-provided AEC3 echo canceller.
    kEchoCancellationAec3,
    // System echo canceller, for example an OS-provided or hardware echo
    // canceller.
    kEchoCancellationSystem
  };

  // Creates an AudioProcessingProperties object with fields initialized to
  // their default values.
  AudioProcessingProperties();
  AudioProcessingProperties(const AudioProcessingProperties& other);
  AudioProcessingProperties& operator=(const AudioProcessingProperties& other);

  // Disables properties that are enabled by default.
  void DisableDefaultProperties();

  // Returns whether echo cancellation is enabled.
  bool EchoCancellationEnabled() const;

  // Returns whether WebRTC-provided echo cancellation is enabled.
  bool EchoCancellationIsWebRtcProvided() const;

  // Converts this struct to an equivalent media::AudioProcessingSettings.
  // TODO(https://crbug.com/878757): Eliminate this class in favor of the media
  // one.
  media::AudioProcessingSettings ToAudioProcessingSettings() const;

  EchoCancellationType echo_cancellation_type =
      EchoCancellationType::kEchoCancellationAec2;
  bool disable_hw_noise_suppression = false;
  bool goog_audio_mirroring = false;
  bool goog_auto_gain_control = true;
  bool goog_experimental_echo_cancellation =
#if defined(OS_ANDROID)
      false;
#else
      true;
#endif
  bool goog_typing_noise_detection = true;
  bool goog_noise_suppression = true;
  bool goog_experimental_noise_suppression = true;
  bool goog_highpass_filter = true;
  bool goog_experimental_auto_gain_control = true;
};

// Enables the echo cancellation in |audio_processing|.
void EnableEchoCancellation(AudioProcessing* audio_processing);

// Enables the noise suppression in |audio_processing|.
void EnableNoiseSuppression(AudioProcessing* audio_processing,
                            webrtc::NoiseSuppression::Level ns_level);

// Enables the typing detection in |audio_processing|.
void EnableTypingDetection(AudioProcessing* audio_processing,
                           webrtc::TypingDetection* typing_detector);

// Starts the echo cancellation dump in
// |audio_processing|. |worker_queue| must be kept alive until either
// |audio_processing| is destroyed, or
// StopEchoCancellationDump(audio_processing) is called.
void StartEchoCancellationDump(AudioProcessing* audio_processing,
                               base::File aec_dump_file,
                               rtc::TaskQueue* worker_queue);

// Stops the echo cancellation dump in |audio_processing|.
// This method has no impact if echo cancellation dump has not been started on
// |audio_processing|.
void StopEchoCancellationDump(AudioProcessing* audio_processing);

// Loads fixed gains for pre-amplifier and gain control from config JSON string.
CONTENT_EXPORT void GetExtraGainConfig(
    const base::Optional<std::string>& audio_processing_platform_config_json,
    base::Optional<double>* pre_amplifier_fixed_gain_factor,
    base::Optional<double>* gain_control_compression_gain_db);

// Enables automatic gain control. If optional |fixed_gain| is set, will set the
// gain control mode to use the fixed gain.
void EnableAutomaticGainControl(AudioProcessing* audio_processing,
                                base::Optional<double> compression_gain_db);

// Enables pre-amplifier with given gain factor if the optional |factor| is set.
void ConfigPreAmplifier(webrtc::AudioProcessing::Config* apm_config,
                        base::Optional<double> fixed_gain_factor);

void GetAudioProcessingStats(
    AudioProcessing* audio_processing,
    webrtc::AudioProcessorInterface::AudioProcessorStats* stats);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

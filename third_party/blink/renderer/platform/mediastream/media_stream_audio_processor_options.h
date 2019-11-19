// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "media/base/audio_point.h"
#include "media/base/audio_processing.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/media/base/media_channel.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/rtc_base/task_queue.h"

namespace webrtc {

class TypingDetection;

}

namespace blink {

using webrtc::AudioProcessing;

static constexpr int kAudioProcessingSampleRate =
#if defined(OS_ANDROID)
    AudioProcessing::kSampleRate16kHz;
#else
    AudioProcessing::kSampleRate48kHz;
#endif

// Simple struct with audio-processing properties.
struct PLATFORM_EXPORT AudioProcessingProperties {
  enum class EchoCancellationType {
    // Echo cancellation disabled.
    kEchoCancellationDisabled,
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

  bool HasSameReconfigurableSettings(
      const AudioProcessingProperties& other) const;

  bool HasSameNonReconfigurableSettings(
      const AudioProcessingProperties& other) const;

  // Converts this struct to an equivalent media::AudioProcessingSettings.
  // TODO(https://crbug.com/878757): Eliminate this class in favor of the media
  // one.
  media::AudioProcessingSettings ToAudioProcessingSettings() const;

  EchoCancellationType echo_cancellation_type =
      EchoCancellationType::kEchoCancellationAec3;
  bool disable_hw_noise_suppression = false;
  bool goog_audio_mirroring = false;
  bool goog_auto_gain_control = true;
  bool goog_experimental_echo_cancellation =
#if defined(OS_ANDROID)
      false;
#else
      true;
#endif
  bool goog_typing_noise_detection = false;
  bool goog_noise_suppression = true;
  bool goog_experimental_noise_suppression = true;
  bool goog_highpass_filter = true;
  bool goog_experimental_auto_gain_control = true;
};

// Enables the typing detection with the given detector.
PLATFORM_EXPORT void EnableTypingDetection(
    AudioProcessing::Config* apm_config,
    webrtc::TypingDetection* typing_detector);

// Starts the echo cancellation dump in
// |audio_processing|. |worker_queue| must be kept alive until either
// |audio_processing| is destroyed, or
// StopEchoCancellationDump(audio_processing) is called.
PLATFORM_EXPORT void StartEchoCancellationDump(
    AudioProcessing* audio_processing,
    base::File aec_dump_file,
    rtc::TaskQueue* worker_queue);

// Stops the echo cancellation dump in |audio_processing|.
// This method has no impact if echo cancellation dump has not been started on
// |audio_processing|.
PLATFORM_EXPORT void StopEchoCancellationDump(
    AudioProcessing* audio_processing);

// Enables automatic gain control with flags and optional configures.
PLATFORM_EXPORT void ConfigAutomaticGainControl(
    AudioProcessing::Config* apm_config,
    bool agc_enabled,
    bool experimental_agc_enabled,
    bool use_hybrid_agc,
    base::Optional<bool> hybrid_agc_use_peaks_not_rms,
    base::Optional<int> hybrid_agc_saturation_margin,
    base::Optional<double> compression_gain_db);

PLATFORM_EXPORT void PopulateApmConfig(
    AudioProcessing::Config* apm_config,
    const AudioProcessingProperties& properties,
    const base::Optional<std::string>& audio_processing_platform_config_json,
    base::Optional<double>* gain_control_compression_gain_db);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

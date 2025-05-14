// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include "build/build_config.h"
#include "media/base/audio_processing.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace blink {

// Returns whether system noise suppression is allowed to be used regardless of
// whether the noise suppression constraint is set, or whether a browser-based
// AEC is active. This is currently the default on at least MacOS but is not
// allowed for ChromeOS or Windows setups. On Windows, the system effects AEC,
// NS and AGC always come as a "package" and it it not possible to enable or
// disable the system NS independently. TODO(crbug.com/417413190): delete if not
// relevant any more.
constexpr bool IsIndependentSystemNsAllowed() {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  return false;
#else
  return true;
#endif
}

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

  enum class VoiceIsolationType {
    // Voice isolation behavior selected by the system is used.
    kVoiceIsolationDefault,
    // Voice isolation is disabled.
    kVoiceIsolationDisabled,
    // Voice isolation is enabled.
    kVoiceIsolationEnabled,
  };

  // Disables properties that are enabled by default.
  void DisableDefaultProperties();

  bool HasSameReconfigurableSettings(
      const AudioProcessingProperties& other) const;

  bool HasSameNonReconfigurableSettings(
      const AudioProcessingProperties& other) const;

  // Converts this struct to an equivalent media::AudioProcessingSettings.
  media::AudioProcessingSettings ToAudioProcessingSettings(
      bool multi_channel_capture_processing) const;

  EchoCancellationType echo_cancellation_type =
      EchoCancellationType::kEchoCancellationAec3;
  // Indicates whether system-level gain control and noise suppression
  // functionalities are active that fill a role comparable to the browser
  // counterparts.
  bool system_gain_control_activated = false;
  bool system_noise_suppression_activated = false;

  bool auto_gain_control = true;
  bool noise_suppression = true;
  VoiceIsolationType voice_isolation =
      VoiceIsolationType::kVoiceIsolationDefault;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

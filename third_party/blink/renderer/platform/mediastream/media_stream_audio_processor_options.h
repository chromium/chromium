// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include <string>

#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

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

  std::string ToString() const;

  EchoCancellationType echo_cancellation_type =
      EchoCancellationType::kEchoCancellationAec3;
  bool auto_gain_control = true;
  bool noise_suppression = true;
  VoiceIsolationType voice_isolation =
      VoiceIsolationType::kVoiceIsolationDefault;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

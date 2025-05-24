// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"

namespace blink {

void AudioProcessingProperties::DisableDefaultProperties() {
  echo_cancellation_type = EchoCancellationType::kEchoCancellationDisabled;
  auto_gain_control = false;
  noise_suppression = false;
  voice_isolation = VoiceIsolationType::kVoiceIsolationDefault;
}

bool AudioProcessingProperties::HasSameReconfigurableSettings(
    const AudioProcessingProperties& other) const {
  return echo_cancellation_type == other.echo_cancellation_type;
}

bool AudioProcessingProperties::HasSameNonReconfigurableSettings(
    const AudioProcessingProperties& other) const {
  return auto_gain_control == other.auto_gain_control &&
         noise_suppression == other.noise_suppression &&
         voice_isolation == other.voice_isolation;
}

std::string AudioProcessingProperties::ToString() const {
  auto aec_to_string = [](EchoCancellationType type) {
    switch (type) {
      case EchoCancellationType::kEchoCancellationDisabled:
        return "disabled";
      case EchoCancellationType::kEchoCancellationAec3:
        return "aec3";
      case EchoCancellationType::kEchoCancellationSystem:
        return "system";
    }
  };
  auto str = base::StringPrintf(
      "echo_cancellation_type: %s, "
      "auto_gain_control: %s, "
      "noise_suppression: %s, ",
      aec_to_string(echo_cancellation_type),
      base::ToString(auto_gain_control).c_str(),
      base::ToString(noise_suppression).c_str());
  return str;
}

}  // namespace blink

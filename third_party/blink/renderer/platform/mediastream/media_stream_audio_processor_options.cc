// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

namespace blink {

void AudioProcessingProperties::DisableDefaultProperties() {
  echo_cancellation_type = EchoCancellationType::kEchoCancellationDisabled;
  auto_gain_control = false;
  noise_suppression = false;
  voice_isolation = VoiceIsolationType::kVoiceIsolationDefault;
}

bool AudioProcessingProperties::EchoCancellationEnabled() const {
  return echo_cancellation_type !=
         EchoCancellationType::kEchoCancellationDisabled;
}

bool AudioProcessingProperties::EchoCancellationIsWebRtcProvided() const {
  return echo_cancellation_type == EchoCancellationType::kEchoCancellationAec3;
}

bool AudioProcessingProperties::HasSameReconfigurableSettings(
    const AudioProcessingProperties& other) const {
  return echo_cancellation_type == other.echo_cancellation_type;
}

bool AudioProcessingProperties::HasSameNonReconfigurableSettings(
    const AudioProcessingProperties& other) const {
  return disable_hw_noise_suppression == other.disable_hw_noise_suppression &&
         auto_gain_control == other.auto_gain_control &&
         noise_suppression == other.noise_suppression &&
         voice_isolation == other.voice_isolation;
}

bool AudioProcessingProperties::GainControlEnabled() const {
  return auto_gain_control;
}

media::AudioProcessingSettings
AudioProcessingProperties::ToAudioProcessingSettings(
    bool multi_channel_capture_processing) const {
  media::AudioProcessingSettings out;
  out.echo_cancellation =
      echo_cancellation_type == EchoCancellationType::kEchoCancellationAec3;
  out.noise_suppression =
      noise_suppression && !system_noise_suppression_activated;

  out.automatic_gain_control =
      auto_gain_control && !system_gain_control_activated;

  out.multi_channel_capture_processing = multi_channel_capture_processing;
  return out;
}
}  // namespace blink

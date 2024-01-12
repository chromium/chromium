// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

namespace blink {

void AudioProcessingProperties::DisableDefaultProperties() {
  echo_cancellation_type = EchoCancellationType::kEchoCancellationDisabled;
  goog_auto_gain_control = false;
  goog_experimental_echo_cancellation = false;
  goog_noise_suppression = false;
  goog_experimental_noise_suppression = false;
  goog_highpass_filter = false;
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
         goog_audio_mirroring == other.goog_audio_mirroring &&
         goog_auto_gain_control == other.goog_auto_gain_control &&
         goog_experimental_echo_cancellation ==
             other.goog_experimental_echo_cancellation &&
         goog_noise_suppression == other.goog_noise_suppression &&
         goog_experimental_noise_suppression ==
             other.goog_experimental_noise_suppression &&
         goog_highpass_filter == other.goog_highpass_filter &&
         voice_isolation == other.voice_isolation;
}

bool AudioProcessingProperties::GainControlEnabled() const {
  return goog_auto_gain_control;
}

media::AudioProcessingSettings
AudioProcessingProperties::ToAudioProcessingSettings(
    bool multi_channel_capture_processing) const {
  media::AudioProcessingSettings out;
  out.echo_cancellation =
      echo_cancellation_type == EchoCancellationType::kEchoCancellationAec3;
  out.noise_suppression =
      goog_noise_suppression && !system_noise_suppression_activated;
  // TODO(https://bugs.webrtc.org/5298): Also toggle transient suppression when
  // system effects are activated?
  out.transient_noise_suppression = goog_experimental_noise_suppression;

  out.automatic_gain_control =
      goog_auto_gain_control && !system_gain_control_activated;

  out.high_pass_filter = goog_highpass_filter;
  out.multi_channel_capture_processing = multi_channel_capture_processing;
  out.stereo_mirroring = goog_audio_mirroring;
  // TODO(https://crbug.com/1215061): Deprecate this behavior. The constraint is
  // no longer meaningful, but sees significant usage, so some care is required.
  out.force_apm_creation = goog_experimental_echo_cancellation;
  return out;
}
}  // namespace blink

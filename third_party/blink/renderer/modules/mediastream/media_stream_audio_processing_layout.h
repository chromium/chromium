// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSING_LAYOUT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSING_LAYOUT_H_

#include <optional>

#include "media/base/audio_processing.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Source of truth for media stream audio processing configuration.
// Based on the requested AudioProcessingProperties and available device
// effects, determines what processing should be done in WebRTC and what
// should be delegated to the platform, as well as configures necessary
// AudioProcessingSettings for WebRTC processing.
class MODULES_EXPORT MediaStreamAudioProcessingLayout {
 public:
  static bool IsIndependentSystemNsAllowedForTests();

  static media::AudioProcessingSettings ComputeWebrtcProcessingSettingsForTests(
      const AudioProcessingProperties& properties,
      int enabled_platform_effects,
      bool multichannel_processing);

  static MediaStreamAudioProcessingLayout None();

  static std::optional<MediaStreamAudioProcessingLayout>
  MaybeMakeForProcessedDisplayCapture(
      const AudioProcessingProperties& properties,
      int channels);

  static MediaStreamAudioProcessingLayout MakeForUnprocessedLocalSource(
      const AudioProcessingProperties& properties,
      int available_platform_effects);

  static MediaStreamAudioProcessingLayout MakeForUnprocessedLocalSourceForTests(
      bool platform_aec,
      int available_platform_effects);

  MediaStreamAudioProcessingLayout(const AudioProcessingProperties& properties,
                                   int available_platform_effects,
                                   int channels);

  const AudioProcessingProperties& properties() const { return properties_; }

  const media::AudioProcessingSettings& webrtc_processing_settings() const {
    return webrtc_processing_settings_;
  }

  int platform_effects() const { return platform_effects_; }

  bool NeedApmInAudioService() const;

  bool NeedWebrtcAudioProcessing() const;

  bool AecIsPlatformProvided() const {
    return echo_canceller_.IsPlatformProvided();
  }

  bool NoiseSuppressionInTandem() const;

  bool AutomaticGainControlInTandem() const;

 private:
  MediaStreamAudioProcessingLayout(const AudioProcessingProperties& properties,
                                   const EchoCanceller& echo_canceller,
                                   int available_platform_effects,
                                   int channels);

  MediaStreamAudioProcessingLayout(
      const AudioProcessingProperties& properties,
      const EchoCanceller& echo_canceller,
      int platform_effects,
      const media::AudioProcessingSettings& webrtc_processing_settings);

  const AudioProcessingProperties properties_;
  const EchoCanceller echo_canceller_;
  const int platform_effects_ = 0;
  const media::AudioProcessingSettings webrtc_processing_settings_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSING_LAYOUT_H_

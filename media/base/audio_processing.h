// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_PROCESSING_H_
#define MEDIA_BASE_AUDIO_PROCESSING_H_

#include <string>

#include "base/files/file.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/base/media_export.h"

namespace media {

enum class AutomaticGainControlType {
  kDisabled,
  kDefault,
  kExperimental,
  kHybridExperimental
};
enum class EchoCancellationType { kDisabled, kAec3, kSystemAec };
enum class NoiseSuppressionType { kDisabled, kDefault, kExperimental };

struct MEDIA_EXPORT AudioProcessingSettings {
  EchoCancellationType echo_cancellation = EchoCancellationType::kDisabled;
  NoiseSuppressionType noise_suppression = NoiseSuppressionType::kDisabled;
  AutomaticGainControlType automatic_gain_control =
      AutomaticGainControlType::kDisabled;
  bool high_pass_filter = false;
  bool typing_detection = false;
  bool stereo_mirroring = false;

  bool operator==(const AudioProcessingSettings& b) const {
    return echo_cancellation == b.echo_cancellation &&
           noise_suppression == b.noise_suppression &&
           automatic_gain_control == b.automatic_gain_control &&
           high_pass_filter == b.high_pass_filter &&
           typing_detection == b.typing_detection &&
           stereo_mirroring == b.stereo_mirroring;
  }

  // Indicates whether WebRTC will be required to perform the audio processing.
  bool requires_apm() const {
    return echo_cancellation == EchoCancellationType::kAec3 ||
           noise_suppression != NoiseSuppressionType::kDisabled ||
           automatic_gain_control != AutomaticGainControlType::kDisabled ||
           high_pass_filter || typing_detection || stereo_mirroring;
  }

  // Stringifies the settings for human-readable logging.
  std::string ToString() const;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_PROCESSING_H_

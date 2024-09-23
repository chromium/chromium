// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_PROCESSING_H_
#define MEDIA_BASE_AUDIO_PROCESSING_H_

#include <string>

#include "build/build_config.h"
#include "media/base/media_export.h"

namespace media {

// This struct specifies software audio processing effects to be applied by
// Chrome to mic capture audio. If system / hardware effects replace effects in
// this struct, then the corresponding parameters in the struct should be
// disabled.
struct MEDIA_EXPORT AudioProcessingSettings {
  bool echo_cancellation = true;
  bool noise_suppression = true;
  // Keytap removal, sometimes called "experimental noise suppression".
  // TODO(https://webrtc.com/7494): Deprecate this setting.
  bool transient_noise_suppression = true;
  bool automatic_gain_control = true;
  bool high_pass_filter = true;
  // Multi-channel is not an individual audio effect, but determines whether the
  // processing algorithms should preserve multi-channel input audio.
  bool multi_channel_capture_processing = true;
  bool stereo_mirroring = false;

  // TODO(crbug.com/40205136): Deprecate this setting.
  // This flag preserves the behavior of the to-be-deprecated flag / constraint
  // |AudioProcessingProperties::goog_experimental_echo_cancellation|: It has no
  // effect on what effects are enabled, but for legacy reasons, it forces APM
  // to be created and used.
  bool force_apm_creation =
#if BUILDFLAG(IS_ANDROID)
      false;
#else
      true;
#endif

  bool operator==(const AudioProcessingSettings& b) const {
    return echo_cancellation == b.echo_cancellation &&
           noise_suppression == b.noise_suppression &&
           transient_noise_suppression == b.transient_noise_suppression &&
           automatic_gain_control == b.automatic_gain_control &&
           high_pass_filter == b.high_pass_filter &&
           multi_channel_capture_processing ==
               b.multi_channel_capture_processing &&
           stereo_mirroring == b.stereo_mirroring &&
           force_apm_creation == b.force_apm_creation;
  }

  bool NeedWebrtcAudioProcessing() const {
    // TODO(crbug.com/40205004): Legacy iOS-specific behavior;
    // reconsider.
#if BUILDFLAG(IS_IOS)
    if (stereo_mirroring)
      return true;
#else
    if (echo_cancellation || automatic_gain_control) {
      return true;
    }
#endif

#if !BUILDFLAG(IS_ANDROID)
    if (force_apm_creation)
      return true;
#endif

    return noise_suppression || high_pass_filter || transient_noise_suppression;
  }

  bool NeedAudioModification() const {
    return NeedWebrtcAudioProcessing() || stereo_mirroring;
  }

  // Deprecated.
  // TODO(crbug.com/40889535): Use `AudioProcessor::NeedsPlayoutReference()`
  // instead.
  bool NeedPlayoutReference() const {
    return echo_cancellation || automatic_gain_control;
  }

  // Stringifies the settings for human-readable logging.
  std::string ToString() const;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_PROCESSING_H_

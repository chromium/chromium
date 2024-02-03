// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_PROCESSOR_CONTROLS_H_
#define MEDIA_BASE_AUDIO_PROCESSOR_CONTROLS_H_

#include <optional>

#include "base/functional/callback.h"
#include "media/base/media_export.h"

namespace media {

// Audio processing metrics that are reported by the audio service.
struct MEDIA_EXPORT AudioProcessingStats {
  std::optional<double> echo_return_loss;
  std::optional<double> echo_return_loss_enhancement;
};

// Interactions with the audio service.
class MEDIA_EXPORT AudioProcessorControls {
 public:
  using GetStatsCB =
      base::OnceCallback<void(const media::AudioProcessingStats& stats)>;

  // Request the latest stats from the audio processor. Stats are returned
  // asynchronously through |callback|.
  virtual void GetStats(GetStatsCB callback) = 0;

  // Set preferred number of microphone channels.
  virtual void SetPreferredNumCaptureChannels(
      int32_t num_preferred_channels) = 0;

 protected:
  virtual ~AudioProcessorControls() = default;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_PROCESSOR_CONTROLS_H_

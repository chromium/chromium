// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Volume range definition and UMA logging utilities for
// `IAudioEndpointVolume::GetVolumeRange()`.
//
// Implementation notes:
// - The minimum supported client is Windows Vista.

#ifndef MEDIA_AUDIO_WIN_VOLUME_RANGE_UTIL_H_
#define MEDIA_AUDIO_WIN_VOLUME_RANGE_UTIL_H_

#include "media/base/media_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// Microphone volume range in decibels.
struct VolumeRange {
  float min_volume_db;  // Range minimum.
  float max_volume_db;  // Range maximum.
  // The range above is divided into N uniform intervals of size
  // `volume_step_db`.
  float volume_step_db;
};

// Logs `Media.Audio.Capture.Win.VolumeRange*` UMA histograms.
// `Media.Audio.Capture.Win.VolumeRangeAvailable` is always logged; the logged
// value reflects whether `range` is specified. When specified, the volume range
// and the number of steps are also logged as follows. `range.min_volume_db` and
// `range.max_volume_db` are logged as `Media.Audio.Capture.Win.VolumeRangeMin2`
// and `Media.Audio.Capture.Win.VolumeRangeMax2` respectively by casting the
// original values to an integer, by clamping in the [-60, 60] range, and by
// adding a 60 dB offset in order to log positive values - required by the UMA
// histograms framework. The value for
// `Media.Audio.Capture.Win.VolumeRangeNumSteps` is computed as
// (`range.max_volume_db` - `range.min_volume_db` / `volume_step_db`) and it is
// clamped to 2000.
MEDIA_EXPORT void LogVolumeRangeUmaHistograms(
    absl::optional<VolumeRange> range);

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_VOLUME_RANGE_UTIL_H_

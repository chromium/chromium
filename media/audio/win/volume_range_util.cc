// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/volume_range_util.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace media {
namespace {

constexpr int CountVolumeRangeIntervals(VolumeRange range) {
  DCHECK_LT(range.min_volume_db, range.max_volume_db);
  DCHECK_GT(range.volume_step_db, 0.0f);
  return (range.max_volume_db - range.min_volume_db) / range.volume_step_db;
}

}  // namespace

void LogVolumeRangeUmaHistograms(absl::optional<VolumeRange> range) {
  base::UmaHistogramBoolean("Media.Audio.Capture.Win.VolumeRangeAvailable",
                            range.has_value());
  if (!range.has_value()) {
    return;
  }
  // Count the number of steps before the range is modified.
  constexpr int kMaxVolumeSteps = 2000;
  const int num_steps =
      std::min(CountVolumeRangeIntervals(*range), kMaxVolumeSteps);
  // Approximate `range` to an integer interval since UMA histograms do not log
  // floating point values.
  range->min_volume_db = std::floor(range->min_volume_db);
  range->max_volume_db = std::ceil(range->max_volume_db);
  // `range` is expected to be fully included in [`kMinDb`, `kMaxDb`].
  constexpr int kMinDb = -60;
  constexpr int kMaxDb = 60;
  static_assert(kMinDb < kMaxDb, "");
  // UMA histograms only accept non-negative values; however, a volume range is
  // expected to include negative values (if `kMinDb` is negative). Therefore,
  // `max(0, -kMinDb)` is added as offset.
  constexpr int kVolumeOffsetDb = kMinDb < 0 ? -kMinDb : 0;
  range->min_volume_db += kVolumeOffsetDb;
  range->max_volume_db += kVolumeOffsetDb;
  // Log the modified volume range.
  constexpr int kExclusiveMaxVolumeDb = kMaxDb + kVolumeOffsetDb + 1;
  DCHECK_LE(range->min_volume_db, range->max_volume_db);
  base::UmaHistogramExactLinear("Media.Audio.Capture.Win.VolumeRangeMin2",
                                static_cast<int>(range->min_volume_db),
                                kExclusiveMaxVolumeDb);
  DCHECK_GE(range->max_volume_db, 0);
  base::UmaHistogramExactLinear("Media.Audio.Capture.Win.VolumeRangeMax2",
                                static_cast<int>(range->max_volume_db),
                                kExclusiveMaxVolumeDb);
  // Log the number of volume range steps.
  DCHECK_GT(num_steps, 0);
  constexpr int kStepsNumBuckets = 100;
  base::UmaHistogramCustomCounts("Media.Audio.Capture.Win.VolumeRangeNumSteps",
                                 /*sample=*/num_steps,
                                 /*min=*/1, /*max=*/kMaxVolumeSteps,
                                 kStepsNumBuckets);
}

}  // namespace media

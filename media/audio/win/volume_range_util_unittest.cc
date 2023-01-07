// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/volume_range_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {
namespace {

using base::Bucket;
using testing::ElementsAre;

constexpr int kClampedMaxVolumeDb = 60;
constexpr int kVolumeOffsetDb = 60;
constexpr int kMaxNumSteps = 2000;

constexpr VolumeRange kValidVolumeRange{.min_volume_db = -17.0f,
                                        .max_volume_db = 20.0f,
                                        .volume_step_db = 0.03125f};

constexpr int ComputeNumSteps(VolumeRange range) {
  return (range.max_volume_db - range.min_volume_db) / range.volume_step_db;
}

TEST(VolumeRangeUtilWin, LogUnavailableVolumeRange) {
  base::HistogramTester tester;
  LogVolumeRangeUmaHistograms(/*range=*/absl::nullopt);
  EXPECT_THAT(
      tester.GetAllSamples("Media.Audio.Capture.Win.VolumeRangeAvailable"),
      ElementsAre(Bucket(false, 1)));
}

TEST(VolumeRangeUtilWin, LogAvailableVolumeRange) {
  base::HistogramTester tester;
  LogVolumeRangeUmaHistograms(kValidVolumeRange);
  EXPECT_THAT(
      tester.GetAllSamples("Media.Audio.Capture.Win.VolumeRangeAvailable"),
      ElementsAre(Bucket(true, 1)));
}

TEST(VolumeRangeUtilWin, VolumeRangeMinMaxOffsetAdded) {
  base::HistogramTester tester;
  LogVolumeRangeUmaHistograms(kValidVolumeRange);
  tester.ExpectUniqueSample(
      "Media.Audio.Capture.Win.VolumeRangeMin2",
      static_cast<int>(kValidVolumeRange.min_volume_db + kVolumeOffsetDb), 1);
  tester.ExpectUniqueSample(
      "Media.Audio.Capture.Win.VolumeRangeMax2",
      static_cast<int>(kValidVolumeRange.max_volume_db + kVolumeOffsetDb), 1);
}

TEST(VolumeRangeUtilWin, VolumeRangeMinMaxRoundedOff) {
  base::HistogramTester tester;
  constexpr VolumeRange kVolumeRange{.min_volume_db = -1.123f,
                                     .max_volume_db = 1.123f,
                                     .volume_step_db = 2.246};
  LogVolumeRangeUmaHistograms(kVolumeRange);
  tester.ExpectUniqueSample("Media.Audio.Capture.Win.VolumeRangeMin2",
                            -2 + kVolumeOffsetDb, 1);
  tester.ExpectUniqueSample("Media.Audio.Capture.Win.VolumeRangeMax2",
                            2 + kVolumeOffsetDb, 1);
}

// Checks that volume values outside of the expected range fall into the first
// and the last (a.k.a., overflow) buckets respectively.
TEST(VolumeRangeUtilWin, VolumeRangeMinMaxClamped) {
  base::HistogramTester tester;
  constexpr VolumeRange kVolumeRange{.min_volume_db = -10000.0f,
                                     .max_volume_db = 10000.0f,
                                     .volume_step_db = 1.0f};
  LogVolumeRangeUmaHistograms(kVolumeRange);
  tester.ExpectUniqueSample("Media.Audio.Capture.Win.VolumeRangeMin2", 0, 1);
  constexpr int kOverflowBucket = kClampedMaxVolumeDb + kVolumeOffsetDb + 1;
  tester.ExpectUniqueSample("Media.Audio.Capture.Win.VolumeRangeMax2",
                            kOverflowBucket, 1);
}

TEST(VolumeRangeUtilWin, VolumeRangeNumSteps) {
  base::HistogramTester tester;
  LogVolumeRangeUmaHistograms(kValidVolumeRange);
  tester.ExpectUniqueSample("Media.Audio.Capture.Win.VolumeRangeNumSteps",
                            ComputeNumSteps(kValidVolumeRange), 1);
}

TEST(VolumeRangeUtilWin, VolumeRangeNumStepsClamped) {
  base::HistogramTester tester;
  constexpr VolumeRange kVolumeRange{.min_volume_db = -17.0f,
                                     .max_volume_db = 20.0f,
                                     .volume_step_db = 0.00925f};
  LogVolumeRangeUmaHistograms(kVolumeRange);
  constexpr int kNumSteps = ComputeNumSteps(kVolumeRange);
  static_assert(kNumSteps > kMaxNumSteps, "");
  tester.ExpectUniqueSample("Media.Audio.Capture.Win.VolumeRangeNumSteps",
                            kMaxNumSteps, 1);
}

}  // namespace
}  // namespace media

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/media_switches.h"
#include "media/capabilities/webrtc_video_stats_db.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/services/webrtc_video_perf_history.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::IsNull;

namespace media {
namespace {

// Aliases for readability.
constexpr bool kIsDecode = true;
constexpr bool kIsEncode = false;
constexpr bool kHardware = true;
constexpr bool kSoftware = false;
constexpr bool kIsSmooth = true;
constexpr bool kIsNotSmooth = false;
constexpr VideoCodecProfile kKnownProfile = VP9PROFILE_PROFILE0;
constexpr int32_t kPixelsHd = 1280 * 720;
constexpr int32_t kPixelsFullHd = 1920 * 1080;
constexpr int32_t kPixels4K = 3840 * 2160;
constexpr int kFramesProcessed = 1000;
constexpr int kKeyFramesProcessed = 4;

}  // namespace

class FakeWebrtcVideoStatsDB : public WebrtcVideoStatsDB {
 public:
  FakeWebrtcVideoStatsDB() = default;
  ~FakeWebrtcVideoStatsDB() override = default;

  // Call CompleteInitialize(...) to run `init_cb` callback.
  void Initialize(base::OnceCallback<void(bool)> init_cb) override {
    EXPECT_FALSE(!!pendnding_init_cb_);
    pendnding_init_cb_ = std::move(init_cb);
  }

  // Completes fake initialization, running `init_cb` with the supplied value
  // for success.
  void CompleteInitialize(bool success) {
    DVLOG(2) << __func__ << " running with success = " << success;
    EXPECT_TRUE(!!pendnding_init_cb_);
    std::move(pendnding_init_cb_).Run(success);
  }

  // Simple hooks to fail the next calls to AppendDecodeStats() and
  // GetDecodeStats(). Will be reset to false after the call.
  void set_fail_next_append(bool fail_append) {
    fail_next_append_ = fail_append;
  }
  void set_fail_next_get(bool fail_get) { fail_next_get_ = fail_get; }

  void AppendVideoStats(const VideoDescKey& key,
                        const VideoStats& new_stats,
                        AppendVideoStatsCB append_done_cb) override {
    if (fail_next_append_) {
      fail_next_append_ = false;
      std::move(append_done_cb).Run(false);
      return;
    }

    std::string key_str = key.Serialize();
    if (entries_.find(key_str) == entries_.end()) {
      entries_.emplace(std::make_pair(key_str, VideoStatsEntry{new_stats}));
    } else {
      VideoStatsEntry& known_entry = entries_.at(key_str);
      known_entry.insert(
          known_entry.begin(),
          VideoStats(new_stats.frames_processed, new_stats.key_frames_processed,
                     new_stats.p99_processing_time_ms));
    }

    std::move(append_done_cb).Run(true);
  }

  void GetVideoStats(const VideoDescKey& key,
                     GetVideoStatsCB get_stats_cb) override {
    if (fail_next_get_) {
      fail_next_get_ = false;
      std::move(get_stats_cb).Run(false, std::nullopt);
      return;
    }

    auto entry_it = entries_.find(key.Serialize());
    if (entry_it == entries_.end()) {
      std::move(get_stats_cb).Run(true, std::nullopt);
    } else {
      std::move(get_stats_cb).Run(true, entry_it->second);
    }
  }

  void GetVideoStatsCollection(
      const VideoDescKey& key,
      GetVideoStatsCollectionCB get_stats_cb) override {
    if (fail_next_get_) {
      fail_next_get_ = false;
      std::move(get_stats_cb).Run(false, std::nullopt);
      return;
    }

    WebrtcVideoStatsDB::VideoStatsCollection collection;
    std::string key_filter = key.SerializeWithoutPixels();
    for (auto const& [str, video_stats_entry] : entries_) {
      if (str.rfind(key_filter, 0) == 0) {
        std::optional<int> pixels = VideoDescKey::ParsePixelsFromKey(str);
        if (pixels) {
          collection.insert({*pixels, std::move(video_stats_entry)});
        }
      }
    }
    if (collection.empty()) {
      std::move(get_stats_cb).Run(true, std::nullopt);
    } else {
      std::move(get_stats_cb).Run(true, std::move(collection));
    }
  }

  void ClearStats(base::OnceClosure clear_done_cb) override {
    entries_.clear();
    std::move(clear_done_cb).Run();
  }

 private:
  bool fail_next_append_ = false;
  bool fail_next_get_ = false;

  std::map<std::string, VideoStatsEntry> entries_;

  base::OnceCallback<void(bool)> pendnding_init_cb_;
};

class WebrtcVideoPerfHistoryTest : public testing::Test {
 public:
  void SetUp() override {
    perf_history_ = std::make_unique<WebrtcVideoPerfHistory>(
        std::make_unique<FakeWebrtcVideoStatsDB>());
  }

  void TearDown() override { perf_history_.reset(); }

  FakeWebrtcVideoStatsDB* GetFakeDB() {
    return static_cast<FakeWebrtcVideoStatsDB*>(perf_history_->db_.get());
  }

  void PreInitializeDB(bool success) {
    // Invoke private method to start initialization. Usually invoked by first
    // API call requiring DB access.
    perf_history_->InitDatabase();
    // Complete initialization by firing callback from our fake DB.
    GetFakeDB()->CompleteInitialize(success);
  }

  // Tests may set this as the callback for WebrtcVideoPerfHistory::GetPerfInfo
  // to check the results of the call.
  MOCK_METHOD1(MockGetPerfInfoCB, void(bool is_smooth));

  // Tests may set this as the callback for
  // WebrtcVideoPerfHistory::SavePerfRecord to verify that the data is stored.
  MOCK_METHOD0(MockSaveDoneCB, void());

  // Tests should EXPECT_CALL this method prior to ClearHistory() to know that
  // the operation has completed.
  MOCK_METHOD0(MockOnClearedHistory, void());

  MOCK_METHOD1(MockGetWebrtcVideoStatsDBCB, void(WebrtcVideoStatsDB* db));

  void SavePerfRecord(mojom::WebrtcPredictionFeatures features,
                      mojom::WebrtcVideoStats video_stats,
                      bool expect_callback) {
    base::OnceClosure save_done_cb = base::BindOnce(
        &WebrtcVideoPerfHistoryTest::MockSaveDoneCB, base::Unretained(this));
    if (expect_callback) {
      EXPECT_CALL(*this, MockSaveDoneCB());
    }
    perf_history_->GetSaveCallback().Run(features, video_stats,
                                         std::move(save_done_cb));
  }
  void SavePerfRecord(mojom::WebrtcPredictionFeatures features,
                      mojom::WebrtcVideoStats video_stats) {
    SavePerfRecord(std::move(features), std::move(video_stats),
                   /*expect_callback=*/true);
  }

 protected:
  using VideoDescKey = WebrtcVideoStatsDB::VideoDescKey;
  using VideoStatsEntry = WebrtcVideoStatsDB::VideoStatsEntry;
  using Features = media::mojom::WebrtcPredictionFeatures;
  using VideoStats = media::mojom::WebrtcVideoStats;

  float GetSmoothnessThreshold(bool is_decode) {
    return WebrtcVideoPerfHistory::GetSmoothnessThreshold(is_decode);
  }

  float GetSmoothDecisionRatioThreshold() {
    return WebrtcVideoPerfHistory::GetSmoothDecisionRatioThreshold();
  }

  base::test::TaskEnvironment task_environment_;

  // The WebrtcVideoStatsReporter being tested.
  std::unique_ptr<WebrtcVideoPerfHistory> perf_history_;
};

struct WebrtcPerfHistoryTestParams {
  const bool defer_initialize;
};

// When bool param is true, tests should wait until the end to run
// GetFakeDB()->CompleteInitialize(). Otherwise run PreInitializeDB() at the
// test start.
class WebrtcVideoPerfHistoryParamTest
    : public testing::WithParamInterface<WebrtcPerfHistoryTestParams>,
      public WebrtcVideoPerfHistoryTest {};

TEST_P(WebrtcVideoPerfHistoryParamTest, GetPerfInfo) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  // First add 2 records to the history. First record with HD resolution and
  // second with Full HD resolution.
  constexpr float kP99ProcessingTimeMsSmoothAt60Hz = 12.0f;
  constexpr float kP99ProcessingTimeMsSmoothAt30Hz = 24.0f;

  // Add the entries.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt60Hz));
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt30Hz));

  // Verify perf history returns is_smooth = true for HD at 30 and 60 fps.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
      /*frames_per_second=*/30,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = true for Full HD at 30 fps and
  // is_smooth = false for Full HD at 60 fps.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      /*frames_per_second=*/30,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history optimistically returns is_smooth = true when no entry
  // can be found with the given configuration.
  constexpr VideoCodecProfile kUnknownProfile = VP9PROFILE_PROFILE2;
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kUnknownProfile, kPixelsFullHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

// Test to override the smoothness threshold by setting the corresponding field
// trial parameter.
TEST_P(WebrtcVideoPerfHistoryParamTest,
       GetPerfInfoSmoothnessThresholdOverride) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<base::FieldTrialList> field_trial_list;

  float previousSmoothnessThresholdDecode = GetSmoothnessThreshold(kIsDecode);
  float previousSmoothnessThresholdEncode = GetSmoothnessThreshold(kIsEncode);
  // The 99th percentile decode time must be lower than 50% of 1/fps to be
  // considered smooth.
  float kNewSmoothnessThresholdDecode = 0.5f;
  // The 99th percentile encode time must be lower than 120% of 1/fps to be
  // considered smooth.
  float kNewSmoothnessThresholdEncode = 1.2f;

  // Override field trial.
  base::FieldTrialParams field_trial_params;
  field_trial_params["smoothness_threshold_decode"] =
      base::NumberToString(kNewSmoothnessThresholdDecode);
  field_trial_params["smoothness_threshold_encode"] =
      base::NumberToString(kNewSmoothnessThresholdEncode);
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kWebrtcMediaCapabilitiesParameters, field_trial_params);
  EXPECT_NE(kNewSmoothnessThresholdDecode, previousSmoothnessThresholdDecode);
  EXPECT_NE(kNewSmoothnessThresholdEncode, previousSmoothnessThresholdEncode);
  EXPECT_EQ(kNewSmoothnessThresholdDecode, GetSmoothnessThreshold(kIsDecode));
  EXPECT_EQ(kNewSmoothnessThresholdEncode, GetSmoothnessThreshold(kIsEncode));

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  constexpr int kFramesPerSecond = 30;

  // Add a Full HD decode entry just above the new threshold.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            /*p99_processing_time_ms=*/1.01 *
                                kNewSmoothnessThresholdDecode * 1000.0 /
                                kFramesPerSecond));
  // Add an HD decode entry just below the new threshold.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            /*p99_processing_time_ms=*/0.99 *
                                kNewSmoothnessThresholdDecode * 1000.0 /
                                kFramesPerSecond));

  // Verify that Full HD is not smooth and HD is smooth.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      kFramesPerSecond,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
      kFramesPerSecond,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Add a Full HD encdde entry just above the new threshold.
  SavePerfRecord(Features(kIsEncode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            /*p99_processing_time_ms=*/1.01 *
                                kNewSmoothnessThresholdEncode * 1000.0 /
                                kFramesPerSecond));
  // Add an HD encode entry just below the new threshold.
  SavePerfRecord(Features(kIsEncode, kKnownProfile, kPixelsHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            /*p99_processing_time_ms=*/0.99 *
                                kNewSmoothnessThresholdEncode * 1000.0 /
                                kFramesPerSecond));

  // Verify that Full HD is not smooth and HD is smooth.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsEncode, kKnownProfile, kPixelsFullHd, kSoftware),
      kFramesPerSecond,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsEncode, kKnownProfile, kPixelsHd, kSoftware),
      kFramesPerSecond,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

// Verify that the combined smoothness prediction is correct in the case that
// the database contains entries with mixed smoothness predicitions.
TEST_P(WebrtcVideoPerfHistoryParamTest, GetPerfInfoCombinedPrediction) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  constexpr float kP99ProcessingTimeMsSmoothAt60Hz = 12.0f;
  constexpr float kP99ProcessingTimeMsSmoothAt30Hz = 24.0f;

  // The SmoothDecisionRatioThreshold determines the minimum ratio of smooth
  // entries for the combined prediction to be considered smooth.
  constexpr int kNumberOfEntries = 10;
  const float kSmoothDecisionRatioThreshold = GetSmoothDecisionRatioThreshold();
  const int kNotSmoothEntries =
      kNumberOfEntries * (1 - kSmoothDecisionRatioThreshold);
  const int kSmoothEntries = kNumberOfEntries - kNotSmoothEntries;

  // Add `kNotSmoothEntries` that are not smooth at 60 fps.
  for (int i = 0; i < kNotSmoothEntries; ++i) {
    // Add the entries.
    SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                   VideoStats(kFramesProcessed, kKeyFramesProcessed,
                              kP99ProcessingTimeMsSmoothAt30Hz));
  }
  // Verify perf history returns is_smooth = false at 60 fps.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Add `kSmoothEntries` - 1 that are smooth at 60 fps and verify that
  // is_smooth = false still.
  for (int i = 0; i < kSmoothEntries - 1; ++i) {
    // Add the entries.
    SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                   VideoStats(kFramesProcessed, kKeyFramesProcessed,
                              kP99ProcessingTimeMsSmoothAt60Hz));
  }
  // Verify perf history returns is_smooth = false at 60 fps.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Add one more entry and verify that is_smooth = true now.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt60Hz));
  // Verify perf history returns is_smooth = true at 60 fps.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Add 5 more entries that are smooth at 60 fps and verify that is_smooth =
  // true still.
  for (int i = 0; i < 5 - 1; ++i) {
    // Add the entries.
    SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                   VideoStats(kFramesProcessed, kKeyFramesProcessed,
                              kP99ProcessingTimeMsSmoothAt60Hz));
  }
  // Verify perf history returns is_smooth = true at 60 fps.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

// Test to override smooth decision ratio threshold by setting the corresponding
// field trial parameter.
TEST_P(WebrtcVideoPerfHistoryParamTest,
       GetPerfInfoSmoothDecisionRatioThresholdOverride) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<base::FieldTrialList> field_trial_list;
  // Override the smooth decision ratio threshold through the field trial.
  float previousSmoothDecisionRatioThreshold =
      GetSmoothDecisionRatioThreshold();
  // The ratio of smooth entries must be greater than the threshold for the
  // combined stats to be considered smooth.
  float kNewSmoothDecisionRatioThreshold = 0.7f;

  base::FieldTrialParams field_trial_params;
  field_trial_params["smooth_decision_ratio_threshold"] =
      base::NumberToString(kNewSmoothDecisionRatioThreshold);
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kWebrtcMediaCapabilitiesParameters, field_trial_params);
  EXPECT_NE(kNewSmoothDecisionRatioThreshold,
            previousSmoothDecisionRatioThreshold);
  EXPECT_EQ(kNewSmoothDecisionRatioThreshold,
            GetSmoothDecisionRatioThreshold());

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  constexpr float kP99ProcessingTimeMsSmoothAt60Hz = 12.0f;
  constexpr float kP99ProcessingTimeMsSmoothAt30Hz = 24.0f;
  constexpr int kFramesPerSecond = 60;

  // Add two smooth Full HD @ 60 fps decode entries.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt60Hz));
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt60Hz));
  // Add one not smooth Full HD @ 30 fps decode entry.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt30Hz));

  // Verify that Full HD @ 60 fps is not smooth since the ratio threshold is now
  // set to 0.7, but only 2/3 = 0.66 of the entries are smooth.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      kFramesPerSecond,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest, GetPerfInfoFrameRateBucketing) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  // First add 2 records to the history. First record with HD resolution and
  // second with Full HD resolution.
  constexpr float kP99ProcessingTimeMsSmoothAt60Hz = 12.0f;
  constexpr float kP99ProcessingTimeMsSmoothAt20Hz = 44.0f;

  // Add the entries.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt60Hz));
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt20Hz));

  // Verify perf history returns is_smooth = true for HD at 60 fps.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));
  // Verify perf history returns is_smooth = true for HD also at 120 fps since
  // 120 fps is quantized to 60 fps due to privacy concerns.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
      /*frames_per_second=*/120,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = false for Full HD at both 20 and 30
  // fps. Even though the processing time would work at 20 fps the output is
  // still expected to be smooth = false since 20 fps is quantized to 30 fps.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      /*frames_per_second=*/20,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
      /*frames_per_second=*/30,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

// Only save valid keys and stats.
TEST_P(WebrtcVideoPerfHistoryParamTest, OnlySaveValidKeysAndStats) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  constexpr float kValidP99ProcessingTimeMs = 12.0f;
  // Explicitly state that no save-done callbacks are expected.
  EXPECT_CALL(*this, MockSaveDoneCB()).Times(0);
  // Add invalid entries and verify that there's no save done callback.
  // Unknown profile.
  SavePerfRecord(
      Features(kIsDecode, VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN,
               kPixelsHd, kSoftware),
      VideoStats(kFramesProcessed, kKeyFramesProcessed,
                 kValidP99ProcessingTimeMs),
      /*expect_callback=*/false);
  // Out of bounds profile.
  SavePerfRecord(Features(kIsDecode, static_cast<VideoCodecProfile>(1000),
                          kPixelsHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kValidP99ProcessingTimeMs),
                 /*expect_callback=*/false);
  // Untracked codec profile.
  SavePerfRecord(
      Features(kIsDecode, DOLBYVISION_PROFILE5, kPixelsHd, kSoftware),
      VideoStats(kFramesProcessed, kKeyFramesProcessed,
                 kValidP99ProcessingTimeMs),
      /*expect_callback=*/false);
  // Invalid pixels.
  SavePerfRecord(
      Features(kIsDecode, kKnownProfile, /*video_pixels=*/100, kSoftware),
      VideoStats(kFramesProcessed, kKeyFramesProcessed,
                 kValidP99ProcessingTimeMs),
      /*expect_callback=*/false);
  SavePerfRecord(
      Features(kIsDecode, kKnownProfile, /*video_pixels=*/1e9, kSoftware),
      VideoStats(kFramesProcessed, kKeyFramesProcessed,
                 kValidP99ProcessingTimeMs),
      /*expect_callback=*/false);
  // Too few frames processed.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
                 VideoStats(/*frames_processed=*/10, kKeyFramesProcessed,
                            kValidP99ProcessingTimeMs),
                 /*expect_callback=*/false);
  // Too many frames processed.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
                 VideoStats(/*frames_processed=*/1e6, kKeyFramesProcessed,
                            kValidP99ProcessingTimeMs),
                 /*expect_callback=*/false);
  // Key frames higher than frames_processed.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
                 VideoStats(kFramesProcessed,
                            /*key_frames_processed=*/kFramesProcessed + 1,
                            kValidP99ProcessingTimeMs),
                 /*expect_callback=*/false);
  // Negative processing time.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            /*p99_processing_time_ms=*/-1.0f),
                 /*expect_callback=*/false);
  // Too high processing time.
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            /*p99_processing_time_ms=*/60000.0f),
                 /*expect_callback=*/false);

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest, SmoothIsTrueForUntrackedCodecProfiles) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  // First add 2 records with untracked codec profiles to the history that are
  // not smooth at 60Hz.
  constexpr float kP99ProcessingTimeMsNotSmoothAt60Hz = 40.0f;
  SavePerfRecord(
      Features(kIsDecode, DOLBYVISION_PROFILE5, kPixelsHd, kSoftware),
      VideoStats(kFramesProcessed, kKeyFramesProcessed,
                 kP99ProcessingTimeMsNotSmoothAt60Hz),
      /*expect_callback=*/false);
  SavePerfRecord(
      Features(kIsDecode, DOLBYVISION_PROFILE8, kPixelsHd, kSoftware),
      VideoStats(kFramesProcessed, kKeyFramesProcessed,
                 kP99ProcessingTimeMsNotSmoothAt60Hz),
      /*expect_callback=*/false);

  // Verify perf history returns is_smooth = true anyway.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, DOLBYVISION_PROFILE5, kPixelsHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, DOLBYVISION_PROFILE8, kPixelsHd, kSoftware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest, GetPerfInfoFailedInitialize) {
  WebrtcPerfHistoryTestParams params = GetParam();
  // Fail initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/false);

  // When initialization fails, callback should optimistically claim smooth
  // performance.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsDecode, kKnownProfile, kPixelsHd, kSoftware),
      /*frames_per_second=*/30,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Fail deferred DB initialization (see comment at top of test).
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(false);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest, AppendAndDestroyStats) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  constexpr float kP99ProcessingTimeMsSmoothAt30Hz = 24.0f;

  // Add the entries.
  SavePerfRecord(Features(kIsEncode, kKnownProfile, kPixelsFullHd, kHardware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt30Hz));

  // Verify its there before we ClearHistory(). Note that perf is NOT smooth at
  // 60 fps.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsEncode, kKnownProfile, kPixelsFullHd, kHardware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Initiate async clearing of history.
  EXPECT_CALL(*this, MockOnClearedHistory());
  perf_history_->ClearHistory(
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockOnClearedHistory,
                     base::Unretained(this)));

  // Verify record we added above is no longer present.
  // SUBTLE: The PerfHistory will optimistically respond kIsSmooth when no data
  // is found. So the signal that the entry was removed is the CB now claims
  // "smooth" when it claimed NOT smooth just moments before.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsEncode, kKnownProfile, kPixelsFullHd, kHardware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest, GetWebrtcVideoStatsDB) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  // Request a pointer to WebrtcVideoStatsDB and verify the callback.
  EXPECT_CALL(*this, MockGetWebrtcVideoStatsDBCB(_))
      .WillOnce([&](const auto* db_ptr) {
        // Not able to simply use a matcher because the DB does not exist at the
        // time we setup the EXPECT_CALL.
        EXPECT_EQ(GetFakeDB(), db_ptr);
      });

  perf_history_->GetWebrtcVideoStatsDB(
      base::BindOnce(&WebrtcVideoPerfHistoryTest::MockGetWebrtcVideoStatsDBCB,
                     base::Unretained(this)));

  task_environment_.RunUntilIdle();

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest, GetWebrtcVideoStatsDBFailedInitialize) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/false);

  // Request a pointer to WebrtcVideoStatsDB and verify the callback provides
  // a nullptr due to failed initialization.
  EXPECT_CALL(*this, MockGetWebrtcVideoStatsDBCB(IsNull()));
  perf_history_->GetWebrtcVideoStatsDB(
      base::BindOnce(&WebrtcVideoPerfHistoryTest::MockGetWebrtcVideoStatsDBCB,
                     base::Unretained(this)));

  task_environment_.RunUntilIdle();

  // Complete failed deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(false);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest, FailedDatabaseGetForAppend) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  // Create a simple record. After we fail to save this record we should find
  // smooth = true (the default for no-data-found).
  constexpr float kP99ProcessingTimeMsSmoothAt30Hz = 24.0f;

  // Fail the "get" step of the save (we always get existing stats so that the
  // new stats can be appended to it).
  GetFakeDB()->set_fail_next_get(true);

  // Attempt (and fail) the save.
  SavePerfRecord(Features(kIsEncode, kKnownProfile, kPixelsFullHd, kHardware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt30Hz));

  // Verify perf history still returns is_smooth = true since
  // no data was successfully saved for the given configuration.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsEncode, kKnownProfile, kPixelsFullHd, kHardware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest, FailedDatabaseAppend) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  // Force the DB to fail on the next append.
  GetFakeDB()->set_fail_next_append(true);

  // Create a simple record. After we fail to save this record we should find
  // smooth = true (the default for no-data-found).

  constexpr float kP99ProcessingTimeMsSmoothAt30Hz = 24.0f;

  // Attempt (and fail) the save.
  SavePerfRecord(Features(kIsEncode, kKnownProfile, kPixelsFullHd, kHardware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt30Hz));

  // Verify perf history still returns is_smooth = true since
  // no data was successfully saved for the given configuration.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
  perf_history_->GetPerfInfo(
      Features::New(kIsEncode, kKnownProfile, kPixelsFullHd, kHardware),
      /*frames_per_second=*/60,
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest, GetPerfInfo4KSmoothImpliesHdSmooth) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  // Add 4K entry that indicates that 4K is smooth at 60 Hz.
  constexpr float kP99ProcessingTimeMsSmoothAt60Hz = 12.0f;
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixels4K, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt60Hz));

  // Verify perf history returns is_smooth = true for all resolutions at 30/60
  // fps.
  constexpr int kPixels[] = {1280 * 720, 1920 * 1080, 3840 * 2160};
  constexpr int kFramerates[] = {30, 60};
  for (auto pixels : kPixels) {
    for (auto framerate : kFramerates) {
      EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
      perf_history_->GetPerfInfo(
          Features::New(kIsDecode, kKnownProfile, pixels, kSoftware), framerate,
          base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                         base::Unretained(this)));
    }
  }

  // Add an entry indicating that Full HD is not smooth.
  constexpr float kP99ProcessingTimeMsNotSmoothAt30Hz = 60.0f;
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsNotSmoothAt30Hz));

  // This is an incosistency, but the 4K entry with smooth=true overrides the
  // FullHD entry with smooth=false. Verify perf history returns is_smooth =
  // true for all resolutions at 30/60 fps.
  for (auto pixels : kPixels) {
    for (auto framerate : kFramerates) {
      EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth));
      perf_history_->GetPerfInfo(
          Features::New(kIsDecode, kKnownProfile, pixels, kSoftware), framerate,
          base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                         base::Unretained(this)));
    }
  }

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest,
       GetPerfInfoFullHdNotSmoothImplies4KNotSmooth) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  // Add entry that indicates that FullHD is not smooth at 30 Hz.
  constexpr float kP99ProcessingTimeMsNotSmoothAt30Hz = 60.0f;
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsNotSmoothAt30Hz));

  // Verify perf history returns is_smooth = true only for resolutions below
  // FullHD at 30/60 fps.
  constexpr int kPixels[] = {1280 * 720, 1920 * 1080, 3840 * 2160};
  constexpr int kFramerates[] = {30, 60};
  for (auto pixels : kPixels) {
    for (auto framerate : kFramerates) {
      EXPECT_CALL(*this, MockGetPerfInfoCB(
                             pixels < 1920 * 1080 ? kIsSmooth : kIsNotSmooth));
      perf_history_->GetPerfInfo(
          Features::New(kIsDecode, kKnownProfile, pixels, kSoftware), framerate,
          base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                         base::Unretained(this)));
    }
  }

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(WebrtcVideoPerfHistoryParamTest,
       GetPerfInfoFullHdSmoothEvenIf4KNotSmooth) {
  // NOTE: When the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  WebrtcPerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/*success=*/true);

  // Add 4K entry that indicates that 4K is not smooth at 30 Hz.
  constexpr float kP99ProcessingTimeMsNotSmoothAt30Hz = 60.0f;
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixels4K, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsNotSmoothAt30Hz));

  // Verify perf history returns is_smooth = true for all resolutions below 4K
  // at 30/60 fps.
  constexpr int kPixels[] = {1280 * 720, 1920 * 1080, 3840 * 2160};
  constexpr int kFramerates[] = {30, 60};
  for (auto pixels : kPixels) {
    for (auto framerate : kFramerates) {
      EXPECT_CALL(*this, MockGetPerfInfoCB(
                             pixels <= 1920 * 1080 ? kIsSmooth : kIsNotSmooth));
      perf_history_->GetPerfInfo(
          Features::New(kIsDecode, kKnownProfile, pixels, kSoftware), framerate,
          base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                         base::Unretained(this)));
    }
  }

  // Add an entry indicating that Full HD is smooth.
  constexpr float kP99ProcessingTimeMsSmoothAt60Hz = 12.0f;
  SavePerfRecord(Features(kIsDecode, kKnownProfile, kPixelsFullHd, kSoftware),
                 VideoStats(kFramesProcessed, kKeyFramesProcessed,
                            kP99ProcessingTimeMsSmoothAt60Hz));

  // Repeat test. The added entry is consistent with the default prediction.
  for (auto pixels : kPixels) {
    for (auto framerate : kFramerates) {
      EXPECT_CALL(*this, MockGetPerfInfoCB(
                             pixels <= 1920 * 1080 ? kIsSmooth : kIsNotSmooth));
      perf_history_->GetPerfInfo(
          Features::New(kIsDecode, kKnownProfile, pixels, kSoftware), framerate,
          base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockGetPerfInfoCB,
                         base::Unretained(this)));
    }
  }

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

const WebrtcPerfHistoryTestParams kWebrtcPerfHistoryTestParams[] = {
    {/*defer_initialize=*/true},
    {/*defer_initialize=*/false},
};

INSTANTIATE_TEST_SUITE_P(VaryDBInitTiming,
                         WebrtcVideoPerfHistoryParamTest,
                         ::testing::ValuesIn(kWebrtcPerfHistoryTestParams));

//
// The following tests are not parameterized. They instead always hard code
// deferred initialization.
//

TEST_F(WebrtcVideoPerfHistoryTest, ClearHistoryTriggersSuccessfulInitialize) {
  // Clear the DB. Completion callback shouldn't fire until initialize
  // completes.
  EXPECT_CALL(*this, MockOnClearedHistory()).Times(0);
  perf_history_->ClearHistory(
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockOnClearedHistory,
                     base::Unretained(this)));

  // Give completion callback a chance to fire. Confirm it did not fire.
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  // Expect completion callback after we successfully initialize.
  EXPECT_CALL(*this, MockOnClearedHistory());
  GetFakeDB()->CompleteInitialize(true);

  // Give deferred callback a chance to fire.
  task_environment_.RunUntilIdle();
}

TEST_F(WebrtcVideoPerfHistoryTest, ClearHistoryTriggersFailedInitialize) {
  // Clear the DB. Completion callback shouldn't fire until initialize
  // completes.
  EXPECT_CALL(*this, MockOnClearedHistory()).Times(0);
  perf_history_->ClearHistory(
      base::BindOnce(&WebrtcVideoPerfHistoryParamTest::MockOnClearedHistory,
                     base::Unretained(this)));

  // Give completion callback a chance to fire. Confirm it did not fire.
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  // Expect completion callback after completing initialize. "Failure" is
  // still a form of completion.
  EXPECT_CALL(*this, MockOnClearedHistory());
  GetFakeDB()->CompleteInitialize(false);

  // Give deferred callback a chance to fire.
  task_environment_.RunUntilIdle();
}

}  // namespace media

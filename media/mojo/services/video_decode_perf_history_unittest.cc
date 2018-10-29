// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "media/capabilities/video_decode_stats_db.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using UkmEntry = ukm::builders::Media_VideoDecodePerfRecord;
using testing::Eq;
using testing::IsNull;
using testing::_;

namespace {

// Aliases for readability.
const bool kIsSmooth = true;
const bool kIsNotSmooth = false;
const bool kIsPowerEfficient = true;
const bool kIsNotPowerEfficient = false;
const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));
const bool kIsTopFrame = true;
const uint64_t kPlayerId = 1234u;

}  // namespace

namespace media {

class FakeVideoDecodeStatsDB : public VideoDecodeStatsDB {
 public:
  FakeVideoDecodeStatsDB() = default;
  ~FakeVideoDecodeStatsDB() override = default;

  // Call CompleteInitialize(...) to run |init_cb| callback.
  void Initialize(base::OnceCallback<void(bool)> init_cb) override {
    pendnding_init_cb_ = std::move(init_cb);
  }

  // Completes fake initialization, running |init_cb| with the supplied value
  // for success.
  void CompleteInitialize(bool success) {
    DVLOG(2) << __func__ << " running with success = " << success;
    EXPECT_FALSE(!pendnding_init_cb_);
    std::move(pendnding_init_cb_).Run(success);
  }

  void AppendDecodeStats(const VideoDescKey& key,
                         const DecodeStatsEntry& new_entry,
                         AppendDecodeStatsCB append_done_cb) override {
    std::string key_str = MakeKeyString(key);
    if (entries_.find(key_str) == entries_.end()) {
      entries_.emplace(std::make_pair(key_str, new_entry));
    } else {
      const DecodeStatsEntry& known_entry = entries_.at(key_str);
      entries_.at(key_str) = DecodeStatsEntry(
          known_entry.frames_decoded + new_entry.frames_decoded,
          known_entry.frames_dropped + new_entry.frames_dropped,
          known_entry.frames_power_efficient +
              new_entry.frames_power_efficient);
    }

    std::move(append_done_cb).Run(true);
  }

  void GetDecodeStats(const VideoDescKey& key,
                      GetDecodeStatsCB get_stats_cb) override {
    auto entry_it = entries_.find(MakeKeyString(key));
    if (entry_it == entries_.end()) {
      std::move(get_stats_cb).Run(true, nullptr);
    } else {
      std::move(get_stats_cb)
          .Run(true, std::make_unique<DecodeStatsEntry>(entry_it->second));
    }
  }

  void ClearStats(base::OnceClosure clear_done_cb) override {
    entries_.clear();
    std::move(clear_done_cb).Run();
  }

 private:
  static std::string MakeKeyString(const VideoDescKey& key) {
    return base::StringPrintf("%d|%s|%d", static_cast<int>(key.codec_profile),
                              key.size.ToString().c_str(), key.frame_rate);
  }

  std::map<std::string, DecodeStatsEntry> entries_;

  base::OnceCallback<void(bool)> pendnding_init_cb_;
};

class VideoDecodePerfHistoryTest : public testing::Test {
 public:
  void SetUp() override {
    test_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    perf_history_ = std::make_unique<VideoDecodePerfHistory>(
        std::make_unique<FakeVideoDecodeStatsDB>());
  }

  void TearDown() override { perf_history_.reset(); }

  FakeVideoDecodeStatsDB* GetFakeDB() {
    return static_cast<FakeVideoDecodeStatsDB*>(perf_history_->db_.get());
  }

  void PreInitializeDB(bool initialize_success) {
    // Invoke private method to start initialization. Usually invoked by first
    // API call requiring DB access.
    perf_history_->InitDatabase();
    // Complete initialization by firing callback from our fake DB.
    GetFakeDB()->CompleteInitialize(initialize_success);
  }

  // Tests may set this as the callback for VideoDecodePerfHistory::GetPerfInfo
  // to check the results of the call.
  MOCK_METHOD2(MockGetPerfInfoCB,
               void(bool is_smooth, bool is_power_efficient));

  // Tests should EXPECT_CALL this method prior to ClearHistory() to know that
  // the operation has completed.
  MOCK_METHOD0(MockOnClearedHistory, void());

  MOCK_METHOD1(MockGetVideoDecodeStatsDBCB, void(VideoDecodeStatsDB* db));

  mojom::PredictionFeatures MakeFeatures(VideoCodecProfile profile,
                                         gfx::Size video_size,
                                         int frames_per_sec) {
    mojom::PredictionFeatures features;
    features.profile = profile;
    features.video_size = video_size;
    features.frames_per_sec = frames_per_sec;
    return features;
  }

  mojom::PredictionFeaturesPtr MakeFeaturesPtr(VideoCodecProfile profile,
                                               gfx::Size video_size,
                                               int frames_per_sec) {
    mojom::PredictionFeaturesPtr features = mojom::PredictionFeatures::New();
    *features = MakeFeatures(profile, video_size, frames_per_sec);
    return features;
  }

  mojom::PredictionTargets MakeTargets(uint32_t frames_decoded,
                                       uint32_t frames_dropped,
                                       uint32_t frames_power_efficient) {
    mojom::PredictionTargets targets;
    targets.frames_decoded = frames_decoded;
    targets.frames_dropped = frames_dropped;
    targets.frames_power_efficient = frames_power_efficient;
    return targets;
  }

  void SavePerfRecord(const url::Origin& origin,
                      bool is_top_frame,
                      mojom::PredictionFeatures features,
                      mojom::PredictionTargets targets,
                      uint64_t player_id) {
    // Null saved done CB. Save is verified separately via GetPerfInfo() after
    // save completes.
    base::OnceClosure save_done_cb;

    const ukm::SourceId source_id = test_recorder_->GetNewSourceID();
    test_recorder_->UpdateSourceURL(source_id, origin.GetURL());

    perf_history_->GetSaveCallback().Run(source_id, is_top_frame, features,
                                         targets, player_id,
                                         std::move(save_done_cb));
  }

 protected:
  using VideoDescKey = VideoDecodeStatsDB::VideoDescKey;
  using DecodeStatsEntry = VideoDecodeStatsDB::DecodeStatsEntry;

  static constexpr double kMaxSmoothDroppedFramesPercent =
      VideoDecodePerfHistory::kMaxSmoothDroppedFramesPercent;
  static constexpr double kMinPowerEfficientDecodedFramePercent =
      VideoDecodePerfHistory::kMinPowerEfficientDecodedFramePercent;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;

  // The VideoDecodeStatsReporter being tested.
  std::unique_ptr<VideoDecodePerfHistory> perf_history_;
};

// When bool param is true, tests should wait until the end to run
// GetFakeDB()->CompleteInitialize(). Otherwise run PreInitializeDB() at the
// test start.
class VideoDecodePerfHistoryParamTest
    : public testing::WithParamInterface<bool>,
      public VideoDecodePerfHistoryTest {};

TEST_P(VideoDecodePerfHistoryParamTest, GetPerfInfo_Smooth) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  bool defer_initialize = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!defer_initialize)
    PreInitializeDB(/* success */ true);

  // First add 2 records to the history. The second record has a higher frame
  // rate and a higher number of dropped frames such that it is "not smooth".
  const VideoCodecProfile kKnownProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kKownSize(100, 200);
  const int kSmoothFrameRate = 30;
  const int kNotSmoothFrameRate = 90;
  const int kFramesDecoded = 1000;
  const int kNotPowerEfficientFramesDecoded = 0;
  // Sets the ratio of dropped frames to barely qualify as smooth.
  const int kSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent;
  // Set the ratio of dropped frames to barely qualify as NOT smooth.
  const int kNotSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent + 1;

  // Add the entries.
  SavePerfRecord(kOrigin, kIsTopFrame,
                 MakeFeatures(kKnownProfile, kKownSize, kSmoothFrameRate),
                 MakeTargets(kFramesDecoded, kSmoothFramesDropped,
                             kNotPowerEfficientFramesDecoded),
                 kPlayerId);
  SavePerfRecord(kOrigin, kIsTopFrame,
                 MakeFeatures(kKnownProfile, kKownSize, kNotSmoothFrameRate),
                 MakeTargets(kFramesDecoded, kNotSmoothFramesDropped,
                             kNotPowerEfficientFramesDecoded),
                 kPlayerId);

  // Verify perf history returns is_smooth = true for the smooth entry.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kKnownProfile, kKownSize, kSmoothFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = false for the NOT smooth entry.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kKnownProfile, kKownSize, kNotSmoothFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history optimistically returns is_smooth = true when no entry
  // can be found with the given configuration.
  const VideoCodecProfile kUnknownProfile = VP9PROFILE_PROFILE2;
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kUnknownProfile, kKownSize, kNotSmoothFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    scoped_task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, GetPerfInfo_PowerEfficient) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  bool defer_initialize = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!defer_initialize)
    PreInitializeDB(/* success */ true);

  // First add 3 records to the history:
  // - the first has a high number of power efficiently decoded frames;
  // - the second has a low number of power efficiently decoded frames;
  // - the third is similar to the first with a high number of dropped frames.
  const VideoCodecProfile kPowerEfficientProfile = VP9PROFILE_PROFILE0;
  const VideoCodecProfile kNotPowerEfficientProfile = VP8PROFILE_ANY;
  const gfx::Size kKownSize(100, 200);
  const int kSmoothFrameRate = 30;
  const int kNotSmoothFrameRate = 90;
  const int kFramesDecoded = 1000;
  const int kPowerEfficientFramesDecoded =
      kFramesDecoded * kMinPowerEfficientDecodedFramePercent;
  const int kNotPowerEfficientFramesDecoded =
      kFramesDecoded * kMinPowerEfficientDecodedFramePercent - 1;
  // Sets the ratio of dropped frames to barely qualify as smooth.
  const int kSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent;
  // Set the ratio of dropped frames to barely qualify as NOT smooth.
  const int kNotSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent + 1;

  // Add the entries.
  SavePerfRecord(
      kOrigin, kIsTopFrame,
      MakeFeatures(kPowerEfficientProfile, kKownSize, kSmoothFrameRate),
      MakeTargets(kFramesDecoded, kSmoothFramesDropped,
                  kPowerEfficientFramesDecoded),
      kPlayerId);
  SavePerfRecord(
      kOrigin, kIsTopFrame,
      MakeFeatures(kNotPowerEfficientProfile, kKownSize, kSmoothFrameRate),
      MakeTargets(kFramesDecoded, kSmoothFramesDropped,
                  kNotPowerEfficientFramesDecoded),
      kPlayerId);
  SavePerfRecord(
      kOrigin, kIsTopFrame,
      MakeFeatures(kPowerEfficientProfile, kKownSize, kNotSmoothFrameRate),
      MakeTargets(kFramesDecoded, kNotSmoothFramesDropped,
                  kPowerEfficientFramesDecoded),
      kPlayerId);

  // Verify perf history returns is_smooth = true, is_power_efficient = true.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kPowerEfficientProfile, kKownSize, kSmoothFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = true, is_power_efficient = false.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kNotPowerEfficientProfile, kKownSize, kSmoothFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = false, is_power_efficient = true.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kPowerEfficientProfile, kKownSize, kNotSmoothFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history optimistically returns is_smooth = true and
  // is_power_efficient = true when no entry can be found with the given
  // configuration.
  const VideoCodecProfile kUnknownProfile = VP9PROFILE_PROFILE2;
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kUnknownProfile, kKownSize, kNotSmoothFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    scoped_task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, GetPerfInfo_FailedInitialize) {
  bool defer_initialize = GetParam();
  // Fail initialization in advance of API calls when not asked to defer.
  if (!defer_initialize)
    PreInitializeDB(/* success */ false);

  const VideoCodecProfile kProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;

  // When initialization fails, callback should optimistically claim both smooth
  // and power efficient performance.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kProfile, kSize, kFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Fail deferred DB initialization (see comment at top of test).
  if (defer_initialize) {
    GetFakeDB()->CompleteInitialize(false);

    // Allow initialize-deferred API calls to complete.
    scoped_task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, AppendAndDestroyStats) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  bool defer_initialize = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!defer_initialize)
    PreInitializeDB(/* success */ true);

  // Add a simple record to the history.
  const VideoCodecProfile kProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;
  const int kFramesDecoded = 1000;
  const int kManyFramesDropped = kFramesDecoded / 2;
  const int kFramesPowerEfficient = kFramesDecoded;
  SavePerfRecord(
      kOrigin, kIsTopFrame, MakeFeatures(kProfile, kSize, kFrameRate),
      MakeTargets(kFramesDecoded, kManyFramesDropped, kFramesPowerEfficient),
      kPlayerId);

  // Verify its there before we ClearHistory(). Note that perf is NOT smooth.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kProfile, kSize, kFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Initiate async clearing of history.
  EXPECT_CALL(*this, MockOnClearedHistory());
  perf_history_->ClearHistory(
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockOnClearedHistory,
                     base::Unretained(this)));

  // Verify record we added above is no longer present.
  // SUBTLE: The PerfHistory will optimistically respond kIsSmooth when no data
  // is found. So the signal that the entry was removed is the CB now claims
  // "smooth" when it claimed NOT smooth just moments before.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kProfile, kSize, kFrameRate),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    scoped_task_environment_.RunUntilIdle();
  }

  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, kOrigin.GetURL());

#define EXPECT_UKM(name, value) \
  test_recorder_->ExpectEntryMetric(entry, name, value)

    EXPECT_UKM(UkmEntry::kVideo_InTopFrameName, kIsTopFrame);
    EXPECT_UKM(UkmEntry::kVideo_PlayerIDName, kPlayerId);
    EXPECT_UKM(UkmEntry::kVideo_CodecProfileName, kProfile);
    EXPECT_UKM(UkmEntry::kVideo_FramesPerSecondName, kFrameRate);
#undef EXPECT_UKM

    // TODO(chcunningham): Expand UKM tests to include absence tests.
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, GetVideoDecodeStatsDB) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  bool defer_initialize = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!defer_initialize)
    PreInitializeDB(/* success */ true);

  // Request a pointer to VideoDecodeStatsDB and verify the callback.
  EXPECT_CALL(*this, MockGetVideoDecodeStatsDBCB(_))
      .WillOnce([&](const auto* db_ptr) {
        // Not able to simply use a matcher because the DB does not exist at the
        // time we setup the EXPECT_CALL.
        EXPECT_EQ(GetFakeDB(), db_ptr);
      });

  perf_history_->GetVideoDecodeStatsDB(
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetVideoDecodeStatsDBCB,
                     base::Unretained(this)));

  scoped_task_environment_.RunUntilIdle();

  // Complete successful deferred DB initialization (see comment at top of test)
  if (defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    scoped_task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest,
       GetVideoDecodeStatsDB_FailedInitialize) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  bool defer_initialize = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!defer_initialize)
    PreInitializeDB(/* success */ false);

  // Request a pointer to VideoDecodeStatsDB and verify the callback provides
  // a nullptr due to failed initialization.
  EXPECT_CALL(*this, MockGetVideoDecodeStatsDBCB(IsNull()));
  perf_history_->GetVideoDecodeStatsDB(
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetVideoDecodeStatsDBCB,
                     base::Unretained(this)));

  scoped_task_environment_.RunUntilIdle();

  // Complete failed deferred DB initialization (see comment at top of test)
  if (defer_initialize) {
    GetFakeDB()->CompleteInitialize(false);

    // Allow initialize-deferred API calls to complete.
    scoped_task_environment_.RunUntilIdle();
  }
}

INSTANTIATE_TEST_CASE_P(VaryDBInitTiming,
                        VideoDecodePerfHistoryParamTest,
                        ::testing::Values(true, false));

}  // namespace media

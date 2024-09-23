// Copyright 2017 The Chromium Authors
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
#include "components/ukm/test_ukm_recorder.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/capabilities/video_decode_stats_db.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/services/test_helpers.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using UkmEntry = ukm::builders::Media_VideoDecodePerfRecord;
using testing::IsNull;
using testing::_;

namespace {

// Aliases for readability.
const bool kIsSmooth = true;
const bool kIsNotSmooth = false;
const bool kIsPowerEfficient = true;
const bool kIsNotPowerEfficient = false;
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
    EXPECT_FALSE(!!pendnding_init_cb_);
    pendnding_init_cb_ = std::move(init_cb);
  }

  // Completes fake initialization, running |init_cb| with the supplied value
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

  void AppendDecodeStats(const VideoDescKey& key,
                         const DecodeStatsEntry& new_entry,
                         AppendDecodeStatsCB append_done_cb) override {
    if (fail_next_append_) {
      fail_next_append_ = false;
      std::move(append_done_cb).Run(false);
      return;
    }

    std::string key_str = key.Serialize();
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
    if (fail_next_get_) {
      fail_next_get_ = false;
      std::move(get_stats_cb).Run(false, nullptr);
      return;
    }

    auto entry_it = entries_.find(key.Serialize());
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
  friend class VideoDecodePerfHistoryTest;

  // Private method for immediate retrieval of stats for test helpers.
  std::unique_ptr<DecodeStatsEntry> GetDecodeStatsSync(
      const VideoDescKey& key) {
    auto entry_it = entries_.find(key.Serialize());
    if (entry_it == entries_.end()) {
      return nullptr;
    } else {
      return std::make_unique<DecodeStatsEntry>(entry_it->second);
    }
  }

  bool fail_next_append_ = false;
  bool fail_next_get_ = false;

  std::map<std::string, DecodeStatsEntry> entries_;

  base::OnceCallback<void(bool)> pendnding_init_cb_;
};

class VideoDecodePerfHistoryTest : public testing::Test {
 public:
  // Indicates what type of UKM verification should be performed upon saving
  // new stats to the perf history.
  enum class UkmVerifcation {
    kNoUkmsAdded,
    kSaveTriggersUkm,
  };

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

  double GetMaxSmoothDroppedFramesPercent(bool is_eme = false) {
    return VideoDecodePerfHistory::GetMaxSmoothDroppedFramesPercent(is_eme);
  }

  static base::FieldTrialParams GetFieldTrialParams() {
    return VideoDecodePerfHistory::GetFieldTrialParams();
  }

  // Tests may set this as the callback for VideoDecodePerfHistory::GetPerfInfo
  // to check the results of the call.
  MOCK_METHOD2(MockGetPerfInfoCB,
               void(bool is_smooth, bool is_power_efficient));

  // Tests should EXPECT_CALL this method prior to ClearHistory() to know that
  // the operation has completed.
  MOCK_METHOD0(MockOnClearedHistory, void());

  MOCK_METHOD1(MockGetVideoDecodeStatsDBCB, void(VideoDecodeStatsDB* db));

  // Internal check that the UKM verification is complete before the test exits.
  MOCK_METHOD0(UkmVerifyDoneCb, void());

  void SavePerfRecord(UkmVerifcation ukm_verification,
                      const url::Origin& origin,
                      bool is_top_frame,
                      mojom::PredictionFeatures features,
                      mojom::PredictionTargets targets,
                      uint64_t player_id) {
    // Manually associate URL with |source_id|. In production this happens
    // externally at a higher layer.
    const ukm::SourceId source_id = test_recorder_->GetNewSourceID();
    test_recorder_->UpdateSourceURL(source_id, origin.GetURL());

    // Use save callback to verify UKM reporting.
    base::OnceClosure save_done_cb;
    switch (ukm_verification) {
      case UkmVerifcation::kSaveTriggersUkm: {
        // Expect UKM report with given properties upon successful save. Use
        // old stats values from DB to set expectations about what API would
        // have claimed pre-save.
        std::unique_ptr<DecodeStatsEntry> old_stats =
            GetStatsForFeatures(features);
        save_done_cb =
            base::BindOnce(&VideoDecodePerfHistoryTest::VerifyLastUkmReport,
                           base::Unretained(this), origin, is_top_frame,
                           features, targets, player_id, std::move(old_stats));
        break;
      }
      case UkmVerifcation::kNoUkmsAdded: {
        // Expect no additional UKM entries upon failing to save. Capture the
        // current entry count before save to verify no new entries are added.
        size_t current_num_entries =
            test_recorder_->GetEntriesByName(UkmEntry::kEntryName).size();
        save_done_cb = base::BindOnce(
            &VideoDecodePerfHistoryTest::VerifyNoUkmReportForFailedSave,
            base::Unretained(this), current_num_entries);
        break;
      }
    }

    // Check that the UKM verification is complete before the test exits.
    EXPECT_CALL(*this, UkmVerifyDoneCb());

    perf_history_->GetSaveCallback().Run(
        source_id, learning::FeatureValue(kOrigin.host()), is_top_frame,
        features, targets, player_id, std::move(save_done_cb));
  }

 protected:
  using VideoDescKey = VideoDecodeStatsDB::VideoDescKey;
  using DecodeStatsEntry = VideoDecodeStatsDB::DecodeStatsEntry;

  // Private helper to public test methods. This bypasses  VPH::GetPerfInfo()
  // to synchronously grab stats from the fake DB. Tests should instead use
  // the VPH::GetPerfInfo().
  std::unique_ptr<DecodeStatsEntry> GetStatsForFeatures(
      mojom::PredictionFeatures features) {
    VideoDecodeStatsDB::VideoDescKey video_key =
        VideoDecodeStatsDB::VideoDescKey::MakeBucketedKey(
            features.profile, features.video_size, features.frames_per_sec,
            features.key_system, features.use_hw_secure_codecs);
    return GetFakeDB()->GetDecodeStatsSync(video_key);
  }

  // Lookup up the most recent recorded entry and verify its properties against
  // method arguments.
  void VerifyLastUkmReport(const url::Origin& origin,
                           bool is_top_frame,
                           mojom::PredictionFeatures features,
                           mojom::PredictionTargets new_targets,
                           uint64_t player_id,
                           std::unique_ptr<DecodeStatsEntry> old_stats) {
#define EXPECT_UKM(name, value) \
  test_recorder_->ExpectEntryMetric(entry, name, value)
#define EXPECT_NO_UKM(name) \
  EXPECT_FALSE(test_recorder_->EntryHasMetric(entry, name))

    const auto& entries =
        test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_GE(entries.size(), 1U);
    auto* entry = entries.back().get();

    // Verify stream properties. Make a key to ensure we check bucketed values.
    VideoDecodeStatsDB::VideoDescKey key =
        VideoDecodeStatsDB::VideoDescKey::MakeBucketedKey(
            features.profile, features.video_size, features.frames_per_sec,
            features.key_system, features.use_hw_secure_codecs);
    test_recorder_->ExpectEntrySourceHasUrl(entry, origin.GetURL());
    EXPECT_UKM(UkmEntry::kVideo_InTopFrameName, is_top_frame);
    EXPECT_UKM(UkmEntry::kVideo_PlayerIDName, player_id);
    EXPECT_UKM(UkmEntry::kVideo_CodecProfileName, key.codec_profile);
    EXPECT_UKM(UkmEntry::kVideo_FramesPerSecondName, key.frame_rate);
    EXPECT_UKM(UkmEntry::kVideo_NaturalHeightName, key.size.height());
    EXPECT_UKM(UkmEntry::kVideo_NaturalWidthName, key.size.width());
    if (key.key_system.empty()) {
      EXPECT_NO_UKM(UkmEntry::kVideo_EME_KeySystemName);
      EXPECT_NO_UKM(UkmEntry::kVideo_EME_UseHwSecureCodecsName);
    } else {
      EXPECT_UKM(UkmEntry::kVideo_EME_KeySystemName,
                 GetKeySystemIntForUKM(key.key_system));
      EXPECT_UKM(UkmEntry::kVideo_EME_UseHwSecureCodecsName,
                 key.use_hw_secure_codecs);
    }

    // Verify past stats.
    bool past_is_smooth = false;
    bool past_is_efficient = false;
    perf_history_->AssessStats(key, old_stats.get(), &past_is_smooth,
                               &past_is_efficient);
    EXPECT_UKM(UkmEntry::kPerf_ApiWouldClaimIsSmoothName, past_is_smooth);
    EXPECT_UKM(UkmEntry::kPerf_ApiWouldClaimIsPowerEfficientName,
               past_is_efficient);
    // Zero it out to make verification readable.
    if (!old_stats)
      old_stats = std::make_unique<DecodeStatsEntry>(0, 0, 0);
    EXPECT_UKM(UkmEntry::kPerf_PastVideoFramesDecodedName,
               old_stats->frames_decoded);
    EXPECT_UKM(UkmEntry::kPerf_PastVideoFramesDroppedName,
               old_stats->frames_dropped);
    EXPECT_UKM(UkmEntry::kPerf_PastVideoFramesPowerEfficientName,
               old_stats->frames_power_efficient);

    // Verify latest stats.
    VideoDecodeStatsDB::DecodeStatsEntry new_stats(
        new_targets.frames_decoded, new_targets.frames_dropped,
        new_targets.frames_power_efficient);
    bool new_is_smooth = false;
    bool new_is_efficient = false;
    perf_history_->AssessStats(key, &new_stats, &new_is_smooth,
                               &new_is_efficient);
    EXPECT_UKM(UkmEntry::kPerf_RecordIsSmoothName, new_is_smooth);
    EXPECT_UKM(UkmEntry::kPerf_RecordIsPowerEfficientName, new_is_efficient);
    EXPECT_UKM(UkmEntry::kPerf_VideoFramesDecodedName,
               new_stats.frames_decoded);
    EXPECT_UKM(UkmEntry::kPerf_VideoFramesDroppedName,
               new_stats.frames_dropped);
    EXPECT_UKM(UkmEntry::kPerf_VideoFramesPowerEfficientName,
               new_stats.frames_power_efficient);

#undef EXPECT_UKM
#undef EXPECT_NO_UKM

    UkmVerifyDoneCb();
  }

  void VerifyNoUkmReportForFailedSave(size_t expected_num_ukm_entries) {
    // Verify no new UKM entries appear after failed save.
    const auto& entries =
        test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(expected_num_ukm_entries, entries.size());

    UkmVerifyDoneCb();
  }

  static constexpr double kMinPowerEfficientDecodedFramePercent =
      VideoDecodePerfHistory::kMinPowerEfficientDecodedFramePercent;

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;

  // The VideoDecodeStatsReporter being tested.
  std::unique_ptr<VideoDecodePerfHistory> perf_history_;

  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));
};

struct PerfHistoryTestParams {
  const bool defer_initialize;
  const std::string key_system;
  const bool use_hw_secure_codecs;
};

// When bool param is true, tests should wait until the end to run
// GetFakeDB()->CompleteInitialize(). Otherwise run PreInitializeDB() at the
// test start.
class VideoDecodePerfHistoryParamTest
    : public testing::WithParamInterface<PerfHistoryTestParams>,
      public VideoDecodePerfHistoryTest {};

TEST_P(VideoDecodePerfHistoryParamTest, GetPerfInfo_Smooth) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  PerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
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
      kFramesDecoded * GetMaxSmoothDroppedFramesPercent();
  // Set the ratio of dropped frames to barely qualify as NOT smooth.
  const int kNotSmoothFramesDropped =
      kFramesDecoded * GetMaxSmoothDroppedFramesPercent() + 1;

  // Add the entries.
  SavePerfRecord(UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
                 MakeFeatures(kKnownProfile, kKownSize, kSmoothFrameRate,
                              params.key_system, params.use_hw_secure_codecs),
                 MakeTargets(kFramesDecoded, kSmoothFramesDropped,
                             kNotPowerEfficientFramesDecoded),
                 kPlayerId);
  SavePerfRecord(UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
                 MakeFeatures(kKnownProfile, kKownSize, kNotSmoothFrameRate,
                              params.key_system, params.use_hw_secure_codecs),
                 MakeTargets(kFramesDecoded, kNotSmoothFramesDropped,
                             kNotPowerEfficientFramesDecoded),
                 kPlayerId);

  // Verify perf history returns is_smooth = true for the smooth entry.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kKnownProfile, kKownSize, kSmoothFrameRate,
                      params.key_system, params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = false for the NOT smooth entry.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kKnownProfile, kKownSize, kNotSmoothFrameRate,
                      params.key_system, params.use_hw_secure_codecs),
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
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, GetPerfInfo_PowerEfficient) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  PerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
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
      kFramesDecoded * GetMaxSmoothDroppedFramesPercent();
  // Set the ratio of dropped frames to barely qualify as NOT smooth.
  const int kNotSmoothFramesDropped =
      kFramesDecoded * GetMaxSmoothDroppedFramesPercent() + 1;

  // Add the entries.
  SavePerfRecord(
      UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
      MakeFeatures(kPowerEfficientProfile, kKownSize, kSmoothFrameRate,
                   params.key_system, params.use_hw_secure_codecs),
      MakeTargets(kFramesDecoded, kSmoothFramesDropped,
                  kPowerEfficientFramesDecoded),
      kPlayerId);
  SavePerfRecord(
      UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
      MakeFeatures(kNotPowerEfficientProfile, kKownSize, kSmoothFrameRate,
                   params.key_system, params.use_hw_secure_codecs),
      MakeTargets(kFramesDecoded, kSmoothFramesDropped,
                  kNotPowerEfficientFramesDecoded),
      kPlayerId);
  SavePerfRecord(
      UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
      MakeFeatures(kPowerEfficientProfile, kKownSize, kNotSmoothFrameRate,
                   params.key_system, params.use_hw_secure_codecs),
      MakeTargets(kFramesDecoded, kNotSmoothFramesDropped,
                  kPowerEfficientFramesDecoded),
      kPlayerId);

  // Verify perf history returns is_smooth = true, is_power_efficient = true.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kPowerEfficientProfile, kKownSize, kSmoothFrameRate,
                      params.key_system, params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = true, is_power_efficient = false.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kNotPowerEfficientProfile, kKownSize, kSmoothFrameRate,
                      params.key_system, params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = false, is_power_efficient = true.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kPowerEfficientProfile, kKownSize, kNotSmoothFrameRate,
                      params.key_system, params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history optimistically returns is_smooth = true and
  // is_power_efficient = true when no entry can be found with the given
  // configuration.
  const VideoCodecProfile kUnknownProfile = VP9PROFILE_PROFILE2;
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kUnknownProfile, kKownSize, kNotSmoothFrameRate,
                      params.key_system, params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, GetPerfInfo_FailedInitialize) {
  PerfHistoryTestParams params = GetParam();
  // Fail initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/* success */ false);

  const VideoCodecProfile kProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;

  // When initialization fails, callback should optimistically claim both smooth
  // and power efficient performance.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kProfile, kSize, kFrameRate, params.key_system,
                      params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Fail deferred DB initialization (see comment at top of test).
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(false);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, AppendAndDestroyStats) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  PerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/* success */ true);

  // Add a simple record to the history.
  const VideoCodecProfile kProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;
  const int kFramesDecoded = 1000;
  const int kManyFramesDropped = kFramesDecoded / 2;
  const int kFramesPowerEfficient = kFramesDecoded;
  SavePerfRecord(
      UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
      MakeFeatures(kProfile, kSize, kFrameRate, params.key_system,
                   params.use_hw_secure_codecs),
      MakeTargets(kFramesDecoded, kManyFramesDropped, kFramesPowerEfficient),
      kPlayerId);

  // Verify its there before we ClearHistory(). Note that perf is NOT smooth.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kProfile, kSize, kFrameRate, params.key_system,
                      params.use_hw_secure_codecs),
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
      MakeFeaturesPtr(kProfile, kSize, kFrameRate, params.key_system,
                      params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, GetVideoDecodeStatsDB) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  PerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
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

  task_environment_.RunUntilIdle();

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest,
       GetVideoDecodeStatsDB_FailedInitialize) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  PerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/* success */ false);

  // Request a pointer to VideoDecodeStatsDB and verify the callback provides
  // a nullptr due to failed initialization.
  EXPECT_CALL(*this, MockGetVideoDecodeStatsDBCB(IsNull()));
  perf_history_->GetVideoDecodeStatsDB(
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetVideoDecodeStatsDBCB,
                     base::Unretained(this)));

  task_environment_.RunUntilIdle();

  // Complete failed deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(false);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, FailedDatabaseGetForAppend) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  PerfHistoryTestParams params = GetParam();

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/* success */ true);

  // Create a record that is neither smooth nor power efficient. After we fail
  // to save this record we should find smooth = power_efficient = true (the
  // default for no-data-found).
  const VideoCodecProfile kProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;
  const int kFramesDecoded = 1000;
  const int kFramesDropped =
      kFramesDecoded * GetMaxSmoothDroppedFramesPercent() + 1;
  const int kFramesPowerEfficient = 0;

  // Fail the "get" step of the save (we always get existing stats prior to
  // save for UKM reporting).
  GetFakeDB()->set_fail_next_get(true);

  // Attempt (and fail) the save. UKM report depends on successful retrieval
  // of stats from the DB, so no UKM reporting should occur here.
  SavePerfRecord(
      UkmVerifcation::kNoUkmsAdded, kOrigin, kIsTopFrame,
      MakeFeatures(kProfile, kSize, kFrameRate, params.key_system,
                   params.use_hw_secure_codecs),
      MakeTargets(kFramesDecoded, kFramesDropped, kFramesPowerEfficient),
      kPlayerId);

  // Verify perf history still returns is_smooth = power_efficient = true since
  // no data was successfully saved for the given configuration.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kProfile, kSize, kFrameRate, params.key_system,
                      params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest, FailedDatabaseAppend) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  PerfHistoryTestParams params = GetParam();

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/* success */ true);

  // Force the DB to fail on the next append.
  GetFakeDB()->set_fail_next_append(true);

  // Create a record that is neither smooth nor power efficient. After we fail
  // to save this record we should find smooth = power_efficient = true (the
  // default for no-data-found).
  const VideoCodecProfile kProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;
  const int kFramesDecoded = 1000;
  const int kFramesDropped =
      kFramesDecoded * GetMaxSmoothDroppedFramesPercent() + 1;
  const int kFramesPowerEfficient = 0;

  // Attempt (and fail) the save. Note that we still expect UKM to be reported
  // because we successfully retrieved stats from the DB, we just fail to append
  // the new stats. UKM reporting occurs between retrieval and appending.
  SavePerfRecord(
      UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
      MakeFeatures(kProfile, kSize, kFrameRate, params.key_system,
                   params.use_hw_secure_codecs),
      MakeTargets(kFramesDecoded, kFramesDropped, kFramesPowerEfficient),
      kPlayerId);

  // Verify perf history still returns is_smooth = power_efficient = true since
  // no data was successfully saved for the given configuration.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kProfile, kSize, kFrameRate, params.key_system,
                      params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

// Tests that the feature parameters are used to override constants for the
// Media Capabilities feature.
// To avoid race conditions when setting the parameter, the test sets it when
// starting and make sure the values recorded to the DB wouldn't be smooth per
// the default value.
TEST_P(VideoDecodePerfHistoryParamTest,
       SmoothThresholdFinchOverride_NoEmeOverride) {
  base::test::ScopedFeatureList scoped_feature_list;

  // EME and non EME threshold should initially be the same (neither is
  // overridden).
  double previous_smooth_dropped_frames_threshold =
      GetMaxSmoothDroppedFramesPercent(false /* is_eme */);
  EXPECT_EQ(previous_smooth_dropped_frames_threshold,
            GetMaxSmoothDroppedFramesPercent(true /* is_eme */));

  double new_smooth_dropped_frames_threshold =
      previous_smooth_dropped_frames_threshold / 2;

  ASSERT_LT(new_smooth_dropped_frames_threshold,
            previous_smooth_dropped_frames_threshold);

  // Override field trial.
  base::FieldTrialParams trial_params;
  trial_params
      [VideoDecodePerfHistory::kMaxSmoothDroppedFramesPercentParamName] =
          base::NumberToString(new_smooth_dropped_frames_threshold);
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaCapabilitiesWithParameters, trial_params);

  EXPECT_EQ(GetFieldTrialParams(), trial_params);

  // Non EME threshold is overridden.
  EXPECT_EQ(new_smooth_dropped_frames_threshold,
            GetMaxSmoothDroppedFramesPercent(false /* is_eme */));

  // EME threshold is also implicitly overridden (we didn't set an EME specific
  // value, so it should defer to the non-EME override).
  EXPECT_EQ(new_smooth_dropped_frames_threshold,
            GetMaxSmoothDroppedFramesPercent(true /* is_eme */));

  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  PerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/* success */ true);

  // First add 2 records to the history. The second record has a higher frame
  // rate and a higher number of dropped frames such that it is "not smooth".
  const VideoCodecProfile kKnownProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kKownSize(100, 200);
  const int kSmoothFrameRatePrevious = 30;
  const int kSmoothFrameRateNew = 90;
  const int kFramesDecoded = 1000;
  const int kNotPowerEfficientFramesDecoded = 0;

  // Sets the ratio of dropped frames to qualify as smooth per the default
  // threshold.
  const int kSmoothFramesDroppedPrevious =
      kFramesDecoded * previous_smooth_dropped_frames_threshold;
  // Sets the ratio of dropped frames to quality as smooth per the new
  // threshold.
  const int kSmoothFramesDroppedNew =
      kFramesDecoded * new_smooth_dropped_frames_threshold;

  // Add the entry.
  SavePerfRecord(
      UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
      MakeFeatures(kKnownProfile, kKownSize, kSmoothFrameRatePrevious,
                   params.key_system, params.use_hw_secure_codecs),
      MakeTargets(kFramesDecoded, kSmoothFramesDroppedPrevious,
                  kNotPowerEfficientFramesDecoded),
      kPlayerId);

  SavePerfRecord(UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
                 MakeFeatures(kKnownProfile, kKownSize, kSmoothFrameRateNew,
                              params.key_system, params.use_hw_secure_codecs),
                 MakeTargets(kFramesDecoded, kSmoothFramesDroppedNew,
                             kNotPowerEfficientFramesDecoded),
                 kPlayerId);

  // Verify perf history returns is_smooth = false for entry that would be
  // smooth per previous smooth threshold.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kKnownProfile, kKownSize, kSmoothFrameRatePrevious,
                      params.key_system, params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = true for entry that would be
  // smooth per new smooth threshold.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kKnownProfile, kKownSize, kSmoothFrameRateNew,
                      params.key_system, params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

TEST_P(VideoDecodePerfHistoryParamTest,
       SmoothThresholdFinchOverride_WithEmeOverride) {
  base::test::ScopedFeatureList scoped_feature_list;

  // EME and non EME threshold should initially be the same (neither is
  // overridden).
  double previous_smooth_dropped_frames_threshold =
      GetMaxSmoothDroppedFramesPercent(false /* is_eme */);
  EXPECT_EQ(previous_smooth_dropped_frames_threshold,
            GetMaxSmoothDroppedFramesPercent(true /* is_eme */));

  double new_CLEAR_smooth_dropped_frames_threshold =
      previous_smooth_dropped_frames_threshold / 2;
  double new_EME_smooth_dropped_frames_threshold =
      previous_smooth_dropped_frames_threshold / 3;

  ASSERT_LT(new_CLEAR_smooth_dropped_frames_threshold,
            previous_smooth_dropped_frames_threshold);
  ASSERT_LT(new_EME_smooth_dropped_frames_threshold,
            new_CLEAR_smooth_dropped_frames_threshold);

  // Override field trial.
  base::FieldTrialParams trial_params;
  trial_params
      [VideoDecodePerfHistory::kMaxSmoothDroppedFramesPercentParamName] =
          base::NumberToString(new_CLEAR_smooth_dropped_frames_threshold);
  trial_params
      [VideoDecodePerfHistory::kEmeMaxSmoothDroppedFramesPercentParamName] =
          base::NumberToString(new_EME_smooth_dropped_frames_threshold);

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaCapabilitiesWithParameters, trial_params);

  EXPECT_EQ(GetFieldTrialParams(), trial_params);

  // Both thresholds should be overridden.
  EXPECT_EQ(new_CLEAR_smooth_dropped_frames_threshold,
            GetMaxSmoothDroppedFramesPercent(false /* is_eme */));
  EXPECT_EQ(new_EME_smooth_dropped_frames_threshold,
            GetMaxSmoothDroppedFramesPercent(true /* is_eme */));

  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  PerfHistoryTestParams params = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!params.defer_initialize)
    PreInitializeDB(/* success */ true);

  // First add 2 records to the history. The second record has a higher frame
  // rate and a higher number of dropped frames such that it is "not smooth".
  const VideoCodecProfile kKnownProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kKownSize(100, 200);
  const int kSmoothFrameRatePrevious = 30;
  const int kSmoothFrameRateNew = 90;
  const int kFramesDecoded = 1000;
  const int kNotPowerEfficientFramesDecoded = 0;

  // Sets the ratio of dropped frames to qualify as NOT smooth. For CLEAR, use
  // the previous smooth threshold. For EME, use the new CLEAR threshold to
  // verify that the EME threshold is lower than CLEAR.
  const int kSmoothFramesDroppedPrevious =
      params.key_system.empty()
          ? kFramesDecoded * previous_smooth_dropped_frames_threshold
          : kFramesDecoded * new_CLEAR_smooth_dropped_frames_threshold;
  // Sets the ratio of dropped frames to quality as smooth per the new threshold
  // depending on whether the key indicates this record is EME.
  const int kSmoothFramesDroppedNew =
      params.key_system.empty()
          ? kFramesDecoded * new_CLEAR_smooth_dropped_frames_threshold
          : kFramesDecoded * new_EME_smooth_dropped_frames_threshold;

  // Add the entry.
  SavePerfRecord(
      UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
      MakeFeatures(kKnownProfile, kKownSize, kSmoothFrameRatePrevious,
                   params.key_system, params.use_hw_secure_codecs),
      MakeTargets(kFramesDecoded, kSmoothFramesDroppedPrevious,
                  kNotPowerEfficientFramesDecoded),
      kPlayerId);

  SavePerfRecord(UkmVerifcation::kSaveTriggersUkm, kOrigin, kIsTopFrame,
                 MakeFeatures(kKnownProfile, kKownSize, kSmoothFrameRateNew,
                              params.key_system, params.use_hw_secure_codecs),
                 MakeTargets(kFramesDecoded, kSmoothFramesDroppedNew,
                             kNotPowerEfficientFramesDecoded),
                 kPlayerId);

  // Verify perf history returns is_smooth = false for entry that would be
  // smooth per previous smooth threshold.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kKnownProfile, kKownSize, kSmoothFrameRatePrevious,
                      params.key_system, params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = true for entry that would be
  // smooth per new smooth threshold.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      MakeFeaturesPtr(kKnownProfile, kKownSize, kSmoothFrameRateNew,
                      params.key_system, params.use_hw_secure_codecs),
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (params.defer_initialize) {
    GetFakeDB()->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    task_environment_.RunUntilIdle();
  }
}

const PerfHistoryTestParams kPerfHistoryTestParams[] = {
    {true, "", false},
    {false, "", false},
    {true, "com.widevine.alpha", false},
    {true, "com.widevine.alpha", true},
};

INSTANTIATE_TEST_SUITE_P(VaryDBInitTiming,
                         VideoDecodePerfHistoryParamTest,
                         ::testing::ValuesIn(kPerfHistoryTestParams));

//
// The following test are not parameterized. They instead always hard code
// deferred initialization.
//

TEST_F(VideoDecodePerfHistoryTest, ClearHistoryTriggersSuccessfulInitialize) {
  // Clear the DB. Completion callback shouldn't fire until initialize
  // completes.
  EXPECT_CALL(*this, MockOnClearedHistory()).Times(0);
  perf_history_->ClearHistory(
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockOnClearedHistory,
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

TEST_F(VideoDecodePerfHistoryTest, ClearHistoryTriggersFailedInitialize) {
  // Clear the DB. Completion callback shouldn't fire until initialize
  // completes.
  EXPECT_CALL(*this, MockOnClearedHistory()).Times(0);
  perf_history_->ClearHistory(
      base::BindOnce(&VideoDecodePerfHistoryParamTest::MockOnClearedHistory,
                     base::Unretained(this)));

  // Give completion callback a chance to fire. Confirm it did not fire.
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  // Expect completion callback after completing initialize. "Failure" is still
  // a form of completion.
  EXPECT_CALL(*this, MockOnClearedHistory());
  GetFakeDB()->CompleteInitialize(false);

  // Give deferred callback a chance to fire.
  task_environment_.RunUntilIdle();
}

}  // namespace media

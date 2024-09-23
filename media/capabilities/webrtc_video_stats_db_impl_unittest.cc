// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/webrtc_video_stats.pb.h"
#include "media/capabilities/webrtc_video_stats_db_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using leveldb_proto::test::FakeDB;
using testing::_;
using testing::Eq;
using testing::Pointee;

namespace media {

class WebrtcVideoStatsDBImplTest : public ::testing::Test {
 public:
  using VideoDescKey = WebrtcVideoStatsDB::VideoDescKey;
  using VideoStats = WebrtcVideoStatsDB::VideoStats;
  using VideoStatsEntry = WebrtcVideoStatsDB::VideoStatsEntry;

  WebrtcVideoStatsDBImplTest()
      : kDecodeStatsKeyVp9(
            VideoDescKey::MakeBucketedKey(/*is_decode_stats=*/true,
                                          VP9PROFILE_PROFILE3,
                                          /*hardware_accelerated=*/false,
                                          1280 * 720)),
        kDecodeStatsKeyVp9Hw(
            VideoDescKey::MakeBucketedKey(/*is_decode_stats=*/true,
                                          VP9PROFILE_PROFILE3,
                                          /*hardware_accelerated=*/true,
                                          1280 * 720)),
        kDecodeStatsKeyVp9FullHd(
            VideoDescKey::MakeBucketedKey(/*is_decode_stats=*/true,
                                          VP9PROFILE_PROFILE3,
                                          /*hardware_accelerated=*/false,
                                          1920 * 1080)),
        kDecodeStatsKeyVp94K(
            VideoDescKey::MakeBucketedKey(/*is_decode_stats=*/true,
                                          VP9PROFILE_PROFILE3,
                                          /*hardware_accelerated=*/false,
                                          3840 * 2160)),
        kDecodeStatsKeyH264(
            VideoDescKey::MakeBucketedKey(/*is_decode_stats=*/true,
                                          H264PROFILE_MIN,
                                          /*hardware_accelerated=*/false,
                                          1920 * 1080)),
        kEncodeStatsKeyVp9(
            VideoDescKey::MakeBucketedKey(/*is_decode_stats=*/false,
                                          VP9PROFILE_PROFILE3,
                                          /*hardware_accelerated=*/false,
                                          1280 * 720)) {
    // Fake DB simply wraps a std::map with the LevelDB interface. We own the
    // map and will delete it in TearDown().
    fake_db_map_ =
        std::make_unique<FakeDB<WebrtcVideoStatsEntryProto>::EntryMap>();
    // `stats_db_` will own this pointer, but we hold a reference to control
    // its behavior.
    fake_db_ = new FakeDB<WebrtcVideoStatsEntryProto>(fake_db_map_.get());

    // Wrap the fake proto DB with our interface.
    stats_db_ = base::WrapUnique(new WebrtcVideoStatsDBImpl(
        std::unique_ptr<FakeDB<WebrtcVideoStatsEntryProto>>(fake_db_)));
  }

  WebrtcVideoStatsDBImplTest(const WebrtcVideoStatsDBImplTest&) = delete;
  WebrtcVideoStatsDBImplTest& operator=(const WebrtcVideoStatsDBImplTest&) =
      delete;

  ~WebrtcVideoStatsDBImplTest() override {
    // Tests should always complete any pending operations
    VerifyNoPendingOps();
  }

  void VerifyOnePendingOp(std::string_view op_name) {
    EXPECT_EQ(stats_db_->pending_operations_.get_pending_ops_for_test().size(),
              1u);
    PendingOperations::PendingOperation* pending_op =
        stats_db_->pending_operations_.get_pending_ops_for_test()
            .begin()
            ->second.get();
    EXPECT_TRUE(pending_op->uma_str_.ends_with(op_name));
  }

  void VerifyNoPendingOps() {
    EXPECT_TRUE(
        stats_db_->pending_operations_.get_pending_ops_for_test().empty());
  }

  base::TimeDelta GetMaxTimeToKeepStats() {
    return WebrtcVideoStatsDBImpl::GetMaxTimeToKeepStats();
  }

  int GetMaxEntriesPerConfig() {
    return WebrtcVideoStatsDBImpl::GetMaxEntriesPerConfig();
  }

  void SetDBClock(base::Clock* clock) {
    stats_db_->set_wall_clock_for_test(clock);
  }

  void InitializeDB() {
    stats_db_->Initialize(base::BindOnce(
        &WebrtcVideoStatsDBImplTest::OnInitialize, base::Unretained(this)));
    EXPECT_CALL(*this, OnInitialize(true));
    fake_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void AppendStats(const VideoDescKey& key, const VideoStats& entry) {
    EXPECT_CALL(*this, MockAppendVideoStatsCb(true));
    stats_db_->AppendVideoStats(
        key, entry,
        base::BindOnce(&WebrtcVideoStatsDBImplTest::MockAppendVideoStatsCb,
                       base::Unretained(this)));
    VerifyOnePendingOp("Read");
    fake_db_->GetCallback(true);
    VerifyOnePendingOp("Write");
    fake_db_->UpdateCallback(true);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void VerifyReadStats(const VideoDescKey& key,
                       const VideoStatsEntry& expected) {
    EXPECT_CALL(*this, MockGetVideoStatsCb(true, std::make_optional(expected)));
    stats_db_->GetVideoStats(
        key, base::BindOnce(&WebrtcVideoStatsDBImplTest::MockGetVideoStatsCb,
                            base::Unretained(this)));
    VerifyOnePendingOp("Read");
    fake_db_->GetCallback(true);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void VerifyReadStatsCollection(
      const VideoDescKey& key,
      const WebrtcVideoStatsDB::VideoStatsCollection& expected) {
    EXPECT_CALL(*this, MockGetVideoStatsCollectionCb(
                           true, std::make_optional(expected)));
    stats_db_->GetVideoStatsCollection(
        key, base::BindOnce(
                 &WebrtcVideoStatsDBImplTest::MockGetVideoStatsCollectionCb,
                 base::Unretained(this)));
    VerifyOnePendingOp("Read");
    fake_db_->LoadCallback(true);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void VerifyEmptyStats(const VideoDescKey& key) {
    EXPECT_CALL(*this,
                MockGetVideoStatsCb(true, std::optional<VideoStatsEntry>()));
    stats_db_->GetVideoStats(
        key, base::BindOnce(&WebrtcVideoStatsDBImplTest::MockGetVideoStatsCb,
                            base::Unretained(this)));
    VerifyOnePendingOp("Read");
    fake_db_->GetCallback(true);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void AppendToProtoDB(const VideoDescKey& key,
                       const WebrtcVideoStatsEntryProto* const proto) {
    base::RunLoop run_loop;
    base::OnceCallback<void(bool)> update_done_cb = base::BindOnce(
        [](base::RunLoop* run_loop, bool success) {
          ASSERT_TRUE(success);
          run_loop->Quit();
        },
        Unretained(&run_loop));

    using DBType = leveldb_proto::ProtoDatabase<WebrtcVideoStatsEntryProto>;
    std::unique_ptr<DBType::KeyEntryVector> entries =
        std::make_unique<DBType::KeyEntryVector>();
    entries->emplace_back(key.Serialize(), *proto);

    fake_db_->UpdateEntries(std::move(entries),
                            std::make_unique<leveldb_proto::KeyVector>(),
                            std::move(update_done_cb));

    fake_db_->UpdateCallback(true);
    run_loop.Run();
  }

  MOCK_METHOD1(OnInitialize, void(bool success));

  MOCK_METHOD2(MockGetVideoStatsCb,
               void(bool success, std::optional<VideoStatsEntry> entry));

  MOCK_METHOD2(
      MockGetVideoStatsCollectionCb,
      void(bool success,
           std::optional<WebrtcVideoStatsDB::VideoStatsCollection> collection));

  MOCK_METHOD1(MockAppendVideoStatsCb, void(bool success));

  MOCK_METHOD0(MockClearStatsCb, void());

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const VideoDescKey kDecodeStatsKeyVp9;
  const VideoDescKey kDecodeStatsKeyVp9Hw;
  const VideoDescKey kDecodeStatsKeyVp9FullHd;
  const VideoDescKey kDecodeStatsKeyVp94K;
  const VideoDescKey kDecodeStatsKeyH264;
  const VideoDescKey kEncodeStatsKeyVp9;

  // See documentation in SetUp()
  std::unique_ptr<FakeDB<WebrtcVideoStatsEntryProto>::EntryMap> fake_db_map_;
  raw_ptr<FakeDB<WebrtcVideoStatsEntryProto>, DanglingUntriaged> fake_db_;
  std::unique_ptr<WebrtcVideoStatsDBImpl> stats_db_;
};

TEST_F(WebrtcVideoStatsDBImplTest, InitializeFailed) {
  stats_db_->Initialize(base::BindOnce(
      &WebrtcVideoStatsDBImplTest::OnInitialize, base::Unretained(this)));
  EXPECT_CALL(*this, OnInitialize(false));
  fake_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
}

TEST_F(WebrtcVideoStatsDBImplTest, InitializeTimedOut) {
  // Queue up an Initialize.
  stats_db_->Initialize(base::BindOnce(
      &WebrtcVideoStatsDBImplTest::OnInitialize, base::Unretained(this)));
  VerifyOnePendingOp("Initialize");

  // Move time forward enough to trigger timeout.
  EXPECT_CALL(*this, OnInitialize(_)).Times(0);
  task_environment_.FastForwardBy(base::Seconds(100));
  task_environment_.RunUntilIdle();

  // Verify we didn't get an init callback and task is no longer considered
  // pending (because it timed out).
  testing::Mock::VerifyAndClearExpectations(this);
  VerifyNoPendingOps();

  // Verify callback still works if init completes very late.
  EXPECT_CALL(*this, OnInitialize(false));
  fake_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
}

TEST_F(WebrtcVideoStatsDBImplTest, ReadExpectingNothing) {
  InitializeDB();
  VerifyEmptyStats(kDecodeStatsKeyVp9);
}

TEST_F(WebrtcVideoStatsDBImplTest, WriteReadAndClear) {
  InitializeDB();

  // Set test clock.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Append and read back some VP9 stats.
  VideoStats stats1(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                    240, 6, 7.2);
  VideoStatsEntry entry{stats1};
  AppendStats(kDecodeStatsKeyVp9, stats1);
  VerifyReadStats(kDecodeStatsKeyVp9, entry);

  // Reading with the wrong key (different codec) should still return nothing.
  VerifyEmptyStats(kDecodeStatsKeyH264);
  VerifyEmptyStats(kEncodeStatsKeyVp9);

  // Appending new VP9 stats.
  clock.Advance(base::Hours(1));
  VideoStats stats2(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                    1000, 14, 6.8);

  AppendStats(kDecodeStatsKeyVp9, stats2);
  VideoStatsEntry aggregate_entry{stats2, stats1};
  VerifyReadStats(kDecodeStatsKeyVp9, aggregate_entry);

  // Clear all stats from the DB.
  EXPECT_CALL(*this, MockClearStatsCb);
  stats_db_->ClearStats(base::BindOnce(
      &WebrtcVideoStatsDBImplTest::MockClearStatsCb, base::Unretained(this)));
  VerifyOnePendingOp("Clear");
  fake_db_->UpdateCallback(true);

  // Database is now empty. Expect null entry.
  VerifyEmptyStats(kDecodeStatsKeyVp9);
}

TEST_F(WebrtcVideoStatsDBImplTest, ExpiredStatsAreNotReturned) {
  InitializeDB();

  // Set test clock.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Append and read back some VP9 stats.
  VideoStats stats1(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                    240, 6, 7.2);
  VideoStatsEntry entry{stats1};
  AppendStats(kDecodeStatsKeyVp9, stats1);
  VerifyReadStats(kDecodeStatsKeyVp9, entry);

  // Appending new VP9 stats.
  clock.Advance(base::Days(2));
  VideoStats stats2(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                    1000, 14, 6.8);
  clock.SetNow(base::Time::FromMillisecondsSinceUnixEpoch(stats2.timestamp));

  AppendStats(kDecodeStatsKeyVp9, stats2);
  VideoStatsEntry aggregate_entry{stats2, stats1};
  VerifyReadStats(kDecodeStatsKeyVp9, aggregate_entry);

  // Set the clock to a date so that the first entry is expired.
  clock.SetNow(base::Time::FromMillisecondsSinceUnixEpoch(stats1.timestamp) +
               base::Days(1) + GetMaxTimeToKeepStats());
  VideoStatsEntry nonexpired_entry{stats2};
  VerifyReadStats(kDecodeStatsKeyVp9, nonexpired_entry);

  // Set the clock so that all data have expired.
  clock.SetNow(base::Time::FromMillisecondsSinceUnixEpoch(stats2.timestamp) +
               base::Days(1) + GetMaxTimeToKeepStats());

  // All stats are expired. Expect null entry.
  VerifyEmptyStats(kDecodeStatsKeyVp9);
}

TEST_F(WebrtcVideoStatsDBImplTest, ConfigureExpireDays) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<base::FieldTrialList> field_trial_list;

  base::TimeDelta previous_max_days_to_keep_stats = GetMaxTimeToKeepStats();
  constexpr int kNewMaxDaysToKeepStats = 4;
  ASSERT_LT(base::Days(kNewMaxDaysToKeepStats),
            previous_max_days_to_keep_stats);

  // Override field trial.
  base::FieldTrialParams params;
  params["db_days_to_keep_stats"] =
      base::NumberToString(kNewMaxDaysToKeepStats);
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kWebrtcMediaCapabilitiesParameters, params);
  EXPECT_EQ(base::Days(kNewMaxDaysToKeepStats), GetMaxTimeToKeepStats());

  InitializeDB();

  // Inject a test clock and initialize with the current time.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Append and verify read-back.
  VideoStats stats1(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                    240, 6, 7.2);
  VideoStatsEntry entry{stats1};
  AppendStats(kDecodeStatsKeyVp9, stats1);
  VerifyReadStats(kDecodeStatsKeyVp9, entry);

  // Some simple math to avoid troubles of integer division.
  int half_days_to_keep_stats = kNewMaxDaysToKeepStats / 2;
  int remaining_days_to_keep_stats =
      kNewMaxDaysToKeepStats - half_days_to_keep_stats;

  // Advance time half way through grace period. Verify stats not expired.
  clock.Advance(base::Days(half_days_to_keep_stats));
  VerifyReadStats(kDecodeStatsKeyVp9, entry);

  // Advance time 1 day beyond grace period, verify stats are expired.
  clock.Advance(base::Days(remaining_days_to_keep_stats + 1));
  VerifyEmptyStats(kDecodeStatsKeyVp9);

  // Advance the clock 100 extra days. Verify stats still expired.
  clock.Advance(base::Days(100));
  VerifyEmptyStats(kDecodeStatsKeyVp9);
}

TEST_F(WebrtcVideoStatsDBImplTest, NewStatsReplaceOldStats) {
  InitializeDB();

  // Inject a test clock and initialize with the current time.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Append and verify read-back.
  constexpr int kNumberOfStatsToAdd = 30;
  EXPECT_GT(kNumberOfStatsToAdd, GetMaxEntriesPerConfig());
  VideoStatsEntry entry;
  for (int i = 0; i < kNumberOfStatsToAdd; ++i) {
    VideoStats stats(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                     240 + i, 6, 7.2 + i % 3);
    AppendStats(kDecodeStatsKeyVp9, stats);
    // Start popping the last stats entry if the number of entries has reached
    // the limit.
    if (i >= GetMaxEntriesPerConfig()) {
      entry.pop_back();
    }
    entry.insert(entry.begin(), stats);
    VerifyReadStats(kDecodeStatsKeyVp9, entry);
    clock.Advance(base::Days(1));
  }
}

TEST_F(WebrtcVideoStatsDBImplTest, ConfigureMaxEntriesPerConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<base::FieldTrialList> field_trial_list;

  int previous_max_entries_per_config = GetMaxEntriesPerConfig();
  constexpr int kNewMaxEntriesPerConfig = 3;
  ASSERT_LT(kNewMaxEntriesPerConfig, previous_max_entries_per_config);

  // Override field trial.
  base::FieldTrialParams params;
  params["db_max_entries_per_cpnfig"] =
      base::NumberToString(kNewMaxEntriesPerConfig);
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kWebrtcMediaCapabilitiesParameters, params);
  EXPECT_EQ(kNewMaxEntriesPerConfig, GetMaxEntriesPerConfig());

  InitializeDB();

  // Inject a test clock and initialize with the current time.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Append and verify read-back.
  constexpr int kNumberOfStatsToAdd = 30;
  EXPECT_GT(kNumberOfStatsToAdd, GetMaxEntriesPerConfig());
  VideoStatsEntry entry;
  for (int i = 0; i < kNumberOfStatsToAdd; ++i) {
    VideoStats stats(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                     240 + i, 6, 7.2 + i % 3);
    AppendStats(kDecodeStatsKeyVp9, stats);
    // Start popping the last stats entry if the number of entries has reached
    // the limit.
    if (i >= GetMaxEntriesPerConfig()) {
      entry.pop_back();
    }
    entry.insert(entry.begin(), stats);
    VerifyReadStats(kDecodeStatsKeyVp9, entry);
    clock.Advance(base::Days(1));
  }
}

TEST_F(WebrtcVideoStatsDBImplTest, OutOfOrderTimestampClearsOldStats) {
  InitializeDB();

  // Inject a test clock and initialize with the current time.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Append and verify read-back.
  constexpr int kNumberOfStatsToAdd = 5;
  VideoStatsEntry entry;
  for (int i = 0; i < kNumberOfStatsToAdd; ++i) {
    VideoStats stats(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                     240 + i, 6, 7.2 + i % 3);
    AppendStats(kDecodeStatsKeyVp9, stats);
    entry.insert(entry.begin(), stats);
    VerifyReadStats(kDecodeStatsKeyVp9, entry);
    clock.Advance(base::Days(1));
  }

  // Go back in time and add a new stats entry.
  clock.Advance(-base::Days(20));
  VideoStats stats(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(), 123,
                   5, 11.2);
  AppendStats(kDecodeStatsKeyVp9, stats);
  // Only the last appended stats should be in the database now.
  entry = {stats};
  VerifyReadStats(kDecodeStatsKeyVp9, entry);
}

TEST_F(WebrtcVideoStatsDBImplTest, FailedWrite) {
  InitializeDB();

  // Expect the callback to indicate success = false when the write fails.
  EXPECT_CALL(*this, MockAppendVideoStatsCb(false));

  // Append stats, but fail the internal DB update.
  stats_db_->AppendVideoStats(
      kDecodeStatsKeyVp9, VideoStats(1234, 240, 6, 7.2),
      base::BindOnce(&WebrtcVideoStatsDBImplTest::MockAppendVideoStatsCb,
                     base::Unretained(this)));
  fake_db_->GetCallback(true);
  fake_db_->UpdateCallback(false);
}

TEST_F(WebrtcVideoStatsDBImplTest, DiscardCorruptedDBData) {
  InitializeDB();

  // Inject a test clock and initialize with the current time.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Start with a proto that represents a valid uncorrupted and unexpired entry.
  WebrtcVideoStatsEntryProto valid_proto;
  WebrtcVideoStatsProto* valid_entry = valid_proto.add_stats();
  valid_entry->set_timestamp(
      clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull());
  valid_entry->set_frames_processed(300);
  valid_entry->set_key_frames_processed(8);
  valid_entry->set_p99_processing_time_ms(11.3);

  // Append it and read it back without issue.
  AppendToProtoDB(kEncodeStatsKeyVp9, &valid_proto);
  VerifyReadStats(
      kEncodeStatsKeyVp9,
      {VideoStats{valid_entry->timestamp(), valid_entry->frames_processed(),
                  valid_entry->key_frames_processed(),
                  valid_entry->p99_processing_time_ms()}});

  WebrtcVideoStatsEntryProto invalid_proto;
  WebrtcVideoStatsProto* invalid_entry = invalid_proto.add_stats();
  //  Invalid because number of frames processed is too low Verify
  // you can't read it back (filtered for corruption).
  *invalid_entry = *valid_entry;
  invalid_entry->set_frames_processed(30);
  AppendToProtoDB(kDecodeStatsKeyVp9, &invalid_proto);
  VerifyEmptyStats(kDecodeStatsKeyVp9);

  // Invalid because number of frames processed is too high. Verify
  // you can't read it back (filtered for corruption).
  *invalid_entry = *valid_entry;
  invalid_entry->set_frames_processed(1000000);
  AppendToProtoDB(kDecodeStatsKeyVp9, &invalid_proto);
  VerifyEmptyStats(kDecodeStatsKeyVp9);

  // Invalid because number of key frames is higher than number of frames
  // processed. Verify you can't read it back (filtered for corruption).
  *invalid_entry = *valid_entry;
  invalid_entry->set_key_frames_processed(valid_entry->frames_processed() + 1);
  AppendToProtoDB(kDecodeStatsKeyVp9, &invalid_proto);
  VerifyEmptyStats(kDecodeStatsKeyVp9);

  // Invalid processing time. Verify
  // you can't read it back (filtered for corruption).
  *invalid_entry = *valid_entry;
  invalid_entry->set_p99_processing_time_ms(-1.0);
  AppendToProtoDB(kDecodeStatsKeyVp9, &invalid_proto);
  VerifyEmptyStats(kDecodeStatsKeyVp9);

  *invalid_entry = *valid_entry;
  invalid_entry->set_p99_processing_time_ms(20000.0);
  AppendToProtoDB(kDecodeStatsKeyVp9, &invalid_proto);
  VerifyEmptyStats(kDecodeStatsKeyVp9);
}

TEST_F(WebrtcVideoStatsDBImplTest, WriteAndReadCollection) {
  InitializeDB();

  // Set test clock.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Append stats for multiple resolutions.
  VideoStats stats1(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                    240, 6, 7.2);
  VideoStatsEntry entry1{stats1};
  AppendStats(kDecodeStatsKeyVp9, stats1);

  VideoStats stats2(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                    360, 7, 9.2);
  VideoStatsEntry entry2{stats2};
  AppendStats(kDecodeStatsKeyVp9FullHd, stats2);

  VideoStats stats3(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                    480, 11, 13.3);
  VideoStatsEntry entry3{stats3};
  AppendStats(kDecodeStatsKeyVp94K, stats3);

  // Add elements that should not be returned.
  VideoStats stats4(clock.Now().InMillisecondsFSinceUnixEpochIgnoringNull(),
                    490, 13, 15.3);
  AppendStats(kEncodeStatsKeyVp9, stats4);
  AppendStats(kDecodeStatsKeyVp9Hw, stats4);

  // Created the expected collection that should be returned.
  WebrtcVideoStatsDB::VideoStatsCollection expected;
  expected.insert({kDecodeStatsKeyVp9.pixels, entry1});
  expected.insert({kDecodeStatsKeyVp9FullHd.pixels, entry2});
  expected.insert({kDecodeStatsKeyVp94K.pixels, entry3});

  // The same collection is returned for all keys that are associated to the
  // collection.
  VerifyReadStatsCollection(kDecodeStatsKeyVp9, expected);
  VerifyReadStatsCollection(kDecodeStatsKeyVp9FullHd, expected);
  VerifyReadStatsCollection(kDecodeStatsKeyVp94K, expected);
}

}  // namespace media

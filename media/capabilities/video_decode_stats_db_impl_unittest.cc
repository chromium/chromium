// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/video_decode_stats.pb.h"
#include "media/capabilities/video_decode_stats_db_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using leveldb_proto::test::FakeDB;
using testing::_;
using testing::Eq;
using testing::Pointee;

namespace media {

class VideoDecodeStatsDBImplTest : public ::testing::Test {
 public:
  using VideoDescKey = VideoDecodeStatsDB::VideoDescKey;
  using DecodeStatsEntry = VideoDecodeStatsDB::DecodeStatsEntry;

  VideoDecodeStatsDBImplTest()
      : kStatsKeyVp9(VideoDescKey::MakeBucketedKey(VP9PROFILE_PROFILE3,
                                                   gfx::Size(1024, 768),
                                                   60,
                                                   "com.widevine.alpha",
                                                   true)),
        kStatsKeyAvc(VideoDescKey::MakeBucketedKey(H264PROFILE_MIN,
                                                   gfx::Size(1024, 768),
                                                   60,
                                                   "",
                                                   false)) {
    bool parsed_time = base::Time::FromString(
        VideoDecodeStatsDBImpl::kDefaultWriteTime, &kDefaultWriteTime);
    DCHECK(parsed_time);

    // Fake DB simply wraps a std::map with the LevelDB interface. We own the
    // map and will delete it in TearDown().
    fake_db_map_ = std::make_unique<FakeDB<DecodeStatsProto>::EntryMap>();
    // |stats_db_| will own this pointer, but we hold a reference to control
    // its behavior.
    fake_db_ = new FakeDB<DecodeStatsProto>(fake_db_map_.get());

    // Wrap the fake proto DB with our interface.
    stats_db_ = base::WrapUnique(new VideoDecodeStatsDBImpl(
        std::unique_ptr<FakeDB<DecodeStatsProto>>(fake_db_)));
  }

  int GetMaxFramesPerBuffer() {
    return VideoDecodeStatsDBImpl::GetMaxFramesPerBuffer();
  }

  int GetMaxDaysToKeepStats() {
    return VideoDecodeStatsDBImpl::GetMaxDaysToKeepStats();
  }

  bool GetEnableUnweightedEntries() {
    return VideoDecodeStatsDBImpl::GetEnableUnweightedEntries();
  }

  void SetDBClock(base::Clock* clock) {
    stats_db_->set_wall_clock_for_test(clock);
  }

  void InitializeDB() {
    stats_db_->Initialize(base::BindOnce(
        &VideoDecodeStatsDBImplTest::OnInitialize, base::Unretained(this)));
    EXPECT_CALL(*this, OnInitialize(true));
    fake_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void AppendStats(const VideoDescKey& key, const DecodeStatsEntry& entry) {
    EXPECT_CALL(*this, MockAppendDecodeStatsCb(true));
    stats_db_->AppendDecodeStats(
        key, entry,
        base::BindOnce(&VideoDecodeStatsDBImplTest::MockAppendDecodeStatsCb,
                       base::Unretained(this)));
    fake_db_->GetCallback(true);
    fake_db_->UpdateCallback(true);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void VerifyReadStats(const VideoDescKey& key,
                       const DecodeStatsEntry& expected) {
    EXPECT_CALL(*this, MockGetDecodeStatsCb(true, Pointee(Eq(expected))));
    stats_db_->GetDecodeStats(
        key, base::BindOnce(&VideoDecodeStatsDBImplTest::GetDecodeStatsCb,
                            base::Unretained(this)));
    fake_db_->GetCallback(true);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void VerifyEmptyStats(const VideoDescKey& key) {
    EXPECT_CALL(*this, MockGetDecodeStatsCb(true, nullptr));
    stats_db_->GetDecodeStats(
        key, base::BindOnce(&VideoDecodeStatsDBImplTest::GetDecodeStatsCb,
                            base::Unretained(this)));
    fake_db_->GetCallback(true);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  // Unwraps move-only parameters to pass to the mock function.
  void GetDecodeStatsCb(bool success, std::unique_ptr<DecodeStatsEntry> entry) {
    MockGetDecodeStatsCb(success, entry.get());
  }

  void AppendToProtoDB(const VideoDescKey& key,
                       const DecodeStatsProto* const proto) {
    base::RunLoop run_loop;
    base::OnceCallback<void(bool)> update_done_cb = base::BindOnce(
        [](base::RunLoop* run_loop, bool success) {
          ASSERT_TRUE(success);
          run_loop->Quit();
        },
        Unretained(&run_loop));

    using DBType = leveldb_proto::ProtoDatabase<DecodeStatsProto>;
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

  MOCK_METHOD2(MockGetDecodeStatsCb,
               void(bool success, DecodeStatsEntry* entry));

  MOCK_METHOD1(MockAppendDecodeStatsCb, void(bool success));

  MOCK_METHOD0(MockClearStatsCb, void());

 protected:
  base::test::TaskEnvironment task_environment_;

  const VideoDescKey kStatsKeyVp9;
  const VideoDescKey kStatsKeyAvc;

  // Const in practice, but not marked const for compatibility with
  // base::Time::FromString.
  base::Time kDefaultWriteTime;

  // See documentation in SetUp()
  std::unique_ptr<FakeDB<DecodeStatsProto>::EntryMap> fake_db_map_;
  FakeDB<DecodeStatsProto>* fake_db_;
  std::unique_ptr<VideoDecodeStatsDBImpl> stats_db_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDecodeStatsDBImplTest);
};

TEST_F(VideoDecodeStatsDBImplTest, FailedInitialize) {
  stats_db_->Initialize(base::BindOnce(
      &VideoDecodeStatsDBImplTest::OnInitialize, base::Unretained(this)));
  EXPECT_CALL(*this, OnInitialize(false));
  fake_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
}

TEST_F(VideoDecodeStatsDBImplTest, ReadExpectingNothing) {
  InitializeDB();
  VerifyEmptyStats(kStatsKeyVp9);
}

TEST_F(VideoDecodeStatsDBImplTest, WriteReadAndClear) {
  InitializeDB();

  // Append and read back some VP9 stats.
  DecodeStatsEntry entry(1000, 2, 10);
  AppendStats(kStatsKeyVp9, entry);
  VerifyReadStats(kStatsKeyVp9, entry);

  // Reading with the wrong key (different codec) should still return nothing.
  VerifyEmptyStats(kStatsKeyAvc);

  // Appending same VP9 stats should read back as 2x the initial entry.
  AppendStats(kStatsKeyVp9, entry);
  DecodeStatsEntry aggregate_entry(2000, 4, 20);
  VerifyReadStats(kStatsKeyVp9, aggregate_entry);

  // Clear all stats from the DB.
  stats_db_->ClearStats(base::BindOnce(
      &VideoDecodeStatsDBImplTest::MockClearStatsCb, base::Unretained(this)));
  fake_db_->LoadKeysCallback(true);
  fake_db_->UpdateCallback(true);

  // Database is now empty. Expect null entry.
  VerifyEmptyStats(kStatsKeyVp9);
}

TEST_F(VideoDecodeStatsDBImplTest, ConfigureMaxFramesPerBuffer) {
  // Setup field trial to use a tiny window of 1 decoded frame.

  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<base::FieldTrialList> field_trial_list;

  int previous_max_frames_per_buffer = GetMaxFramesPerBuffer();
  int new_max_frames_per_buffer = 1;

  ASSERT_LT(new_max_frames_per_buffer, previous_max_frames_per_buffer);

  // Override field trial.
  base::FieldTrialParams params;
  params[VideoDecodeStatsDBImpl::kMaxFramesPerBufferParamName] =
      base::NumberToString(new_max_frames_per_buffer);

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaCapabilitiesWithParameters, params);

  base::FieldTrialParams actual_params;
  EXPECT_TRUE(base::GetFieldTrialParamsByFeature(
      media::kMediaCapabilitiesWithParameters, &actual_params));
  EXPECT_EQ(params, actual_params);

  EXPECT_EQ(new_max_frames_per_buffer, GetMaxFramesPerBuffer());

  // Now verify the configured window is used by writing a frame and then
  // writing another.

  InitializeDB();

  // Append single frame which was, sadly, dropped and not efficient.
  DecodeStatsEntry entry(1, 1, 0);
  AppendStats(kStatsKeyVp9, entry);
  VerifyReadStats(kStatsKeyVp9, entry);

  // Appending another frame which is *not* dropped and *is* efficient.
  // Verify that only this last entry is still in the buffer (no aggregation).
  DecodeStatsEntry second_entry(1, 0, 1);
  AppendStats(kStatsKeyVp9, second_entry);
  VerifyReadStats(kStatsKeyVp9, second_entry);
}

TEST_F(VideoDecodeStatsDBImplTest, ConfigureExpireDays) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<base::FieldTrialList> field_trial_list;

  int previous_max_days_to_keep_stats = GetMaxDaysToKeepStats();
  int new_max_days_to_keep_stats = 4;

  ASSERT_LT(new_max_days_to_keep_stats, previous_max_days_to_keep_stats);

  // Override field trial.
  base::FieldTrialParams params;
  params[VideoDecodeStatsDBImpl::kMaxDaysToKeepStatsParamName] =
      base::NumberToString(new_max_days_to_keep_stats);

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaCapabilitiesWithParameters, params);

  base::FieldTrialParams actual_params;
  EXPECT_TRUE(base::GetFieldTrialParamsByFeature(
      media::kMediaCapabilitiesWithParameters, &actual_params));
  EXPECT_EQ(params, actual_params);

  EXPECT_EQ(new_max_days_to_keep_stats, GetMaxDaysToKeepStats());

  InitializeDB();

  // Inject a test clock and initialize with the current time.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Append and verify read-back.
  AppendStats(kStatsKeyVp9, DecodeStatsEntry(200, 20, 2));
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(200, 20, 2));

  // Some simple math to avoid troubles of integer division.
  int half_days_to_keep_stats = new_max_days_to_keep_stats / 2;
  int remaining_days_to_keep_stats =
      new_max_days_to_keep_stats - half_days_to_keep_stats;

  // Advance time half way through grace period. Verify stats not expired.
  clock.Advance(base::TimeDelta::FromDays(half_days_to_keep_stats));
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(200, 20, 2));

  // Advance time 1 day beyond grace period, verify stats are expired.
  clock.Advance(base::TimeDelta::FromDays((remaining_days_to_keep_stats) + 1));
  VerifyEmptyStats(kStatsKeyVp9);

  // Advance the clock 100 extra days. Verify stats still expired.
  clock.Advance(base::TimeDelta::FromDays(100));
  VerifyEmptyStats(kStatsKeyVp9);
}

TEST_F(VideoDecodeStatsDBImplTest, FailedWrite) {
  InitializeDB();

  // Expect the callback to indicate success = false when the write fails.
  EXPECT_CALL(*this, MockAppendDecodeStatsCb(false));

  // Append stats, but fail the internal DB update.
  stats_db_->AppendDecodeStats(
      kStatsKeyVp9, DecodeStatsEntry(1000, 2, 10),
      base::BindOnce(&VideoDecodeStatsDBImplTest::MockAppendDecodeStatsCb,
                     base::Unretained(this)));
  fake_db_->GetCallback(true);
  fake_db_->UpdateCallback(false);
}

TEST_F(VideoDecodeStatsDBImplTest, FillBufferInMixedIncrements) {
  InitializeDB();

  // Setup DB entry that half fills the buffer with 10% of frames dropped and
  // 50% of frames power efficient.
  const int kNumFramesEntryA = GetMaxFramesPerBuffer() / 2;
  DecodeStatsEntry entryA(kNumFramesEntryA, std::round(0.1 * kNumFramesEntryA),
                          std::round(0.5 * kNumFramesEntryA));
  const double kDropRateA =
      static_cast<double>(entryA.frames_dropped) / entryA.frames_decoded;
  const double kEfficientRateA =
      static_cast<double>(entryA.frames_power_efficient) /
      entryA.frames_decoded;

  // Append entryA to half fill the buffer and verify read. Verify read.
  AppendStats(kStatsKeyVp9, entryA);
  VerifyReadStats(kStatsKeyVp9, entryA);

  // Append same entryA again to completely fill the buffer. Verify read gives
  // out aggregated stats (2x the initial entryA)
  AppendStats(kStatsKeyVp9, entryA);
  VerifyReadStats(
      kStatsKeyVp9,
      DecodeStatsEntry(GetMaxFramesPerBuffer(),
                       std::round(kDropRateA * GetMaxFramesPerBuffer()),
                       std::round(kEfficientRateA * GetMaxFramesPerBuffer())));

  // This row in the DB is now "full" (appended frames >= kMaxFramesPerBuffer)!
  //
  // Future appends will not increase the total count of decoded frames. The
  // ratios of dropped and power efficient frames will be a weighted average.
  // The weight of new appends is determined by how much of the buffer they
  // fill, i.e.
  //
  //   new_append_weight = min(1, new_append_frame_count / kMaxFramesPerBuffer);
  //   old_data_weight = 1 - new_append_weight;
  //
  // The calculation for dropped ratios (same for power efficient) then becomes:
  //
  //   aggregate_drop_ratio = old_drop_ratio * old_drop_weight +
  //                          new_drop_ratio * new_data_weight;
  //
  // See implementation for more details.

  // Append same entryA a 3rd time. Verify we still only get the aggregated
  // stats from above (2x entryA) because the buffer is full and the ratio of
  // dropped and efficient frames is the same.
  AppendStats(kStatsKeyVp9, entryA);
  VerifyReadStats(
      kStatsKeyVp9,
      DecodeStatsEntry(GetMaxFramesPerBuffer(),
                       std::round(kDropRateA * GetMaxFramesPerBuffer()),
                       std::round(kEfficientRateA * GetMaxFramesPerBuffer())));

  // Append entryB that will fill just 10% of the buffer. The new entry has
  // different rates of dropped and power efficient frames to help verify that
  // it is given proper weight as it mixes with existing data in the buffer.
  const int kNumFramesEntryB = std::round(.1 * GetMaxFramesPerBuffer());
  DecodeStatsEntry entryB(kNumFramesEntryB, std::round(0.25 * kNumFramesEntryB),
                          std::round(1 * kNumFramesEntryB));
  const double kDropRateB =
      static_cast<double>(entryB.frames_dropped) / entryB.frames_decoded;
  const double kEfficientRateB =
      static_cast<double>(entryB.frames_power_efficient) /
      entryB.frames_decoded;
  AppendStats(kStatsKeyVp9, entryB);
  // Verify that buffer is still full, but dropped and power efficient frame
  // rates are now higher according to entryB's impact (10%) on the full buffer.
  double mixed_drop_rate = .1 * kDropRateB + .9 * kDropRateA;
  double mixed_effiency_rate = .1 * kEfficientRateB + .9 * kEfficientRateA;
  VerifyReadStats(
      kStatsKeyVp9,
      DecodeStatsEntry(
          GetMaxFramesPerBuffer(),
          std::round(GetMaxFramesPerBuffer() * mixed_drop_rate),
          std::round(GetMaxFramesPerBuffer() * mixed_effiency_rate)));

  // After appending entryB again, verify aggregate ratios behave according to
  // the formula above (appending repeated entryB brings ratios closer to those
  // in entryB, further from entryA).
  AppendStats(kStatsKeyVp9, entryB);
  mixed_drop_rate = .1 * kDropRateB + .9 * mixed_drop_rate;
  mixed_effiency_rate = .1 * kEfficientRateB + .9 * mixed_effiency_rate;
  VerifyReadStats(
      kStatsKeyVp9,
      DecodeStatsEntry(
          GetMaxFramesPerBuffer(),
          std::round(GetMaxFramesPerBuffer() * mixed_drop_rate),
          std::round(GetMaxFramesPerBuffer() * mixed_effiency_rate)));

  // Appending entry*A* again, verify aggregate ratios behave according to
  // the formula above (ratio's move back in direction of those in entryA).
  // Since entryA fills half the buffer it gets a higher weight than entryB did
  // above.
  AppendStats(kStatsKeyVp9, entryA);
  mixed_drop_rate = .5 * kDropRateA + .5 * mixed_drop_rate;
  mixed_effiency_rate = .5 * kEfficientRateA + .5 * mixed_effiency_rate;
  VerifyReadStats(
      kStatsKeyVp9,
      DecodeStatsEntry(
          GetMaxFramesPerBuffer(),
          std::round(GetMaxFramesPerBuffer() * mixed_drop_rate),
          std::round(GetMaxFramesPerBuffer() * mixed_effiency_rate)));

  // Now append entryC with a frame count of 2x the buffer max. Verify entryC
  // gets 100% of the weight, erasing the mixed stats from earlier appends.
  const int kNumFramesEntryC = 2 * GetMaxFramesPerBuffer();
  DecodeStatsEntry entryC(kNumFramesEntryC, std::round(0.3 * kNumFramesEntryC),
                          std::round(0.25 * kNumFramesEntryC));
  const double kDropRateC =
      static_cast<double>(entryC.frames_dropped) / entryC.frames_decoded;
  const double kEfficientRateC =
      static_cast<double>(entryC.frames_power_efficient) /
      entryC.frames_decoded;
  AppendStats(kStatsKeyVp9, entryC);
  VerifyReadStats(
      kStatsKeyVp9,
      DecodeStatsEntry(GetMaxFramesPerBuffer(),
                       std::round(GetMaxFramesPerBuffer() * kDropRateC),
                       std::round(GetMaxFramesPerBuffer() * kEfficientRateC)));
}

// Overfilling an empty buffer triggers the codepath to compute weighted dropped
// and power efficient ratios under a circumstance where the existing counts are
// all zero. This test ensures that we don't do any dividing by zero with that
// empty data.
TEST_F(VideoDecodeStatsDBImplTest, OverfillEmptyBuffer) {
  InitializeDB();

  // Setup DB entry that overflows the buffer max (by 1) with 10% of frames
  // dropped and 50% of frames power efficient.
  const int kNumFramesOverfill = GetMaxFramesPerBuffer() + 1;
  DecodeStatsEntry entryA(kNumFramesOverfill,
                          std::round(0.1 * kNumFramesOverfill),
                          std::round(0.5 * kNumFramesOverfill));

  // Append entry to completely fill the buffer and verify read.
  AppendStats(kStatsKeyVp9, entryA);
  // Read-back stats should have same ratios, but scaled such that
  // frames_decoded = GetMaxFramesPerBuffer().
  DecodeStatsEntry readBackEntryA(GetMaxFramesPerBuffer(),
                                  std::round(0.1 * GetMaxFramesPerBuffer()),
                                  std::round(0.5 * GetMaxFramesPerBuffer()));
  VerifyReadStats(kStatsKeyVp9, readBackEntryA);

  // Append another entry that again overfills with different dropped and power
  // efficient ratios. Verify that read-back only reflects latest entry.
  DecodeStatsEntry entryB(kNumFramesOverfill,
                          std::round(0.2 * kNumFramesOverfill),
                          std::round(0.6 * kNumFramesOverfill));
  AppendStats(kStatsKeyVp9, entryB);
  // Read-back stats should have same ratios, but scaled such that
  // frames_decoded = GetMaxFramesPerBuffer().
  DecodeStatsEntry readBackEntryB(GetMaxFramesPerBuffer(),
                                  std::round(0.2 * GetMaxFramesPerBuffer()),
                                  std::round(0.6 * GetMaxFramesPerBuffer()));
  VerifyReadStats(kStatsKeyVp9, readBackEntryB);
}

TEST_F(VideoDecodeStatsDBImplTest, NoWriteDateReadAndExpire) {
  InitializeDB();

  // Seed the fake proto DB with an old-style entry lacking a write date. This
  // will cause the DB to use kDefaultWriteTime.
  DecodeStatsProto proto_lacking_date;
  proto_lacking_date.set_frames_decoded(100);
  proto_lacking_date.set_frames_dropped(10);
  proto_lacking_date.set_frames_power_efficient(1);
  fake_db_map_->emplace(kStatsKeyVp9.Serialize(), proto_lacking_date);

  // Set "now" to be *before* the default write date. This will be the common
  // case when the proto update (adding last_write_date) first ships (i.e. we
  // don't want to immediately expire all the existing data).
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(kDefaultWriteTime - base::TimeDelta::FromDays(10));
  // Verify the stats are readable (not expired).
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(100, 10, 1));

  // Set "now" to be in the middle of the grace period. Verify stats are still
  // readable (not expired).
  clock.SetNow(kDefaultWriteTime +
               base::TimeDelta::FromDays(GetMaxDaysToKeepStats() / 2));
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(100, 10, 1));

  // Set the clock 1 day beyond the expiry date. Verify stats are no longer
  // readable due to expiration.
  clock.SetNow(kDefaultWriteTime +
               base::TimeDelta::FromDays(GetMaxDaysToKeepStats() + 1));
  VerifyEmptyStats(kStatsKeyVp9);

  // Write some stats to the entry. Verify we get back exactly what's written
  // without summing with the expired stats.
  AppendStats(kStatsKeyVp9, DecodeStatsEntry(50, 5, 0));
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(50, 5, 0));
}

TEST_F(VideoDecodeStatsDBImplTest, NoWriteDateAppendReadAndExpire) {
  InitializeDB();

  // Seed the fake proto DB with an old-style entry lacking a write date. This
  // will cause the DB to use kDefaultWriteTime.
  DecodeStatsProto proto_lacking_date;
  proto_lacking_date.set_frames_decoded(100);
  proto_lacking_date.set_frames_dropped(10);
  proto_lacking_date.set_frames_power_efficient(1);
  fake_db_map_->emplace(kStatsKeyVp9.Serialize(), proto_lacking_date);

  // Set "now" to be *before* the default write date. This will be the common
  // case when the proto update (adding last_write_date) first ships (i.e. we
  // don't want to immediately expire all the existing data).
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(kDefaultWriteTime - base::TimeDelta::FromDays(10));
  // Verify the stats are readable (not expired).
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(100, 10, 1));

  // Append some stats and verify the aggregate math is correct. This will
  // update the last_write_date to the current clock time.
  AppendStats(kStatsKeyVp9, DecodeStatsEntry(200, 20, 2));
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(300, 30, 3));

  // Set "now" to be in the middle of the grace period. Verify stats are still
  // readable (not expired).
  clock.SetNow(kDefaultWriteTime +
               base::TimeDelta::FromDays(GetMaxDaysToKeepStats() / 2));
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(300, 30, 3));

  // Set the clock 1 day beyond the expiry date. Verify stats are no longer
  // readable due to expiration.
  clock.SetNow(kDefaultWriteTime +
               base::TimeDelta::FromDays(GetMaxDaysToKeepStats() + 1));
  VerifyEmptyStats(kStatsKeyVp9);
}

TEST_F(VideoDecodeStatsDBImplTest, AppendAndExpire) {
  InitializeDB();

  // Inject a test clock and initialize with the current time.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Append and verify read-back.
  AppendStats(kStatsKeyVp9, DecodeStatsEntry(200, 20, 2));
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(200, 20, 2));

  // Advance time half way through grace period. Verify stats not expired.
  clock.Advance(base::TimeDelta::FromDays(GetMaxDaysToKeepStats() / 2));
  VerifyReadStats(kStatsKeyVp9, DecodeStatsEntry(200, 20, 2));

  // Advance time 1 day beyond grace period, verify stats are expired.
  clock.Advance(base::TimeDelta::FromDays((GetMaxDaysToKeepStats() / 2) + 1));
  VerifyEmptyStats(kStatsKeyVp9);

  // Advance the clock 100 days. Verify stats still expired.
  clock.Advance(base::TimeDelta::FromDays(100));
  VerifyEmptyStats(kStatsKeyVp9);
}

TEST_F(VideoDecodeStatsDBImplTest, EnableUnweightedEntries) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<base::FieldTrialList> field_trial_list;

  // Default is false.
  EXPECT_FALSE(GetEnableUnweightedEntries());

  // Override field trial.
  base::FieldTrialParams params;
  params[VideoDecodeStatsDBImpl::kEnableUnweightedEntriesParamName] = "true";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaCapabilitiesWithParameters, params);

  base::FieldTrialParams actual_params;
  EXPECT_TRUE(base::GetFieldTrialParamsByFeature(
      media::kMediaCapabilitiesWithParameters, &actual_params));
  EXPECT_EQ(params, actual_params);

  // Confirm field trial overridden.
  EXPECT_TRUE(GetMaxDaysToKeepStats());

  InitializeDB();

  // Append 200 frames with 10% dropped, 1% efficient.
  AppendStats(kStatsKeyVp9, DecodeStatsEntry(200, 0.10 * 200, 0.01 * 200));
  // Use real doubles to keep track of these things to make sure the precision
  // math for repeating decimals works out with whats done internally.
  int num_appends = 1;
  double unweighted_smoothness_avg = 0.10;
  double unweighted_efficiency_avg = 0.01;

  // NOTE, the members of DecodeStatsEntry have a different meaning when using
  // unweighted DB entries. The denominator is 100,000 * the number of appends
  // and the numerator is whatever value achieves the correct unweighted ratio
  // for those appends. See detailed comment in
  // VideoDecodeStatsDBImpl::OnGotDecodeStats();
  const int kNumAppendScale = 100000;
  int expected_denominator = kNumAppendScale * num_appends;
  VerifyReadStats(
      kStatsKeyVp9,
      DecodeStatsEntry(expected_denominator,
                       unweighted_smoothness_avg * expected_denominator,
                       unweighted_efficiency_avg * expected_denominator));

  // Append 20K frames with 5% dropped and 10% efficient.
  AppendStats(kStatsKeyVp9,
              DecodeStatsEntry(20000, 0.05 * 20000, 0.10 * 20000));
  num_appends++;
  unweighted_smoothness_avg = (0.10 + 0.05) / num_appends;
  unweighted_efficiency_avg = (0.01 + 0.10) / num_appends;

  // While new record had 100x more frames than the previous append, the ratios
  // should be an unweighted average of the two records (7.5% dropped and
  // 5.5% efficient).
  expected_denominator = kNumAppendScale * num_appends;
  VerifyReadStats(
      kStatsKeyVp9,
      DecodeStatsEntry(expected_denominator,
                       unweighted_smoothness_avg * expected_denominator,
                       unweighted_efficiency_avg * expected_denominator));

  // Append 1M frames with 3.4567% dropped and 3.4567% efficient.
  AppendStats(kStatsKeyVp9, DecodeStatsEntry(1000000, 0.012345 * 1000000,
                                             0.034567 * 1000000));
  num_appends++;
  unweighted_smoothness_avg = (0.10 + 0.05 + 0.012345) / num_appends;
  unweighted_efficiency_avg = (0.01 + 0.10 + 0.034567) / num_appends;

  // Here, the ratios should still be averaged in the unweighted fashion, but
  // truncated after the 3rd decimal place of the percentage (e.g. 1.234%
  // or the 5th decimal place when represented as a fraction of 1 (0.01234)).
  expected_denominator = kNumAppendScale * num_appends;
  VerifyReadStats(
      kStatsKeyVp9,
      DecodeStatsEntry(expected_denominator,
                       unweighted_smoothness_avg * expected_denominator,
                       unweighted_efficiency_avg * expected_denominator));
}

TEST_F(VideoDecodeStatsDBImplTest, DiscardCorruptedDBData) {
  InitializeDB();

  // Inject a test clock and initialize with the current time.
  base::SimpleTestClock clock;
  SetDBClock(&clock);
  clock.SetNow(base::Time::Now());

  // Construct several distinct key values for storing/retrieving the corrupted
  // data. The details of the keys are not important.
  const auto keyA = VideoDescKey::MakeBucketedKey(
      VP9PROFILE_PROFILE0, gfx::Size(1024, 768), 60, "", false);
  const auto keyB = VideoDescKey::MakeBucketedKey(
      VP9PROFILE_PROFILE1, gfx::Size(1024, 768), 60, "", false);
  const auto keyC = VideoDescKey::MakeBucketedKey(
      VP9PROFILE_PROFILE2, gfx::Size(1024, 768), 60, "", false);
  const auto keyD = VideoDescKey::MakeBucketedKey(
      VP9PROFILE_PROFILE3, gfx::Size(1024, 768), 60, "", false);
  const auto keyE = VideoDescKey::MakeBucketedKey(
      H264PROFILE_BASELINE, gfx::Size(1024, 768), 60, "", false);
  const auto keyF = VideoDescKey::MakeBucketedKey(
      H264PROFILE_MAIN, gfx::Size(1024, 768), 60, "", false);
  const auto keyG = VideoDescKey::MakeBucketedKey(
      H264PROFILE_EXTENDED, gfx::Size(1024, 768), 60, "", false);

  // Start with a proto that represents a valid uncorrupted and unexpired entry.
  DecodeStatsProto protoA;
  protoA.set_frames_decoded(100);
  protoA.set_frames_dropped(15);
  protoA.set_frames_power_efficient(50);
  protoA.set_last_write_date(clock.Now().ToJsTime());
  protoA.set_unweighted_average_frames_dropped(15.0 / 100);
  protoA.set_unweighted_average_frames_efficient(50.0 / 100);
  protoA.set_num_unweighted_playbacks(1);

  // Append it and read it back without issue.
  AppendToProtoDB(keyA, &protoA);
  VerifyReadStats(keyA, DecodeStatsEntry(100, 15, 50));

  // Make the valid proto invalid with more dropped frames than decoded. Verify
  // you can't read it back (filtered for corruption).
  DecodeStatsProto protoB(protoA);
  protoB.set_frames_dropped(150);
  AppendToProtoDB(keyB, &protoB);
  VerifyEmptyStats(keyB);

  // Make an invalid proto with more power efficient frames than decoded. Verify
  // you can't read it back (filtered for corruption).
  DecodeStatsProto protoC(protoA);
  protoC.set_frames_power_efficient(150);
  AppendToProtoDB(keyC, &protoC);
  VerifyEmptyStats(keyC);

  // Make an invalid proto with an unweighted average dropped ratio > 1.
  DecodeStatsProto protoD(protoA);
  protoD.set_unweighted_average_frames_dropped(2.0);
  AppendToProtoDB(keyD, &protoD);
  VerifyEmptyStats(keyD);

  // Make an invalid proto with an unweighted average efficient ratio > 1.
  DecodeStatsProto protoE(protoA);
  protoE.set_unweighted_average_frames_efficient(2.0);
  AppendToProtoDB(keyE, &protoE);
  VerifyEmptyStats(keyE);

  // Make an invalid proto with a negative last write date.
  DecodeStatsProto protoF(protoA);
  protoF.set_last_write_date(-1.0);
  AppendToProtoDB(keyF, &protoF);
  VerifyEmptyStats(keyF);

  // Make an invalid  proto with a last write date in the future.
  DecodeStatsProto protoG(protoA);
  protoG.set_last_write_date(
      (clock.Now() + base::TimeDelta::FromDays(1)).ToJsTime());
  AppendToProtoDB(keyG, &protoG);
  VerifyEmptyStats(keyG);
}

}  // namespace media

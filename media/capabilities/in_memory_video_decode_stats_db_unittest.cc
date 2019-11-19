// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "media/capabilities/in_memory_video_decode_stats_db_impl.h"
#include "media/capabilities/video_decode_stats_db_impl.h"
#include "media/capabilities/video_decode_stats_db_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Pointee;
using testing::IsNull;

namespace media {

static VideoDecodeStatsDB::VideoDescKey kTestKey() {
  return VideoDecodeStatsDB::VideoDescKey::MakeBucketedKey(
      VP9PROFILE_PROFILE3, gfx::Size(1024, 768), 60, "com.widevine.alpha",
      false);
}

static VideoDecodeStatsDB::DecodeStatsEntry kEmtpyEntry() {
  return VideoDecodeStatsDB::DecodeStatsEntry(0, 0, 0);
}

class MockSeedDB : public VideoDecodeStatsDB {
 public:
  MockSeedDB() = default;
  ~MockSeedDB() override = default;

  MOCK_METHOD1(Initialize, void(InitializeCB init_cb));
  MOCK_METHOD3(AppendDecodeStats,
               void(const VideoDescKey& key,
                    const DecodeStatsEntry& entry,
                    AppendDecodeStatsCB append_done_cb));
  MOCK_METHOD2(GetDecodeStats,
               void(const VideoDescKey& key, GetDecodeStatsCB get_stats_cb));
  MOCK_METHOD1(ClearStats, void(base::OnceClosure destroy_done_cb));
};

class MockDBProvider : public VideoDecodeStatsDBProvider {
 public:
  MockDBProvider() = default;
  ~MockDBProvider() override = default;

  MOCK_METHOD1(GetVideoDecodeStatsDB, void(GetCB get_db_b));
};

template <bool WithSeedDB>
class InMemoryDBTestBase : public testing::Test {
 public:
  InMemoryDBTestBase()
      : seed_db_(WithSeedDB ? new MockSeedDB() : nullptr),
        db_provider_(WithSeedDB ? new MockDBProvider() : nullptr),
        in_memory_db_(new InMemoryVideoDecodeStatsDBImpl(db_provider_.get())) {
    // Setup MockDBProvider to provide the seed DB. No need to initialize the
    // DB here since it too is a Mock.
    if (db_provider_) {
      using GetCB = VideoDecodeStatsDBProvider::GetCB;
      ON_CALL(*db_provider_, GetVideoDecodeStatsDB(_))
          .WillByDefault([&](GetCB cb) { std::move(cb).Run(seed_db_.get()); });
    }

    // The InMemoryDB should NEVER modify the seed DB.
    if (seed_db_) {
      EXPECT_CALL(*seed_db_, AppendDecodeStats(_, _, _)).Times(0);
      EXPECT_CALL(*seed_db_, ClearStats(_)).Times(0);
    }
  }

  void InitializeEmptyDB() {
    if (seed_db_)
      EXPECT_CALL(*db_provider_, GetVideoDecodeStatsDB(_));

    EXPECT_CALL(*this, InitializeCB(true));

    in_memory_db_->Initialize(base::BindOnce(&InMemoryDBTestBase::InitializeCB,
                                             base::Unretained(this)));
    task_environment_.RunUntilIdle();
  }

  MOCK_METHOD1(InitializeCB, void(bool success));
  MOCK_METHOD1(AppendDecodeStatsCB, void(bool success));
  MOCK_METHOD2(
      GetDecodeStatsCB,
      void(bool success,
           std::unique_ptr<VideoDecodeStatsDB::DecodeStatsEntry> entry));
  MOCK_METHOD0(ClearStatsCB, void());

 protected:
  using VideoDescKey = media::VideoDecodeStatsDB::VideoDescKey;
  using DecodeStatsEntry = media::VideoDecodeStatsDB::DecodeStatsEntry;

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockSeedDB> seed_db_;
  std::unique_ptr<MockDBProvider> db_provider_;
  std::unique_ptr<InMemoryVideoDecodeStatsDBImpl> in_memory_db_;
};

// Specialization for tests that have/lack a seed DB. Some tests only make sense
// with seed DB, so we separate them.
class SeededInMemoryDBTest : public InMemoryDBTestBase<true> {};
class SeedlessInMemoryDBTest : public InMemoryDBTestBase<false> {};

TEST_F(SeedlessInMemoryDBTest, ReadExpectingEmpty) {
  InitializeEmptyDB();

  // Database is empty, seed DB is empty => expect empty stats entry.
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(kEmtpyEntry()))));

  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
}

TEST_F(SeededInMemoryDBTest, ReadExpectingEmpty) {
  InitializeEmptyDB();

  // Make seed DB return null (empty) for this request.
  EXPECT_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _))
      .WillOnce([](const auto& key, auto get_cb) {
        std::move(get_cb).Run(true, nullptr);
      });

  // Database is empty, seed DB is empty => expect empty stats entry.
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(kEmtpyEntry()))));

  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
}

TEST_F(SeededInMemoryDBTest, ReadExpectingSeedData) {
  InitializeEmptyDB();

  // Setup seed DB to return an entry for the test key.
  DecodeStatsEntry seed_entry(1000, 2, 10);

  EXPECT_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _))
      .WillOnce([&](const auto& key, auto get_cb) {
        std::move(get_cb).Run(true,
                              std::make_unique<DecodeStatsEntry>(seed_entry));
      });

  // Seed DB has a an entry for the test key. Expect it!
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(seed_entry))));

  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(this);

  // Verify a second GetDecodeStats() call with the same key does not trigger a
  // second call to the seed DB (we cache it).
  EXPECT_CALL(*seed_db_, GetDecodeStats(_, _)).Times(0);
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(seed_entry))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
}

TEST_F(SeededInMemoryDBTest, AppendReadAndClear) {
  const DecodeStatsEntry seed_entry(1000, 2, 10);
  const DecodeStatsEntry double_seed_entry(2000, 4, 20);
  const DecodeStatsEntry triple_seed_entry(3000, 6, 30);

  InitializeEmptyDB();

  // Setup seed DB to always return an entry for the test key.
  ON_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _))
      .WillByDefault([&](const auto& key, auto get_cb) {
        std::move(get_cb).Run(true,
                              std::make_unique<DecodeStatsEntry>(seed_entry));
      });

  // First append should trigger a request for the same key from the seed DB.
  // Simulate a successful read providing seed_entry for that key.
  EXPECT_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _));

  // Append the same seed entry, doubling the stats for this key.
  EXPECT_CALL(*this, AppendDecodeStatsCB(true));
  in_memory_db_->AppendDecodeStats(
      kTestKey(), seed_entry,
      base::BindOnce(&InMemoryDBTestBase::AppendDecodeStatsCB,
                     base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(this);

  // Seed DB should not be queried again for this key.
  EXPECT_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _)).Times(0);

  // Now verify that the stats were doubled by the append above.
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(double_seed_entry))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(this);

  // Append the same seed entry again to triple the stats. Additional appends
  // should not trigger queries the seed DB for this key.
  EXPECT_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _)).Times(0);
  in_memory_db_->AppendDecodeStats(
      kTestKey(), seed_entry,
      base::BindOnce(&InMemoryDBTestBase::AppendDecodeStatsCB,
                     base::Unretained(this)));

  // Verify we have 3x the stats.
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(triple_seed_entry))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  // Now destroy the in-memory stats...
  EXPECT_CALL(*this, ClearStatsCB());
  in_memory_db_->ClearStats(base::BindOnce(&InMemoryDBTestBase::ClearStatsCB,
                                           base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(this);

  // With in-memory stats now gone, GetDecodeStats(kTestKey()) should again
  // trigger a call to the seed DB and return the un-doubled seed stats.
  EXPECT_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _));
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(seed_entry))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
}

TEST_F(SeedlessInMemoryDBTest, AppendReadAndClear) {
  const DecodeStatsEntry entry(50, 1, 5);
  const DecodeStatsEntry double_entry(100, 2, 10);

  InitializeEmptyDB();

  // Expect successful append to the empty seedless DB.
  EXPECT_CALL(*this, AppendDecodeStatsCB(true));
  in_memory_db_->AppendDecodeStats(
      kTestKey(), entry,
      base::BindOnce(&InMemoryDBTestBase::AppendDecodeStatsCB,
                     base::Unretained(this)));

  // Verify stats can be read back.
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(entry))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(this);

  // Append same stats again to test summation.
  EXPECT_CALL(*this, AppendDecodeStatsCB(true));
  in_memory_db_->AppendDecodeStats(
      kTestKey(), entry,
      base::BindOnce(&InMemoryDBTestBase::AppendDecodeStatsCB,
                     base::Unretained(this)));

  // Verify doubled stats can be read back.
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(double_entry))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(this);

  // Now destroy the in-memory stats...
  EXPECT_CALL(*this, ClearStatsCB());
  in_memory_db_->ClearStats(base::BindOnce(&InMemoryDBTestBase::ClearStatsCB,
                                           base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(this);

  // Verify DB now empty for this key.
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(kEmtpyEntry()))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
}

TEST_F(SeededInMemoryDBTest, ProvidedNullSeedDB) {
  // DB provider may provide a null seed DB if it encounters some error.
  EXPECT_CALL(*db_provider_, GetVideoDecodeStatsDB(_))
      .WillOnce([](auto get_db_cb) { std::move(get_db_cb).Run(nullptr); });

  // Failing to obtain the seed DB is not a show stopper. The in-memory DB
  // should simply carry on in a seedless fashion.
  EXPECT_CALL(*this, InitializeCB(true));
  in_memory_db_->Initialize(base::BindOnce(&InMemoryDBTestBase::InitializeCB,
                                           base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(this);

  // Writes still succeed.
  EXPECT_CALL(*this, AppendDecodeStatsCB(true));
  const DecodeStatsEntry entry(50, 1, 5);
  in_memory_db_->AppendDecodeStats(
      kTestKey(), entry,
      base::BindOnce(&InMemoryDBTestBase::AppendDecodeStatsCB,
                     base::Unretained(this)));

  // Reads should still succeed.
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(entry))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
}

TEST_F(SeededInMemoryDBTest, SeedReadFailureOnGettingStats) {
  // Everything seems fine at initialization...
  InitializeEmptyDB();

  // But seed DB will repeatedly fail to provide stats.
  ON_CALL(*seed_db_, GetDecodeStats(_, _))
      .WillByDefault([](const auto& key, auto get_cb) {
        std::move(get_cb).Run(false, nullptr);
      });

  // Reading the in-memory will still try to read the seed DB, and the read
  // callback will simply report that the DB is empty for this key.
  EXPECT_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _));
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(kEmtpyEntry()))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
}

TEST_F(SeededInMemoryDBTest, SeedReadFailureOnAppendingingStats) {
  // Everything seems fine at initialization...
  InitializeEmptyDB();

  // But seed DB will repeatedly fail to provide stats.
  ON_CALL(*seed_db_, GetDecodeStats(_, _))
      .WillByDefault([](const auto& key, auto get_cb) {
        std::move(get_cb).Run(false, nullptr);
      });

  // Appending to the in-memory will still try to read the seed DB, and the
  // append will proceed successfully as if the seed DB were empty.
  EXPECT_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _));
  EXPECT_CALL(*this, AppendDecodeStatsCB(true));
  const DecodeStatsEntry entry(50, 1, 5);
  in_memory_db_->AppendDecodeStats(
      kTestKey(), entry,
      base::BindOnce(&InMemoryDBTestBase::AppendDecodeStatsCB,
                     base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(this);

  // Reading the appended data works without issue and does not trigger new
  // queries to the seed DB.
  EXPECT_CALL(*seed_db_, GetDecodeStats(Eq(kTestKey()), _)).Times(0);
  EXPECT_CALL(*this, GetDecodeStatsCB(true, Pointee(Eq(entry))));
  in_memory_db_->GetDecodeStats(
      kTestKey(), base::BindOnce(&InMemoryDBTestBase::GetDecodeStatsCB,
                                 base::Unretained(this)));

  task_environment_.RunUntilIdle();
}

TEST_F(SeededInMemoryDBTest, SeedDBTearDownRace) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Establish depends-on connection from InMemoryDB to SeedDB.
  InitializeEmptyDB();

  // Clearing the seed-db dependency should trigger a crash.
  EXPECT_CHECK_DEATH(seed_db_.reset());
}

}  // namespace media

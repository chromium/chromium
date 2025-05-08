// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/leveldb/cleanup_scheduler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "content/browser/indexed_db/instance/leveldb/backing_store.h"
#include "content/browser/indexed_db/instance/leveldb/indexed_db_leveldb_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace content::indexed_db::level_db {

namespace {
constexpr int64_t kDb1 = 1;
constexpr int64_t kOs1 = 3;
constexpr int64_t kIndex1 = 31;
constexpr int kRoundIterations = 11;
}  // namespace

class LevelDBCleanupSchedulerTest : public testing::Test,
                                    public LevelDBCleanupScheduler::Delegate {
 public:
  LevelDBCleanupSchedulerTest() = default;

  void SetUp() override {
    SetupRealDB();
    scheduler_ =
        std::make_unique<LevelDBCleanupScheduler>(in_memory_db_->db(), this);
  }

  void TearDown() override { scheduler_.reset(); }

  bool UpdateEarliestSweepTime() override { return true; }

  bool UpdateEarliestCompactionTime() override { return true; }

  Status GetCompleteMetadata(
      std::vector<std::unique_ptr<blink::IndexedDBDatabaseMetadata>>* out)
      override {
    // db1
    //   os1
    //     index1
    auto db = std::make_unique<BackingStore::DatabaseMetadata>(u"db1");
    auto& db1 = *db;
    db1.id = kDb1;
    db1.version = 1;
    db1.max_object_store_id = 29;
    db1.object_stores[kOs1] = blink::IndexedDBObjectStoreMetadata(
        u"os1", kOs1, blink::IndexedDBKeyPath(), false, 1000);
    auto& os2 = db1.object_stores[kOs1];
    os2.indexes[kIndex1] = blink::IndexedDBIndexMetadata(
        u"index1", kIndex1, blink::IndexedDBKeyPath(), true, false);

    for (int i = 0; i < kRoundIterations + 1; i++) {
      auto index_key = blink::IndexedDBKey(i, blink::mojom::IDBKeyType::Number);
      auto primary_key =
          blink::IndexedDBKey(i + 1, blink::mojom::IDBKeyType::Number);
      std::string value_str;
      EncodeVarInt(1, &value_str);
      EncodeIDBKey(primary_key, &value_str);
      in_memory_db_->Put(
          IndexDataKey::Encode(kDb1, kOs1, kIndex1, index_key, primary_key),
          &value_str);

      std::string exists_value;
      std::string encoded_primary_key;
      EncodeIDBKey(primary_key, &encoded_primary_key);
      bool tombstone = i % 2 != 0;
      EncodeVarInt(tombstone ? 2 : 1, &exists_value);
      in_memory_db_->Put(
          ExistsEntryKey::Encode(kDb1, kOs1, encoded_primary_key),
          &exists_value);
    }
    out->push_back(std::move(db));

    return Status::OK();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<LevelDBCleanupScheduler> scheduler_;
  base::HistogramTester tester_;
  std::unique_ptr<TransactionalLevelDBDatabase> in_memory_db_;

 private:
  leveldb_env::Options GetLevelDBOptions() {
    leveldb_env::Options options;
    options.comparator = indexed_db::GetDefaultLevelDBComparator();
    options.create_if_missing = true;
    options.write_buffer_size = 4 * 1024 * 1024;
    options.paranoid_checks = true;

    static base::NoDestructor<leveldb_env::ChromiumEnv> g_leveldb_env;
    options.env = g_leveldb_env.get();

    return options;
  }

  void SetupRealDB() {
    leveldb_env::Options options = GetLevelDBOptions();
    std::unique_ptr<leveldb::Env> in_memory_env =
        leveldb_chrome::NewMemEnv("in-memory-testing-db", options.env);
    options.env = in_memory_env.get();

    std::unique_ptr<leveldb::DB> db;
    leveldb::Status s = leveldb_env::OpenDB(options, std::string(), &db);
    ASSERT_TRUE(s.ok());
    scoped_refptr<LevelDBState> level_db_state =
        LevelDBState::CreateForInMemoryDB(std::move(in_memory_env),
                                          options.comparator, std::move(db),
                                          "in-memory-testing-db");
    in_memory_db_ = DefaultTransactionalLevelDBFactory().CreateLevelDBDatabase(
        std::move(level_db_state), nullptr, nullptr,
        TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase);
  }
};

TEST_F(LevelDBCleanupSchedulerTest, WithPostpone) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIdbInSessionDbCleanup);
  EXPECT_FALSE(scheduler_->GetRunningStateForTesting().has_value());

  // Schedule a run to occur after 4 seconds.
  scheduler_->OnTransactionStart();
  scheduler_->Initialize();
  EXPECT_TRUE(scheduler_->GetRunningStateForTesting().has_value());
  scheduler_->OnTransactionComplete();
  EXPECT_TRUE(scheduler_->GetRunningStateForTesting().has_value());
  EXPECT_EQ(scheduler_->GetRunningStateForTesting()->cleanup_phase,
            LevelDBCleanupScheduler::Phase::kRunScheduled);
  EXPECT_EQ(0, scheduler_->GetRunningStateForTesting()->postpone_count);

  // Begin the cleanup process.
  task_environment_.FastForwardBy(
      LevelDBCleanupScheduler::kDeferTimeAfterLastTransaction);

  // The tombstone sweeper completes and sets the phase to
  // `kDatabaseCompaction`.
  EXPECT_EQ(scheduler_->GetRunningStateForTesting()->cleanup_phase,
            LevelDBCleanupScheduler::Phase::kDatabaseCompaction);

  // Signal the start of a new transaction to postpone the cleanup.
  scheduler_->OnTransactionStart();
  // Pass the time in the task scheduler to confirm no operation was
  // performed.
  task_environment_.FastForwardBy(
      LevelDBCleanupScheduler::kDeferTimeAfterLastTransaction);
  // The cleanup will be currently paused. Confirm by checking the
  // phase and the postpone count.
  EXPECT_EQ(scheduler_->GetRunningStateForTesting()->cleanup_phase,
            LevelDBCleanupScheduler::Phase::kDatabaseCompaction);
  EXPECT_EQ(1, scheduler_->GetRunningStateForTesting()->postpone_count);
  scheduler_->OnTransactionComplete();
  EXPECT_EQ(scheduler_->GetRunningStateForTesting()->cleanup_phase,
            LevelDBCleanupScheduler::Phase::kDatabaseCompaction);
  EXPECT_EQ(1, scheduler_->GetRunningStateForTesting()->postpone_count);

  // Complete the database compaction phase.
  task_environment_.FastForwardBy(
      LevelDBCleanupScheduler::kDeferTimeAfterLastTransaction);

  EXPECT_EQ(scheduler_->GetRunningStateForTesting()->cleanup_phase,
            LevelDBCleanupScheduler::Phase::kLoggingAndCleanup);

  task_environment_.FastForwardBy(
      LevelDBCleanupScheduler::kDeferTimeOnNoTransactions);
  EXPECT_FALSE(scheduler_->GetRunningStateForTesting().has_value());

  tester_.ExpectTotalCount(
      "IndexedDB.LevelDBCleanupScheduler.TombstoneSweeperDuration", 1);
  tester_.ExpectTotalCount(
      "IndexedDB.LevelDBCleanupScheduler.DBCompactionDuration", 1);
  tester_.ExpectUniqueSample(
      "IndexedDB.LevelDBCleanupScheduler.CleanerPostponedCount", 1, 1);
  tester_.ExpectTotalCount("IndexedDB.LevelDbTombstoneSweeper.TombstonesFound",
                           1);
  tester_.ExpectTotalCount(
      "IndexedDB.LevelDBCleanupScheduler.PrematureTerminationPhase", 0);
}

TEST_F(LevelDBCleanupSchedulerTest, SecondRunTooQuick) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIdbInSessionDbCleanup);
  scheduler_->OnTransactionStart();
  scheduler_->Initialize();
  scheduler_->OnTransactionComplete();

  task_environment_.FastForwardBy(
      LevelDBCleanupScheduler::kDeferTimeAfterLastTransaction);

  EXPECT_EQ(scheduler_->GetRunningStateForTesting()->cleanup_phase,
            LevelDBCleanupScheduler::Phase::kDatabaseCompaction);

  task_environment_.FastForwardBy(
      LevelDBCleanupScheduler::kDeferTimeOnNoTransactions);

  EXPECT_EQ(scheduler_->GetRunningStateForTesting()->cleanup_phase,
            LevelDBCleanupScheduler::Phase::kLoggingAndCleanup);

  task_environment_.FastForwardBy(
      LevelDBCleanupScheduler::kDeferTimeOnNoTransactions);

  EXPECT_FALSE(scheduler_->GetRunningStateForTesting().has_value());

  // Try initializing the scheduler again.
  // This should fail as `kMinimumTimeBetweenRuns` has not passed since
  // the last run.
  scheduler_->Initialize();

  EXPECT_FALSE(scheduler_->GetRunningStateForTesting().has_value());
}

TEST_F(LevelDBCleanupSchedulerTest, PrematureTermination) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIdbInSessionDbCleanup);
  EXPECT_FALSE(scheduler_->GetRunningStateForTesting().has_value());
  scheduler_->OnTransactionStart();
  scheduler_->Initialize();
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(scheduler_->GetRunningStateForTesting().has_value());
  EXPECT_EQ(scheduler_->GetRunningStateForTesting()->cleanup_phase,
            LevelDBCleanupScheduler::Phase::kRunScheduled);

  // End the scheduler
  scheduler_.reset();

  tester_.ExpectUniqueSample(
      "IndexedDB.LevelDBCleanupScheduler.PrematureTerminationPhase",
      LevelDBCleanupScheduler::Phase::kRunScheduled, 1);
}

TEST_F(LevelDBCleanupSchedulerTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kIdbInSessionDbCleanup);
  EXPECT_FALSE(scheduler_->GetRunningStateForTesting().has_value());
  scheduler_->OnTransactionStart();
  scheduler_->Initialize();
  scheduler_->OnTransactionComplete();
  EXPECT_FALSE(scheduler_->GetRunningStateForTesting().has_value());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(scheduler_->GetRunningStateForTesting().has_value());
  tester_.ExpectTotalCount(
      "IndexedDB.LevelDBCleanupScheduler.TombstoneSweeperDuration", 0);
  tester_.ExpectTotalCount(
      "IndexedDB.LevelDBCleanupScheduler.DBCompactionDuration", 0);
  tester_.ExpectTotalCount(
      "IndexedDB.LevelDBCleanupScheduler.CleanerPostponedCount", 0);
  tester_.ExpectTotalCount("IndexedDB.LevelDbTombstoneSweeper.TombstonesFound",
                           0);
}
}  // namespace content::indexed_db::level_db

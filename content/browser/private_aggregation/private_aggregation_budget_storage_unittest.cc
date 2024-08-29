// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budget_storage.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/clamped_math.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "content/browser/private_aggregation/proto/private_aggregation_budgets.pb.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr char kExampleSerializedOrigin[] = "http://origin.example";

}  // namespace

class PrivateAggregationBudgetStorageTest : public testing::Test {
 public:
  PrivateAggregationBudgetStorageTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    db_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  void TearDown() override {
    // Ensure destruction tasks are run before the test ends. Otherwise,
    // erroneous memory leaks may be detected.
    CloseDatabase();
    task_environment_.RunUntilIdle();
  }

  base::OnceClosure OpenDatabase(bool run_in_memory,
                                 base::OnceClosure on_done_initializing) {
    base::OnceCallback<void(std::unique_ptr<PrivateAggregationBudgetStorage>)>
        create_cb = base::BindOnce(
            &PrivateAggregationBudgetStorageTest::OnStorageInitialized,
            base::Unretained(this), std::move(on_done_initializing));

    return PrivateAggregationBudgetStorage::CreateAsync(
        db_task_runner_, run_in_memory, storage_directory(),
        std::move(create_cb));
  }

  // Waits for the database to be initialized.
  void OpenDatabaseAndWait(bool exclusively_run_in_memory = false) {
    base::RunLoop run_loop;
    OpenDatabase(exclusively_run_in_memory,
                 /*on_done_initializing=*/run_loop.QuitClosure());
    run_loop.Run();
  }

  void CloseDatabase() { storage_.reset(); }

  void EnsureDbFlushes() {
    // Ensures any pending writes are run.
    task_environment_.FastForwardBy(
        PrivateAggregationBudgetStorage::kFlushDelay);
    task_environment_.RunUntilIdle();
  }

  void VerifyInitializationCountHistogram(size_t expected_count) {
    histogram_tester_.ExpectTotalCount(
        "PrivacySandbox.PrivateAggregation.BudgetStorage."
        "BeginInitializationCount",
        expected_count);
  }

  // Helper for the unique sample case.
  void VerifyInitHistograms(
      PrivateAggregationBudgetStorage::InitStatus init_status,
      bool shutdown_before_finishing_initialization,
      int expected_bucket_count = 1) {
    histogram_tester_.ExpectUniqueSample(
        "PrivacySandbox.PrivateAggregation.BudgetStorage.InitStatus",
        init_status, expected_bucket_count);
    histogram_tester_.ExpectUniqueSample(
        "PrivacySandbox.PrivateAggregation.BudgetStorage."
        "ShutdownBeforeFinishingInitialization",
        shutdown_before_finishing_initialization, expected_bucket_count);
    histogram_tester_.ExpectUniqueTimeSample(
        "PrivacySandbox.PrivateAggregation.BudgetStorage.InitTime",
        base::TimeDelta(), expected_bucket_count);

    VerifyInitializationCountHistogram(
        /*expected_count=*/expected_bucket_count);
  }

  void VerifyDbSizeHistogram(int total_num_samples, int num_zero_samples) {
    CHECK_LE(0, num_zero_samples);
    CHECK_LE(num_zero_samples, total_num_samples);

    constexpr std::string_view kFileSizeHistogram =
        "PrivacySandbox.PrivateAggregation.BudgetStorage.DbSize";
    histogram_tester_.ExpectTotalCount(kFileSizeHistogram, total_num_samples);
    histogram_tester_.ExpectBucketCount(kFileSizeHistogram, 0,
                                        num_zero_samples);
  }

  base::FilePath storage_directory() const { return temp_directory_.GetPath(); }

  base::FilePath db_path() const {
    return storage_directory().Append(FILE_PATH_LITERAL("PrivateAggregation"));
  }

  PrivateAggregationBudgetStorage* storage() { return storage_.get(); }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  void OnStorageInitialized(
      base::OnceClosure on_done_initializing,
      std::unique_ptr<PrivateAggregationBudgetStorage> storage) {
    storage_ = std::move(storage);
    std::move(on_done_initializing).Run();
  }

  base::ScopedTempDir temp_directory_;
  std::unique_ptr<PrivateAggregationBudgetStorage> storage_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PrivateAggregationBudgetStorageTest, DatabaseInitialization) {
  EXPECT_FALSE(base::PathExists(db_path()));

  base::RunLoop run_loop;
  VerifyInitializationCountHistogram(/*expected_count=*/0);
  OpenDatabase(/*run_in_memory=*/false,
               /*on_done_initializing=*/base::BindLambdaForTesting(
                   [&run_loop]() { run_loop.Quit(); }));
  // The count should be increased when the initialization begins.
  VerifyInitializationCountHistogram(/*expected_count=*/1);
  EXPECT_FALSE(storage());
  run_loop.Run();
  EXPECT_TRUE(storage());

  // Even an unused instance should create the directory.
  EXPECT_TRUE(base::PathExists(db_path()));

  VerifyInitHistograms(PrivateAggregationBudgetStorage::InitStatus::kSuccess,
                       /*shutdown_before_finishing_initialization=*/false);
  VerifyDbSizeHistogram(/*total_num_samples=*/1, /*num_zero_samples=*/1);
}

TEST_F(PrivateAggregationBudgetStorageTest,
       StorageDirectoryAlreadyCreated_StillInitializes) {
  base::CreateDirectory(storage_directory());

  base::RunLoop run_loop;
  OpenDatabase(/*run_in_memory=*/false,
               /*on_done_initializing=*/run_loop.QuitClosure());
  EXPECT_FALSE(storage());

  run_loop.Run();
  EXPECT_TRUE(storage());

  VerifyInitHistograms(PrivateAggregationBudgetStorage::InitStatus::kSuccess,
                       /*shutdown_before_finishing_initialization=*/false);
  VerifyDbSizeHistogram(/*total_num_samples=*/1, /*num_zero_samples=*/1);
}

TEST_F(PrivateAggregationBudgetStorageTest, DbPathCorrupt_FailsToInitialize) {
  // Create a directory where the database will try to write a file.
  base::CreateDirectory(db_path());

  base::RunLoop run_loop;
  OpenDatabase(/*run_in_memory=*/false,
               /*on_done_initializing=*/run_loop.QuitClosure());
  EXPECT_FALSE(storage());

  run_loop.Run();
  EXPECT_FALSE(storage());

  VerifyInitHistograms(
      PrivateAggregationBudgetStorage::InitStatus::kFailedToOpenDbFile,
      /*shutdown_before_finishing_initialization=*/false);
  VerifyDbSizeHistogram(/*total_num_samples=*/0, /*num_zero_samples=*/0);
}

TEST_F(PrivateAggregationBudgetStorageTest, InMemory_StillInitializes) {
  base::RunLoop run_loop;
  OpenDatabase(/*run_in_memory=*/true,
               /*on_done_initializing=*/run_loop.QuitClosure());
  EXPECT_FALSE(storage());

  run_loop.Run();
  EXPECT_TRUE(storage());

  VerifyInitHistograms(PrivateAggregationBudgetStorage::InitStatus::kSuccess,
                       /*shutdown_before_finishing_initialization=*/false);
  VerifyDbSizeHistogram(/*total_num_samples=*/0, /*num_zero_samples=*/0);
}

TEST_F(PrivateAggregationBudgetStorageTest, DatabaseReopened_DataPersisted) {
  OpenDatabaseAndWait();
  ASSERT_TRUE(storage());

  // The database should start empty.
  EXPECT_FALSE(storage()->budgets_data()->TryGetData(kExampleSerializedOrigin,
                                                     /*data=*/nullptr));

  storage()->budgets_data()->UpdateData(kExampleSerializedOrigin,
                                        proto::PrivateAggregationBudgets());

  EnsureDbFlushes();
  CloseDatabase();

  OpenDatabaseAndWait();

  EXPECT_TRUE(storage()->budgets_data()->TryGetData(kExampleSerializedOrigin,
                                                    /*data=*/nullptr));

  VerifyInitHistograms(PrivateAggregationBudgetStorage::InitStatus::kSuccess,
                       /*shutdown_before_finishing_initialization=*/false,
                       /*expected_bucket_count=*/2);
  VerifyDbSizeHistogram(/*total_num_samples=*/2, /*num_zero_samples=*/1);
}

TEST_F(PrivateAggregationBudgetStorageTest,
       DatabaseClosedBeforeFlush_DataPersisted) {
  OpenDatabaseAndWait();
  ASSERT_TRUE(storage());

  // The database should start empty.
  EXPECT_FALSE(storage()->budgets_data()->TryGetData(kExampleSerializedOrigin,
                                                     /*data=*/nullptr));

  storage()->budgets_data()->UpdateData(kExampleSerializedOrigin,
                                        proto::PrivateAggregationBudgets());
  // Not waiting for DB flush
  CloseDatabase();

  OpenDatabaseAndWait();

  EXPECT_TRUE(storage()->budgets_data()->TryGetData(kExampleSerializedOrigin,
                                                    /*data=*/nullptr));
}

TEST_F(PrivateAggregationBudgetStorageTest,
       InMemoryDatabaseReopened_DataNotPersisted) {
  OpenDatabaseAndWait(/*exclusively_run_in_memory=*/true);
  ASSERT_TRUE(storage());

  // The database should start empty.
  EXPECT_FALSE(storage()->budgets_data()->TryGetData(kExampleSerializedOrigin,
                                                     /*data=*/nullptr));

  storage()->budgets_data()->UpdateData(kExampleSerializedOrigin,
                                        proto::PrivateAggregationBudgets());

  EnsureDbFlushes();
  CloseDatabase();

  OpenDatabaseAndWait(/*exclusively_run_in_memory=*/true);

  EXPECT_FALSE(storage()->budgets_data()->TryGetData(kExampleSerializedOrigin,
                                                     /*data=*/nullptr));

  VerifyInitHistograms(PrivateAggregationBudgetStorage::InitStatus::kSuccess,
                       /*shutdown_before_finishing_initialization=*/false,
                       /*expected_bucket_count=*/2);
  VerifyDbSizeHistogram(/*total_num_samples=*/0, /*num_zero_samples=*/0);
}

TEST_F(PrivateAggregationBudgetStorageTest,
       SchemaVersionChanged_DatabaseRazed) {
  OpenDatabaseAndWait();
  ASSERT_TRUE(storage());

  // The database should start empty.
  EXPECT_FALSE(storage()->budgets_data()->TryGetData(kExampleSerializedOrigin,
                                                     /*data=*/nullptr));

  storage()->budgets_data()->UpdateData(kExampleSerializedOrigin,
                                        proto::PrivateAggregationBudgets());

  EnsureDbFlushes();

  CloseDatabase();

  // Wait for database to finish closing.
  task_environment().RunUntilIdle();

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    sql::MetaTable meta;
    // The values here are irrelevant, as the meta table already exists.
    ASSERT_TRUE(meta.Init(&raw_db, /*version=*/1, /*compatible_version=*/1));

    ASSERT_TRUE(meta.SetVersionNumber(meta.GetVersionNumber() + 1));
    ASSERT_TRUE(meta.SetCompatibleVersionNumber(meta.GetVersionNumber() + 1));
  }

  OpenDatabaseAndWait();

  EXPECT_FALSE(storage()->budgets_data()->TryGetData(kExampleSerializedOrigin,
                                                     /*data=*/nullptr));

  VerifyInitHistograms(PrivateAggregationBudgetStorage::InitStatus::kSuccess,
                       /*shutdown_before_finishing_initialization=*/false,
                       /*expected_bucket_count=*/2);
  VerifyDbSizeHistogram(/*total_num_samples=*/2, /*num_zero_samples=*/1);
}

TEST_F(PrivateAggregationBudgetStorageTest,
       StorageDestroyedImmediatelyAfterInitialization_DoesNotCrash) {
  base::RunLoop run_loop;
  OpenDatabase(
      /*run_in_memory=*/false,
      /*on_done_initializing=*/base::BindLambdaForTesting([this, &run_loop]() {
        CloseDatabase();
        run_loop.Quit();
      }));
  run_loop.Run();

  VerifyInitHistograms(PrivateAggregationBudgetStorage::InitStatus::kSuccess,
                       /*shutdown_before_finishing_initialization=*/false);
  VerifyDbSizeHistogram(/*total_num_samples=*/1, /*num_zero_samples=*/1);
}

TEST_F(PrivateAggregationBudgetStorageTest,
       StorageShutdownImmediatelyAfterCreation_DoesNotCrash) {
  base::RunLoop run_loop;
  base::OnceClosure shutdown = OpenDatabase(
      /*run_in_memory=*/false,
      /*on_done_initializing=*/base::BindLambdaForTesting([this, &run_loop]() {
        CloseDatabase();
        run_loop.Quit();
      }));
  std::move(shutdown).Run();
  run_loop.Run();

  VerifyInitHistograms(PrivateAggregationBudgetStorage::InitStatus::kSuccess,
                       /*shutdown_before_finishing_initialization=*/true);
  VerifyDbSizeHistogram(/*total_num_samples=*/1, /*num_zero_samples=*/1);
}

TEST_F(PrivateAggregationBudgetStorageTest,
       StorageShutdownImmediatelyAfterInitialization_DoesNotCrash) {
  base::RunLoop run_loop;
  base::OnceClosure shutdown = OpenDatabase(
      /*run_in_memory=*/false,
      /*on_done_initializing=*/base::BindLambdaForTesting([&, this]() {
        std::move(shutdown).Run();
        CloseDatabase();
        run_loop.Quit();
      }));
  run_loop.Run();

  VerifyInitHistograms(PrivateAggregationBudgetStorage::InitStatus::kSuccess,
                       /*shutdown_before_finishing_initialization=*/false);
  VerifyDbSizeHistogram(/*total_num_samples=*/1, /*num_zero_samples=*/1);
}

}  // namespace content

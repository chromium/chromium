// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budgeter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_budget_storage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class PrivateAggregationBudgeterUnderTest : public PrivateAggregationBudgeter {
 public:
  PrivateAggregationBudgeterUnderTest(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      bool exclusively_run_in_memory,
      const base::FilePath& path_to_db_dir,
      base::OnceClosure on_storage_done_initializing)
      : PrivateAggregationBudgeter(db_task_runner,
                                   exclusively_run_in_memory,
                                   path_to_db_dir),
        on_storage_done_initializing_(std::move(on_storage_done_initializing)) {
  }

  ~PrivateAggregationBudgeterUnderTest() override = default;

  PrivateAggregationBudgeter::StorageStatus GetStorageStatus() const {
    return storage_status_;
  }

 private:
  void OnStorageDoneInitializing(
      std::unique_ptr<PrivateAggregationBudgetStorage> storage) override {
    PrivateAggregationBudgeter::OnStorageDoneInitializing(std::move(storage));
    if (on_storage_done_initializing_)
      std::move(on_storage_done_initializing_).Run();
  }

  base::OnceClosure on_storage_done_initializing_;
};

// TODO(alexmt): Consider moving logic shared with
// PrivateAggregationBudgetStorageTest to a joint test harness.
class PrivateAggregationBudgeterTest : public testing::Test {
 public:
  PrivateAggregationBudgeterTest()
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
    DestroyBudgeter();
    task_environment_.RunUntilIdle();
  }

  void CreateBudgeter(bool exclusively_run_in_memory,
                      base::OnceClosure on_done_initializing) {
    budgeter_ = std::make_unique<PrivateAggregationBudgeterUnderTest>(
        db_task_runner_, exclusively_run_in_memory, storage_directory(),
        std::move(on_done_initializing));
  }

  void CreateBudgeterAndWait(bool exclusively_run_in_memory = false) {
    base::RunLoop run_loop;
    CreateBudgeter(exclusively_run_in_memory,
                   /*on_done_initializing=*/run_loop.QuitClosure());
    run_loop.Run();
  }

  void DestroyBudgeter() { budgeter_.reset(); }

  void EnsureDbFlushes() {
    // Ensures any pending writes are run.
    task_environment_.FastForwardBy(
        PrivateAggregationBudgetStorage::kFlushDelay);
    task_environment_.RunUntilIdle();
  }

  PrivateAggregationBudgeter* budgeter() { return budgeter_.get(); }

  base::FilePath db_path() const {
    return storage_directory().Append(FILE_PATH_LITERAL("PrivateAggregation"));
  }

  PrivateAggregationBudgeter::StorageStatus GetStorageStatus() const {
    return budgeter_->GetStorageStatus();
  }

 private:
  base::FilePath storage_directory() const { return temp_directory_.GetPath(); }

  base::ScopedTempDir temp_directory_;
  std::unique_ptr<PrivateAggregationBudgeterUnderTest> budgeter_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PrivateAggregationBudgeterTest, BudgeterCreated_DatabaseInitialized) {
  bool is_done_initializing = false;
  base::RunLoop run_loop;
  CreateBudgeter(/*exclusively_run_in_memory=*/false,
                 /*on_done_initializing=*/base::BindLambdaForTesting([&]() {
                   is_done_initializing = true;
                   run_loop.Quit();
                 }));
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializing);
  EXPECT_FALSE(is_done_initializing);

  run_loop.Run();
  EXPECT_TRUE(is_done_initializing);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kOpen);
}

TEST_F(PrivateAggregationBudgeterTest,
       DatabaseInitializationFails_StatusIsClosed) {
  // The database initialization will fail to open if its directory already
  // exists.
  base::CreateDirectory(db_path());

  base::RunLoop run_loop;
  CreateBudgeter(/*exclusively_run_in_memory=*/false,
                 /*on_done_initializing=*/run_loop.QuitClosure());
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializing);

  run_loop.Run();
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializationFailed);
}

TEST_F(PrivateAggregationBudgeterTest, InMemory_StillInitializes) {
  base::RunLoop run_loop;
  CreateBudgeter(/*exclusively_run_in_memory=*/true,
                 /*on_done_initializing=*/run_loop.QuitClosure());
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializing);

  run_loop.Run();
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kOpen);
}

TEST_F(PrivateAggregationBudgeterTest, DatabaseReopened_DataPersisted) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);
  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_TRUE(succeeded);
        ++num_queries_processed;
      }));

  // Ensure database has a chance to persist storage.
  EnsureDbFlushes();

  DestroyBudgeter();
  CreateBudgeterAndWait();

  base::RunLoop run_loop;
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&](bool succeeded) {
        EXPECT_FALSE(succeeded);
        ++num_queries_processed;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 2);
}

TEST_F(PrivateAggregationBudgeterTest,
       InMemoryDatabaseReopened_DataNotPersisted) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait(/*exclusively_run_in_memory=*/true);

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);
  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_TRUE(succeeded);
        ++num_queries_processed;
      }));

  // Ensure database has a chance to persist storage.
  EnsureDbFlushes();

  DestroyBudgeter();
  CreateBudgeterAndWait(/*exclusively_run_in_memory=*/true);

  base::RunLoop run_loop;
  budgeter()->ConsumeBudget(/*budget=*/1, example_key,
                            base::BindLambdaForTesting([&](bool succeeded) {
                              EXPECT_TRUE(succeeded);
                              num_queries_processed++;
                              run_loop.Quit();
                            }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 2);
}

TEST_F(PrivateAggregationBudgeterTest, ConsumeBudgetSameKey) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);

  // Budget can be increased to below max
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_TRUE(succeeded);
        ++num_queries_processed;
      }));

  // Budget can be increased to max
  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope - 1),
      example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_TRUE(succeeded);
        ++num_queries_processed;
      }));

  base::RunLoop run_loop;

  // Budget cannot be increased above max
  budgeter()->ConsumeBudget(/*budget=*/1, example_key,
                            base::BindLambdaForTesting([&](bool succeeded) {
                              EXPECT_FALSE(succeeded);
                              ++num_queries_processed;
                              run_loop.Quit();
                            }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 3);
}

TEST_F(PrivateAggregationBudgeterTest, ConsumeBudgetDifferentTimeWindows) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  base::Time reference_time = base::Time::FromJavaTime(1652984901234);

  // Create a day's worth of budget keys for a particular origin-API pair
  // (with varying time windows) plus one extra.
  std::vector<PrivateAggregationBudgetKey> example_keys;
  for (int i = 0; i < 25; ++i) {
    example_keys.push_back(PrivateAggregationBudgetKey::CreateForTesting(
        url::Origin::Create(GURL("https://a.example")),
        reference_time + i * base::Hours(1),
        PrivateAggregationBudgetKey::Api::kFledge));
  }

  // Consuming this budget 24 times in a day would not exceed the daily budget,
  // but 25 times would.
  int budget_to_use_per_hour =
      PrivateAggregationBudgeter::kMaxBudgetPerScope / 24;
  EXPECT_GT(budget_to_use_per_hour * 25,
            PrivateAggregationBudgeter::kMaxBudgetPerScope);

  // Use budget in the first 24 keys.
  for (int i = 0; i < 24; ++i) {
    budgeter()->ConsumeBudget(
        budget_to_use_per_hour, example_keys[i],
        base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
          EXPECT_TRUE(succeeded);
          ++num_queries_processed;
        }));
  }

  // The last 24 keys are used for calculating remaining budget, so we can't
  // use more during the 24th time window.
  budgeter()->ConsumeBudget(
      budget_to_use_per_hour, example_keys[23],
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_FALSE(succeeded);
        ++num_queries_processed;
      }));

  base::RunLoop run_loop;

  // But the last key can use budget as the first key is no longer in the
  // relevant set of 24 time windows.
  budgeter()->ConsumeBudget(budget_to_use_per_hour, example_keys[24],
                            base::BindLambdaForTesting([&](bool succeeded) {
                              EXPECT_TRUE(succeeded);
                              ++num_queries_processed;
                              run_loop.Quit();
                            }));

  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 26);
}

TEST_F(PrivateAggregationBudgeterTest, ConsumeBudgetDifferentApis) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey fledge_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);

  PrivateAggregationBudgetKey shared_storage_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kSharedStorage);

  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kMaxBudgetPerScope, fledge_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_TRUE(succeeded);
        ++num_queries_processed;
      }));

  base::RunLoop run_loop;

  // The budget for one API does not interfere with the other.
  budgeter()->ConsumeBudget(PrivateAggregationBudgeter::kMaxBudgetPerScope,
                            shared_storage_key,
                            base::BindLambdaForTesting([&](bool succeeded) {
                              EXPECT_TRUE(succeeded);
                              ++num_queries_processed;
                              run_loop.Quit();
                            }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 2);
}

TEST_F(PrivateAggregationBudgeterTest, ConsumeBudgetDifferentOrigins) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);

  PrivateAggregationBudgetKey key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://b.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);

  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kMaxBudgetPerScope, key_a,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_TRUE(succeeded);
        ++num_queries_processed;
      }));

  base::RunLoop run_loop;
  // The budget for one origin does not interfere with the other.
  budgeter()->ConsumeBudget(PrivateAggregationBudgeter::kMaxBudgetPerScope,
                            key_b,
                            base::BindLambdaForTesting([&](bool succeeded) {
                              EXPECT_TRUE(succeeded);
                              ++num_queries_processed;
                              run_loop.Quit();
                            }));
  run_loop.Run();
}

TEST_F(PrivateAggregationBudgeterTest, ConsumeBudgetExtremeValues) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);

  // Request will be rejected if budget non-positive
  budgeter()->ConsumeBudget(
      /*budget=*/-1, example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_FALSE(succeeded);
        ++num_queries_processed;
      }));
  budgeter()->ConsumeBudget(
      /*budget=*/0, example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_FALSE(succeeded);
        ++num_queries_processed;
      }));

  base::RunLoop run_loop;

  // Request will be rejected if budget exceeds maximum
  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope + 1),
      example_key, base::BindLambdaForTesting([&](bool succeeded) {
        EXPECT_FALSE(succeeded);
        ++num_queries_processed;
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 3);
}

TEST_F(PrivateAggregationBudgeterTest,
       ConsumeBudgetBeforeInitialized_QueriesAreQueued) {
  base::RunLoop run_loop;
  CreateBudgeter(/*exclusively_run_in_memory=*/false,
                 /*on_done_initializing=*/run_loop.QuitClosure());

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);

  // Queries should be processed in the order they are received.
  int num_queries_processed = 0;

  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_TRUE(succeeded);
        EXPECT_EQ(++num_queries_processed, 1);
      }));
  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope - 1),
      example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_TRUE(succeeded);
        EXPECT_EQ(++num_queries_processed, 2);
      }));
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_FALSE(succeeded);
        EXPECT_EQ(++num_queries_processed, 3);
      }));

  EXPECT_EQ(num_queries_processed, 0);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializing);

  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 3);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kOpen);
}

TEST_F(PrivateAggregationBudgeterTest,
       ConsumeBudgetBeforeFailedInitialization_QueuedQueriesAreRejected) {
  // The database initialization will fail to open if its directory already
  // exists.
  base::CreateDirectory(db_path());

  base::RunLoop run_loop;
  CreateBudgeter(/*exclusively_run_in_memory=*/false,
                 /*on_done_initializing=*/run_loop.QuitClosure());

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);

  // Queries should be processed in the order they are received.
  int num_queries_processed = 0;

  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_FALSE(succeeded);
        EXPECT_EQ(++num_queries_processed, 1);
      }));
  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope - 1),
      example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_FALSE(succeeded);
        EXPECT_EQ(++num_queries_processed, 2);
      }));
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&num_queries_processed](bool succeeded) {
        EXPECT_FALSE(succeeded);
        EXPECT_EQ(++num_queries_processed, 3);
      }));

  EXPECT_EQ(num_queries_processed, 0);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializing);

  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 3);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializationFailed);
}

TEST_F(PrivateAggregationBudgeterTest,
       MaxPendingCallsExceeded_AdditionalCallsRejected) {
  base::RunLoop run_loop;
  CreateBudgeter(/*exclusively_run_in_memory=*/false,
                 /*on_done_initializing=*/run_loop.QuitClosure());

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);

  int num_queries_succeeded = 0;

  for (int i = 0; i < PrivateAggregationBudgeter::kMaxPendingCalls; ++i) {
    // Queries should be processed in the order they are received.
    budgeter()->ConsumeBudget(
        /*budget=*/1, example_key,
        base::BindLambdaForTesting([&num_queries_succeeded, i](bool succeeded) {
          EXPECT_TRUE(succeeded);
          EXPECT_EQ(num_queries_succeeded++, i);
        }));
  }

  // This query should be immediately rejected.
  bool was_callback_run = false;
  budgeter()->ConsumeBudget(/*budget=*/1, example_key,
                            base::BindLambdaForTesting([&](bool succeeded) {
                              EXPECT_FALSE(succeeded);
                              EXPECT_EQ(num_queries_succeeded, 0);
                              was_callback_run = true;
                            }));

  EXPECT_EQ(num_queries_succeeded, 0);
  EXPECT_TRUE(was_callback_run);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializing);

  run_loop.Run();
  EXPECT_EQ(num_queries_succeeded,
            PrivateAggregationBudgeter::kMaxPendingCalls);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kOpen);
}

TEST_F(PrivateAggregationBudgeterTest,
       BudgeterDestroyedImmediatelyAfterInitialization_DoesNotCrash) {
  base::RunLoop run_loop;
  CreateBudgeter(
      /*exclusively_run_in_memory=*/false,
      /*on_done_initializing=*/base::BindLambdaForTesting([this, &run_loop]() {
        DestroyBudgeter();
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PrivateAggregationBudgeterTest,
       BudgeterDestroyedImmediatelyAfterCreation_DoesNotCrash) {
  CreateBudgeter(/*exclusively_run_in_memory=*/false,
                 /*on_done_initializing=*/base::DoNothing());
  DestroyBudgeter();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content

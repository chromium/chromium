// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budgeter.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budget_storage.h"
#include "content/browser/private_aggregation/proto/private_aggregation_budgets.pb.h"
#include "content/browser/storage_partition_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {
using BudgetEntryValidityStatus =
    PrivateAggregationBudgeter::BudgetValidityStatus;

using RequestResult = PrivateAggregationBudgeter::RequestResult;

const base::Time kExampleTime = base::Time::FromJavaTime(1652984901234);

class PrivateAggregationBudgeterUnderTest : public PrivateAggregationBudgeter {
 public:
  PrivateAggregationBudgeterUnderTest(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner,
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

  // This function adds the value received directly on the storage. As a result,
  // invalid data can be added.
  void AddBudgetValueAtTimestamp(const PrivateAggregationBudgetKey& budget_key,
                                 int budget_value,
                                 int64_t timestamp) {
    if (raw_storage_ == nullptr)
      return;

    std::string origin_key = budget_key.origin().Serialize();

    proto::PrivateAggregationBudgets budgets;
    raw_storage_->budgets_data()->TryGetData(origin_key, &budgets);

    google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetPerHour>*
        hourly_budgets =
            budget_key.api() == PrivateAggregationBudgetKey::Api::kFledge
                ? budgets.mutable_fledge_budgets()
                : budgets.mutable_shared_storage_budgets();

    proto::PrivateAggregationBudgetPerHour* new_budget = hourly_budgets->Add();
    new_budget->set_hour_start_timestamp(timestamp);
    new_budget->set_budget_used(budget_value);

    raw_storage_->budgets_data()->UpdateData(origin_key, budgets);
  }

  void DeleteAllData() { raw_storage_->budgets_data()->DeleteAllData(); }

 private:
  void OnStorageDoneInitializing(
      std::unique_ptr<PrivateAggregationBudgetStorage> storage) override {
    raw_storage_ = storage.get();
    PrivateAggregationBudgeter::OnStorageDoneInitializing(std::move(storage));
    if (on_storage_done_initializing_)
      std::move(on_storage_done_initializing_).Run();
  }

  base::OnceClosure on_storage_done_initializing_;

  // This storage is owned by the base budgeter class so this raw pointer on the
  // derived class is destroyed first and won't become dangling.
  raw_ptr<PrivateAggregationBudgetStorage> raw_storage_;
};

// TODO(alexmt): Consider moving logic shared with
// PrivateAggregationBudgetStorageTest to a joint test harness.
class PrivateAggregationBudgeterTest : public testing::Test {
 public:
  PrivateAggregationBudgeterTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    db_task_runner_ = base::ThreadPool::CreateUpdateableSequencedTaskRunner(
        {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
         base::ThreadPolicy::MUST_USE_FOREGROUND});
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

  void AddBudgetValueAtTimestamp(const PrivateAggregationBudgetKey& budget_key,
                                 int value,
                                 int64_t timestamp) {
    budgeter_.get()->AddBudgetValueAtTimestamp(budget_key, value, timestamp);
  }

  void DeleteAllBudgetData() { budgeter_.get()->DeleteAllData(); }

  PrivateAggregationBudgetKey CreateBudgetKey() {
    return PrivateAggregationBudgetKey::CreateForTesting(
        /*origin=*/url::Origin::Create(GURL("https://a.example/")),
        /*api_invocation_time=*/kExampleTime,
        /*api-*/ PrivateAggregationBudgetKey::Api::kFledge);
  }

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
  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
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
  budgeter()->ConsumeBudget(PrivateAggregationBudgeter::kMaxBudgetPerScope,
                            example_key,
                            base::BindLambdaForTesting(
                                [&num_queries_processed](RequestResult result) {
                                  EXPECT_EQ(result, RequestResult::kApproved);
                                  ++num_queries_processed;
                                }));

  // Ensure database has a chance to persist storage.
  EnsureDbFlushes();

  DestroyBudgeter();
  CreateBudgeterAndWait();

  base::RunLoop run_loop;
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kInsufficientBudget);
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
  budgeter()->ConsumeBudget(PrivateAggregationBudgeter::kMaxBudgetPerScope,
                            example_key,
                            base::BindLambdaForTesting(
                                [&num_queries_processed](RequestResult result) {
                                  EXPECT_EQ(result, RequestResult::kApproved);
                                  ++num_queries_processed;
                                }));

  // Ensure database has a chance to persist storage.
  EnsureDbFlushes();

  DestroyBudgeter();
  CreateBudgeterAndWait(/*exclusively_run_in_memory=*/true);

  base::RunLoop run_loop;
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
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
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  // Budget can be increased to max
  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope - 1),
      example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  base::RunLoop run_loop;

  // Budget cannot be increased above max
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kInsufficientBudget);
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
        base::BindLambdaForTesting(
            [&num_queries_processed](RequestResult result) {
              EXPECT_EQ(result, RequestResult::kApproved);
              ++num_queries_processed;
            }));
  }

  // The last 24 keys are used for calculating remaining budget, so we can't
  // use more during the 24th time window.
  budgeter()->ConsumeBudget(budget_to_use_per_hour, example_keys[23],
                            base::BindLambdaForTesting(
                                [&num_queries_processed](RequestResult result) {
                                  EXPECT_EQ(result,
                                            RequestResult::kInsufficientBudget);
                                  ++num_queries_processed;
                                }));

  base::RunLoop run_loop;

  // But the last key can use budget as the first key is no longer in the
  // relevant set of 24 time windows.
  budgeter()->ConsumeBudget(
      budget_to_use_per_hour, example_keys[24],
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
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
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult request) {
            EXPECT_EQ(request, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  base::RunLoop run_loop;

  // The budget for one API does not interfere with the other.
  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kMaxBudgetPerScope, shared_storage_key,
      base::BindLambdaForTesting([&](RequestResult request) {
        EXPECT_EQ(request, RequestResult::kApproved);
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

  budgeter()->ConsumeBudget(PrivateAggregationBudgeter::kMaxBudgetPerScope,
                            key_a,
                            base::BindLambdaForTesting(
                                [&num_queries_processed](RequestResult result) {
                                  EXPECT_EQ(result, RequestResult::kApproved);
                                  ++num_queries_processed;
                                }));

  base::RunLoop run_loop;
  // The budget for one origin does not interfere with the other.
  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kMaxBudgetPerScope, key_b,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 2);
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
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInvalidRequest);
            ++num_queries_processed;
          }));
  budgeter()->ConsumeBudget(
      /*budget=*/0, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInvalidRequest);
            ++num_queries_processed;
          }));

  base::RunLoop run_loop;

  // Request will be rejected if budget exceeds maximum
  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope + 1),
      example_key, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kRequestedMoreThanTotalBudget);
        ++num_queries_processed;
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 3);
}

TEST_F(PrivateAggregationBudgeterTest, BudgetValidityMetricsRecorded) {
  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey budget_key = CreateBudgetKey();

  constexpr int64_t scope_duration =
      PrivateAggregationBudgeter::kBudgetScopeDuration.InMicroseconds();
  constexpr int64_t window_duration =
      PrivateAggregationBudgetKey::TimeWindow::kDuration.InMicroseconds();
  int64_t latest_window_start = budget_key.time_window()
                                    .start_time()
                                    .ToDeltaSinceWindowsEpoch()
                                    .InMicroseconds();

  int64_t oldest_window_start =
      latest_window_start + window_duration - scope_duration;

  int64_t after_latest_window_start = latest_window_start + window_duration;
  int64_t before_oldest_window_start = oldest_window_start - window_duration;

  constexpr int max_budget = PrivateAggregationBudgeter::kMaxBudgetPerScope;

  struct HourlyBudget {
    int budget;
    int64_t timestamp;
  };

  const struct {
    BudgetEntryValidityStatus expected_status;
    std::vector<HourlyBudget> budgets;
  } kTestCases[] = {
      {BudgetEntryValidityStatus::kValid,
       {
           {1, oldest_window_start},
           {1, latest_window_start},
       }},
      {BudgetEntryValidityStatus::kValidAndEmpty, {}},
      {BudgetEntryValidityStatus::kValidButContainsStaleWindow,
       {

           {1, before_oldest_window_start},
           {1, oldest_window_start},
       }},
      {BudgetEntryValidityStatus::kContainsTimestampInFuture,
       {
           {1, latest_window_start},
           {3, after_latest_window_start},
       }},
      {BudgetEntryValidityStatus::kContainsValueExceedingLimit,
       {
           {1, oldest_window_start},
           {max_budget + 1, latest_window_start},
       }},
      {BudgetEntryValidityStatus::kContainsTimestampNotRoundedToHour,
       {
           {1, oldest_window_start},
           {max_budget, latest_window_start + 1},
       }},
      {BudgetEntryValidityStatus::kContainsNonPositiveValue,
       {
           {-1, after_latest_window_start},
           {3, oldest_window_start + 1},
           {max_budget + 1, latest_window_start},
       }},
      {BudgetEntryValidityStatus::kSpansMoreThanADay,
       {
           {5, before_oldest_window_start},
           {3, latest_window_start},
       }}};

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;
    base::RunLoop run_loop;
    for (const auto& budget : test_case.budgets) {
      AddBudgetValueAtTimestamp(budget_key, budget.budget, budget.timestamp);
    }

    budgeter()->ConsumeBudget(
        /*budget=*/1, budget_key,
        base::BindLambdaForTesting([&](RequestResult result) {
          DeleteAllBudgetData();
          run_loop.Quit();
        }));
    histograms.ExpectUniqueSample(
        "PrivacySandbox.PrivateAggregation.Budgeter.BudgetValidityStatus",
        test_case.expected_status, 1);
    run_loop.Run();
  }
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
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            EXPECT_EQ(++num_queries_processed, 1);
          }));
  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope - 1),
      example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            EXPECT_EQ(++num_queries_processed, 2);
          }));
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
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
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kStorageInitializationFailed);
            EXPECT_EQ(++num_queries_processed, 1);
          }));
  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope - 1),
      example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kStorageInitializationFailed);
            EXPECT_EQ(++num_queries_processed, 2);
          }));
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kStorageInitializationFailed);
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
       MaxPendingCallsExceeded_AdditionalConsumeBudgetCallsRejected) {
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
        base::BindLambdaForTesting(
            [&num_queries_succeeded, i](RequestResult result) {
              EXPECT_EQ(result, RequestResult::kApproved);
              EXPECT_EQ(num_queries_succeeded++, i);
            }));
  }

  // This query should be immediately rejected.
  bool was_callback_run = false;
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kTooManyPendingCalls);
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
       MaxPendingCallsExceeded_AdditionalDataClearingCallsAllowed) {
  base::RunLoop run_loop;
  CreateBudgeter(/*exclusively_run_in_memory=*/false,
                 /*on_done_initializing=*/base::DoNothing());

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          base::Time::FromJavaTime(1652984901234),
          PrivateAggregationBudgetKey::Api::kFledge);

  int num_consume_queries_succeeded = 0;

  for (int i = 0; i < PrivateAggregationBudgeter::kMaxPendingCalls; ++i) {
    // Queries should be processed in the order they are received.
    budgeter()->ConsumeBudget(
        /*budget=*/1, example_key,
        base::BindLambdaForTesting(
            [&num_consume_queries_succeeded, i](RequestResult result) {
              EXPECT_EQ(result, RequestResult::kApproved);
              EXPECT_EQ(num_consume_queries_succeeded++, i);
            }));
  }

  EXPECT_EQ(num_consume_queries_succeeded, 0);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializing);

  // Despite the limit being reached, data clearing requests are allowed to
  // cause the limit to be exceeded and are queued.
  bool was_callback_run = false;
  budgeter()->ClearData(base::Time::Min(), base::Time::Max(),
                        StoragePartition::StorageKeyMatcherFunction(),
                        base::BindLambdaForTesting([&]() {
                          was_callback_run = true;
                          run_loop.Quit();
                        }));
  EXPECT_FALSE(was_callback_run);

  run_loop.Run();
  EXPECT_EQ(num_consume_queries_succeeded,
            PrivateAggregationBudgeter::kMaxPendingCalls);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kOpen);
  EXPECT_TRUE(was_callback_run);
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

TEST_F(PrivateAggregationBudgeterTest, ClearDataBasicTest) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
            ++num_queries_processed;
          }));

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(kExampleTime, kExampleTime,
                        StoragePartition::StorageKeyMatcherFunction(),
                        base::BindLambdaForTesting([&]() {
                          ++num_queries_processed;
                          run_loop.Quit();
                        }));

  // After clearing, we can use the full budget again
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 4);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataCrossesWindowBoundary) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey example_key_1 =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);

  PrivateAggregationBudgetKey example_key_2 =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          kExampleTime + PrivateAggregationBudgetKey::TimeWindow::kDuration,
          PrivateAggregationBudgetKey::Api::kFledge);

  EXPECT_NE(example_key_1.time_window().start_time(),
            example_key_2.time_window().start_time());

  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_1,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope - 1),
      example_key_2,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  // The full budget has been used across the two time windows.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_2,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
            ++num_queries_processed;
          }));

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;

  budgeter()->ClearData(
      kExampleTime,
      kExampleTime + PrivateAggregationBudgetKey::TimeWindow::kDuration,
      StoragePartition::StorageKeyMatcherFunction(),
      base::BindLambdaForTesting([&]() {
        ++num_queries_processed;
        run_loop.Quit();
      }));

  // After clearing, we can use the full budget again.
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key_2,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 5);
}

TEST_F(PrivateAggregationBudgeterTest,
       ClearDataDoesntAffectWindowsOutsideRange) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey key_to_clear =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);

  PrivateAggregationBudgetKey key_after =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          kExampleTime + PrivateAggregationBudgetKey::TimeWindow::kDuration,
          PrivateAggregationBudgetKey::Api::kFledge);

  PrivateAggregationBudgetKey key_before =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          kExampleTime - PrivateAggregationBudgetKey::TimeWindow::kDuration,
          PrivateAggregationBudgetKey::Api::kFledge);

  EXPECT_LT(key_to_clear.time_window().start_time(),
            key_after.time_window().start_time());

  EXPECT_GT(key_to_clear.time_window().start_time(),
            key_before.time_window().start_time());

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/1, key_before, expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope - 2),
      key_to_clear, expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/1, key_after, expect_approved);

  // The full budget has been used across the three time windows.
  budgeter()->ConsumeBudget(
      /*budget=*/1, key_after,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
            ++num_queries_processed;
          }));

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;

  // This will only clear the `key_to_clear`'s budget.
  budgeter()->ClearData(kExampleTime, kExampleTime,
                        StoragePartition::StorageKeyMatcherFunction(),
                        base::BindLambdaForTesting([&]() {
                          ++num_queries_processed;
                          run_loop.Quit();
                        }));

  // After clearing, we can have a budget of exactly
  // (`PrivateAggregationBudgeter::kMaxBudgetPerScope` - 2) that we can use.
  budgeter()->ConsumeBudget(
      /*budget=*/(PrivateAggregationBudgeter::kMaxBudgetPerScope - 2),
      key_after, expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/1, key_after,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kInsufficientBudget);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 7);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataAllApisAffected) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey fledge_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);

  PrivateAggregationBudgetKey shared_storage_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationBudgetKey::Api::kSharedStorage);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, fledge_key,
      expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, fledge_key, expect_insufficient_budget);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope,
      shared_storage_key, expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, shared_storage_key, expect_insufficient_budget);

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(kExampleTime, kExampleTime,
                        StoragePartition::StorageKeyMatcherFunction(),
                        base::BindLambdaForTesting([&]() {
                          ++num_queries_processed;
                          run_loop.Quit();
                        }));

  // After clearing, we can use the full budget again
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, fledge_key,
      expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope,
      shared_storage_key, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 7);
}

TEST_F(PrivateAggregationBudgeterTest, ClearAllDataBasicTest) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
            ++num_queries_processed;
          }));

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(base::Time::Min(), base::Time::Max(),
                        StoragePartition::StorageKeyMatcherFunction(),
                        base::BindLambdaForTesting([&]() {
                          ++num_queries_processed;
                          run_loop.Quit();
                        }));

  // After clearing, we can use the full budget again
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 4);
}

TEST_F(PrivateAggregationBudgeterTest, ClearAllDataNullTimes) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
            ++num_queries_processed;
          }));

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(base::Time(), base::Time(),
                        StoragePartition::StorageKeyMatcherFunction(),
                        base::BindLambdaForTesting([&]() {
                          ++num_queries_processed;
                          run_loop.Quit();
                        }));

  // After clearing, we can use the full budget again
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 4);
}

TEST_F(PrivateAggregationBudgeterTest, ClearAllDataNullStartNonNullEndTime) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
            ++num_queries_processed;
          }));

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(base::Time(), base::Time::Max(),
                        StoragePartition::StorageKeyMatcherFunction(),
                        base::BindLambdaForTesting([&]() {
                          ++num_queries_processed;
                          run_loop.Quit();
                        }));

  // After clearing, we can use the full budget again
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 4);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataFilterSelectsOrigins) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.example/"));

  PrivateAggregationBudgetKey example_key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginA, kExampleTime, PrivateAggregationBudgetKey::Api::kFledge);

  PrivateAggregationBudgetKey example_key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginB, kExampleTime, PrivateAggregationBudgetKey::Api::kFledge);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key_a,
      expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_a, expect_insufficient_budget);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key_b,
      expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_b, expect_insufficient_budget);

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(
      kExampleTime, kExampleTime,
      base::BindLambdaForTesting([&](const blink::StorageKey& storage_key) {
        return storage_key == blink::StorageKey::CreateFirstParty(kOriginA);
      }),
      base::BindLambdaForTesting([&]() {
        ++num_queries_processed;
        run_loop.Quit();
      }));

  // After clearing, we can use the full budget again for the cleared origin.
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key_a,
      expect_approved);
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key_b,
      expect_insufficient_budget);
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 7);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataAllTimeFilterSelectsOrigins) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.example/"));

  PrivateAggregationBudgetKey example_key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginA, kExampleTime, PrivateAggregationBudgetKey::Api::kFledge);

  PrivateAggregationBudgetKey example_key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginB, kExampleTime, PrivateAggregationBudgetKey::Api::kFledge);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key_a,
      expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_a, expect_insufficient_budget);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key_b,
      expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_b, expect_insufficient_budget);

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindLambdaForTesting([&](const blink::StorageKey& storage_key) {
        return storage_key == blink::StorageKey::CreateFirstParty(kOriginA);
      }),
      base::BindLambdaForTesting([&]() {
        ++num_queries_processed;
        run_loop.Quit();
      }));

  // After clearing, we can use the full budget again for the cleared origin.
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key_a,
      expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key_b,
      expect_insufficient_budget);
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 7);
}

TEST_F(PrivateAggregationBudgeterTest,
       BudgeterDestroyedImmedatelyAfterClearData_CallbackStillRun) {
  int num_queries_processed = 0;

  CreateBudgeterAndWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kMaxBudgetPerScope, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  base::RunLoop run_loop;
  budgeter()->ClearData(base::Time(), base::Time(),
                        StoragePartition::StorageKeyMatcherFunction(),
                        base::BindLambdaForTesting([&]() {
                          ++num_queries_processed;
                          run_loop.Quit();
                        }));
  DestroyBudgeter();

  // Callback still run even though the budgeter was immediately destroyed.
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 2);
}

}  // namespace

}  // namespace content

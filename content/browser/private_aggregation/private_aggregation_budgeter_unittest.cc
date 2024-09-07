// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budgeter.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budget_storage.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/proto/private_aggregation_budgets.pb.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {
using BudgetEntryValidityStatus =
    PrivateAggregationBudgeter::BudgetValidityStatus;

using RequestResult = PrivateAggregationBudgeter::RequestResult;

constexpr auto kExampleTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1652984901234);

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
    if (raw_storage_ == nullptr) {
      return;
    }

    std::string site_key = net::SchemefulSite(budget_key.origin()).Serialize();

    proto::PrivateAggregationBudgets budgets;
    raw_storage_->budgets_data()->TryGetData(site_key, &budgets);

    google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
        budget_entries =
            budget_key.api() == PrivateAggregationCallerApi::kProtectedAudience
                ? budgets.mutable_protected_audience_budgets()
                : budgets.mutable_shared_storage_budgets();

    proto::PrivateAggregationBudgetEntry* new_budget = budget_entries->Add();
    new_budget->set_entry_start_timestamp(timestamp);
    new_budget->set_budget_used(budget_value);

    raw_storage_->budgets_data()->UpdateData(site_key, budgets);
  }

  int NumberOfSitesInStorage() {
    if (raw_storage_ == nullptr) {
      return 0;
    }

    return raw_storage_->budgets_data()->GetAllCached().size();
  }

  void DeleteAllData() { raw_storage_->budgets_data()->DeleteAllData(); }

 private:
  void OnStorageDoneInitializing(
      std::unique_ptr<PrivateAggregationBudgetStorage> storage) override {
    raw_storage_ = storage.get();
    PrivateAggregationBudgeter::OnStorageDoneInitializing(std::move(storage));
    if (on_storage_done_initializing_) {
      std::move(on_storage_done_initializing_).Run();
    }
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

  void CreateBudgeterWithoutInitializing(
      bool exclusively_run_in_memory = false,
      base::OnceClosure on_done_initializing = base::DoNothing()) {
    budgeter_ = std::make_unique<PrivateAggregationBudgeterUnderTest>(
        db_task_runner_, exclusively_run_in_memory, storage_directory(),
        std::move(on_done_initializing));
  }

  void CreateAndInitializeBudgeterThenWait(
      bool exclusively_run_in_memory = false) {
    base::RunLoop run_loop;
    CreateBudgeterWithoutInitializing(exclusively_run_in_memory);
    InitializeBudgeter(/*on_done_initializing=*/run_loop.QuitClosure());
    run_loop.Run();
  }

  // Initializes budgeter's storage by calling `ClearData` (which initiates
  // storage initialization) on an empty range.
  void InitializeBudgeter(base::OnceClosure on_done_initializing) {
    ASSERT_EQ(
        GetStorageStatus(),
        PrivateAggregationBudgeter::StorageStatus::kPendingInitialization);

    budgeter()->ClearData(base::Time::Min(), base::Time::Min(),
                          StoragePartition::StorageKeyMatcherFunction(),
                          std::move(on_done_initializing));
    ASSERT_GT(
        GetStorageStatus(),
        PrivateAggregationBudgeter::StorageStatus::kPendingInitialization);
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

  int NumberOfSitesInStorage() {
    return budgeter_.get()->NumberOfSitesInStorage();
  }

  void DeleteAllBudgetData() { budgeter_.get()->DeleteAllData(); }

  PrivateAggregationBudgetKey CreateBudgetKey() {
    return PrivateAggregationBudgetKey::CreateForTesting(
        /*origin=*/url::Origin::Create(GURL("https://a.example/")),
        /*api_invocation_time=*/kExampleTime,
        /*api=*/PrivateAggregationCallerApi::kProtectedAudience);
  }

  base::FilePath db_path() const {
    return storage_directory().Append(FILE_PATH_LITERAL("PrivateAggregation"));
  }

  PrivateAggregationBudgeter::StorageStatus GetStorageStatus() const {
    return budgeter_->GetStorageStatus();
  }

 protected:
  base::FilePath storage_directory() const { return temp_directory_.GetPath(); }

  base::ScopedTempDir temp_directory_;
  std::unique_ptr<PrivateAggregationBudgeterUnderTest> budgeter_;
  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PrivateAggregationBudgeterTest,
       BudgeterCreated_DatabaseInitializedLazily) {
  bool is_done_initializing = false;
  CreateBudgeterWithoutInitializing(
      /*exclusively_run_in_memory=*/false,
      /*on_done_initializing=*/base::BindLambdaForTesting(
          [&is_done_initializing]() { is_done_initializing = true; }));
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kPendingInitialization);
  EXPECT_FALSE(is_done_initializing);

  base::RunLoop run_loop;
  InitializeBudgeter(/*on_done_initializing=*/run_loop.QuitClosure());
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializing);
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

  CreateAndInitializeBudgeterThenWait();

  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializationFailed);
}

TEST_F(PrivateAggregationBudgeterTest, InMemory_StillInitializes) {
  CreateAndInitializeBudgeterThenWait(/*exclusively_run_in_memory=*/true);

  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kOpen);
}

TEST_F(PrivateAggregationBudgeterTest, DatabaseReopened_DataPersisted) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);
  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope,
      example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  // Ensure database has a chance to persist storage.
  EnsureDbFlushes();

  DestroyBudgeter();
  CreateAndInitializeBudgeterThenWait();

  base::RunLoop run_loop;
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
        ++num_queries_processed;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 2);
}

TEST_F(PrivateAggregationBudgeterTest,
       InMemoryDatabaseReopened_DataNotPersisted) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait(/*exclusively_run_in_memory=*/true);

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);
  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope,
      example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  // Ensure database has a chance to persist storage.
  EnsureDbFlushes();

  DestroyBudgeter();
  CreateAndInitializeBudgeterThenWait(/*exclusively_run_in_memory=*/true);

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

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

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
      /*budget=*/(
          PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope -
          1),
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
        EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
        ++num_queries_processed;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 3);
}

TEST_F(PrivateAggregationBudgeterTest,
       ConsumeBudgetDifferentWindowsSmallerScope) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  base::Time reference_time = kExampleTime;

  // Create 10 min worth of budget keys for a particular site-API pair
  // (with varying time windows) plus one extra.
  std::vector<PrivateAggregationBudgetKey> example_keys;
  for (int i = 0; i < 11; ++i) {
    example_keys.push_back(PrivateAggregationBudgetKey::CreateForTesting(
        url::Origin::Create(GURL("https://a.example")),
        reference_time + i * base::Minutes(1),
        PrivateAggregationCallerApi::kProtectedAudience));
  }

  // Consuming this budget 10 times in 10 min would not exceed the daily budget,
  // but 11 times would.
  int budget_to_use_per_minute =
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope / 10;
  EXPECT_GT(
      budget_to_use_per_minute * 11,
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope);

  // Use budget in the first 10 keys.
  for (int i = 0; i < 10; ++i) {
    budgeter()->ConsumeBudget(
        budget_to_use_per_minute, example_keys[i],
        base::BindLambdaForTesting(
            [&num_queries_processed](RequestResult result) {
              EXPECT_EQ(result, RequestResult::kApproved);
              ++num_queries_processed;
            }));
  }

  // The last 10 keys are used for calculating remaining budget, so we can't
  // use more during the 10th time window.
  budgeter()->ConsumeBudget(
      budget_to_use_per_minute, example_keys[9],
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
            ++num_queries_processed;
          }));

  base::RunLoop run_loop;

  // But the last key can use budget as the first key is no longer in the
  // relevant set of 10 smaller scope time windows.
  budgeter()->ConsumeBudget(
      budget_to_use_per_minute, example_keys[10],
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 12);
}

TEST_F(PrivateAggregationBudgeterTest,
       ConsumeBudgetDifferentWindowsLargerScope) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  base::Time reference_time = kExampleTime;

  // Create a day's worth of budget keys for a particular site-API pair
  // (with varying time windows) plus one extra.
  std::vector<PrivateAggregationBudgetKey> example_keys;
  for (int i = 0; i < 1441; ++i) {
    example_keys.push_back(PrivateAggregationBudgetKey::CreateForTesting(
        url::Origin::Create(GURL("https://a.example")),
        reference_time + i * base::Minutes(1),
        PrivateAggregationCallerApi::kProtectedAudience));
  }

  // Consuming this budget 1440 times in a day would not exceed the daily
  // budget, but 25 times would.
  int budget_to_use_per_minute =
      PrivateAggregationBudgeter::kLargerScopeValues.max_budget_per_scope /
      1440;
  EXPECT_GT(
      budget_to_use_per_minute * 1441,
      PrivateAggregationBudgeter::kLargerScopeValues.max_budget_per_scope);

  // Make sure this is valid to request within a 10 min period.
  EXPECT_LT(
      10 * budget_to_use_per_minute,
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope);

  // Use budget in the first 1440 larger scopes.
  for (int i = 0; i < 1440; ++i) {
    budgeter()->ConsumeBudget(
        budget_to_use_per_minute, example_keys[i],
        base::BindLambdaForTesting(
            [&num_queries_processed](RequestResult result) {
              EXPECT_EQ(result, RequestResult::kApproved);
              ++num_queries_processed;
            }));
  }

  // The last 1440 larger scope windows are used for calculating remaining
  // budget, so we can't use more during the 1440th.
  budgeter()->ConsumeBudget(
      budget_to_use_per_minute, example_keys[1439],
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientLargerScopeBudget);
            ++num_queries_processed;
          }));

  base::RunLoop run_loop;

  // But the last window can use budget as the first window is no longer in the
  // relevant set of 1440 larger scope windows.
  budgeter()->ConsumeBudget(
      budget_to_use_per_minute, example_keys[1440],
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 1442);
}

TEST_F(PrivateAggregationBudgeterTest, ConsumeBudgetDifferentApis) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey protected_audience_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey shared_storage_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kSharedStorage);

  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope,
      protected_audience_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult request) {
            EXPECT_EQ(request, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  base::RunLoop run_loop;

  // The budget for one API does not interfere with the other.
  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope,
      shared_storage_key,
      base::BindLambdaForTesting([&](RequestResult request) {
        EXPECT_EQ(request, RequestResult::kApproved);
        ++num_queries_processed;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 2);
}

TEST_F(PrivateAggregationBudgeterTest, ConsumeBudgetDifferentSites) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://b.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope,
      key_a,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  base::RunLoop run_loop;
  // The budget for one site does not interfere with the other.
  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope,
      key_b, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 2);
}

TEST_F(PrivateAggregationBudgeterTest, ConsumeBudgetDifferentOriginsSameSite) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.domain.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://b.domain.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  budgeter()->ConsumeBudget(
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope,
      key_a,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          }));

  base::RunLoop run_loop;
  // The budget is shared for different origins in the same site.
  budgeter()->ConsumeBudget(
      /*budget=*/1, key_b,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
        ++num_queries_processed;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 2);
}

TEST_F(PrivateAggregationBudgeterTest, ConsumeBudgetValueTooLarge) {
  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  base::RunLoop run_loop;

  // Request will be rejected if budget exceeds maximum
  budgeter()->ConsumeBudget(
      /*budget=*/(
          PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope +
          1),
      example_key, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kRequestedMoreThanTotalBudget);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PrivateAggregationBudgeterTest, BudgetValidityMetricsRecorded) {
  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey budget_key = CreateBudgetKey();

  constexpr int64_t kLargerScopeDuration =
      PrivateAggregationBudgeter::kLargerScopeValues.budget_scope_duration
          .InMicroseconds();
  constexpr int64_t kWindowDuration =
      PrivateAggregationBudgetKey::TimeWindow::kDuration.InMicroseconds();

  int64_t latest_window_start = budget_key.time_window()
                                    .start_time()
                                    .ToDeltaSinceWindowsEpoch()
                                    .InMicroseconds();

  int64_t oldest_window_start =
      latest_window_start + kWindowDuration - kLargerScopeDuration;

  int64_t after_latest_window_start = latest_window_start + kWindowDuration;
  int64_t before_oldest_window_start = oldest_window_start - kWindowDuration;

  constexpr int kMaxSmallerScopeBudget =
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope;

  struct BudgetEntries {
    int budget;
    int64_t timestamp;
  };

  const struct {
    BudgetEntryValidityStatus expected_status;
    std::vector<BudgetEntries> budgets;
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
           {kMaxSmallerScopeBudget + 1, latest_window_start},
       }},
      {BudgetEntryValidityStatus::kContainsTimestampNotRoundedToMinute,
       {
           {1, oldest_window_start},
           {kMaxSmallerScopeBudget, latest_window_start + 1},
       }},
      {BudgetEntryValidityStatus::kContainsNonPositiveValue,
       {
           {-1, after_latest_window_start},
           {3, oldest_window_start + 1},
           {kMaxSmallerScopeBudget + 1, latest_window_start},
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
        "PrivacySandbox.PrivateAggregation.Budgeter.BudgetValidityStatus2",
        test_case.expected_status, 1);
    run_loop.Run();
  }
}

TEST_F(PrivateAggregationBudgeterTest,
       EnoughBudgetIfNotEnoughOverallMetricRecorded) {
  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey budget_key = CreateBudgetKey();

  constexpr int64_t kLargerScopeDuration =
      PrivateAggregationBudgeter::kLargerScopeValues.budget_scope_duration
          .InMicroseconds();
  constexpr int64_t kWindowDuration =
      PrivateAggregationBudgetKey::TimeWindow::kDuration.InMicroseconds();

  int64_t latest_window_start = budget_key.time_window()
                                    .start_time()
                                    .ToDeltaSinceWindowsEpoch()
                                    .InMicroseconds();

  int64_t oldest_window_start =
      latest_window_start + kWindowDuration - kLargerScopeDuration;

  constexpr int kMaxSmallerScopeBudget =
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope;
  constexpr int kMaxLargerScopeBudget =
      PrivateAggregationBudgeter::kLargerScopeValues.max_budget_per_scope;

  const struct {
    std::string_view description;
    std::optional<bool> expected_status;
    int amount_to_consume;
    int minimum_histogram_value;
    int smaller_scope_budget_used;
    int larger_scope_budget_used;
  } kTestCases[] = {
      {
          "amount is approved",
          /*expected_status=*/std::nullopt,
          /*amount_to_consume=*/1000,
          /*minimum_histogram_value=*/10,
          /*smaller_scope_budget_used=*/0,
          /*larger_scope_budget_used=*/0,
      },
      {
          "insufficent small scope budget, but minimum would succeed",
          /*expected_status=*/true,
          /*amount_to_consume=*/1000,
          /*minimum_histogram_value=*/10,
          /*smaller_scope_budget_used=*/kMaxSmallerScopeBudget - 100,
          /*larger_scope_budget_used=*/0,
      },
      {
          "insufficent small scope budget, minimum would also fail",
          /*expected_status=*/false,
          /*amount_to_consume=*/1000,
          /*minimum_histogram_value=*/10,
          /*smaller_scope_budget_used=*/kMaxSmallerScopeBudget - 1,
          /*larger_scope_budget_used=*/0,
      },
      {
          "insufficent large scope budget, but minimum would succeed",
          /*expected_status=*/true,
          /*amount_to_consume=*/1000,
          /*minimum_histogram_value=*/10,
          /*smaller_scope_budget_used=*/0,
          /*larger_scope_budget_used=*/kMaxLargerScopeBudget - 100,
      },
      {
          "insufficent large scope budget, minimum would also fail",
          /*expected_status=*/false,
          /*amount_to_consume=*/1000,
          /*minimum_histogram_value=*/10,
          /*smaller_scope_budget_used=*/0,
          /*larger_scope_budget_used=*/kMaxLargerScopeBudget - 1,
      },
      {
          "requested more than the total budget",
          /*expected_status=*/std::nullopt,
          /*amount_to_consume=*/kMaxSmallerScopeBudget + 1,
          /*minimum_histogram_value=*/10,
          /*smaller_scope_budget_used=*/0,
          /*larger_scope_budget_used=*/0,
      },
      {
          "minimum_histogram_value 0",
          /*expected_status=*/std::nullopt,
          /*amount_to_consume=*/1000,
          /*minimum_histogram_value=*/0,
          /*smaller_scope_budget_used=*/0,
          /*larger_scope_budget_used=*/0,
      }};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    base::HistogramTester histograms;
    base::RunLoop run_loop;

    if (test_case.smaller_scope_budget_used) {
      AddBudgetValueAtTimestamp(budget_key, test_case.smaller_scope_budget_used,
                                latest_window_start);
    }
    if (test_case.larger_scope_budget_used) {
      AddBudgetValueAtTimestamp(budget_key, test_case.larger_scope_budget_used,
                                oldest_window_start);
    }

    budgeter()->ConsumeBudget(
        /*budget=*/test_case.amount_to_consume, budget_key,
        test_case.minimum_histogram_value,
        base::BindLambdaForTesting([&](RequestResult result) {
          DeleteAllBudgetData();
          run_loop.Quit();
        }));

    constexpr std::string_view kHistogram =
        "PrivacySandbox.PrivateAggregation.Budgeter."
        "EnoughBudgetForAnyValueIfNotEnoughOverall";

    if (test_case.expected_status.has_value()) {
      histograms.ExpectUniqueSample(kHistogram,
                                    test_case.expected_status.value(), 1);
    } else {
      histograms.ExpectTotalCount(kHistogram, 0);
    }
    run_loop.Run();
  }
}

TEST_F(PrivateAggregationBudgeterTest,
       ConsumeBudgetBeforeInitialized_QueriesAreQueued) {
  base::RunLoop run_loop;
  CreateBudgeterWithoutInitializing();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

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
      /*budget=*/(
          PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope -
          1),
      example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            EXPECT_EQ(++num_queries_processed, 2);
          }));
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed, &run_loop](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
            EXPECT_EQ(++num_queries_processed, 3);
            run_loop.Quit();
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
       ClearDataBeforeInitialized_QueriesAreQueued) {
  base::RunLoop run_loop;
  CreateBudgeterWithoutInitializing();

  bool was_callback_run = false;
  budgeter()->ClearData(base::Time::Min(), base::Time::Max(),
                        StoragePartition::StorageKeyMatcherFunction(),
                        base::BindLambdaForTesting([&]() {
                          was_callback_run = true;
                          run_loop.Quit();
                        }));

  EXPECT_FALSE(was_callback_run);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kInitializing);

  run_loop.Run();

  EXPECT_TRUE(was_callback_run);
  EXPECT_EQ(GetStorageStatus(),
            PrivateAggregationBudgeter::StorageStatus::kOpen);
}

TEST_F(PrivateAggregationBudgeterTest,
       ConsumeBudgetBeforeFailedInitialization_QueuedQueriesAreRejected) {
  // The database initialization will fail to open if its directory already
  // exists.
  base::CreateDirectory(db_path());

  base::RunLoop run_loop;
  CreateBudgeterWithoutInitializing();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

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
      /*budget=*/(
          PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope -
          1),
      example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kStorageInitializationFailed);
            EXPECT_EQ(++num_queries_processed, 2);
          }));
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key,
      base::BindLambdaForTesting(
          [&num_queries_processed, &run_loop](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kStorageInitializationFailed);
            EXPECT_EQ(++num_queries_processed, 3);
            run_loop.Quit();
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
  CreateBudgeterWithoutInitializing();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  int num_queries_succeeded = 0;

  base::RepeatingClosure barrier_quit_closure = base::BarrierClosure(
      PrivateAggregationBudgeter::kMaxPendingCalls, run_loop.QuitClosure());
  for (int i = 0; i < PrivateAggregationBudgeter::kMaxPendingCalls; ++i) {
    // Queries should be processed in the order they are received.
    budgeter()->ConsumeBudget(
        /*budget=*/1, example_key,
        base::BindLambdaForTesting(
            [&num_queries_succeeded, i,
             &barrier_quit_closure](RequestResult result) {
              EXPECT_EQ(result, RequestResult::kApproved);
              EXPECT_EQ(num_queries_succeeded++, i);
              barrier_quit_closure.Run();
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
  CreateBudgeterWithoutInitializing();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

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
       BudgeterDestroyedImmediatelyAfterCreation_DoesNotCrash) {
  CreateBudgeterWithoutInitializing(/*exclusively_run_in_memory=*/false);
  DestroyBudgeter();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivateAggregationBudgeterTest,
       BudgeterDestroyedImmediatelyAfterInitializationStarted_DoesNotCrash) {
  base::RunLoop run_loop;
  CreateBudgeterWithoutInitializing(/*exclusively_run_in_memory=*/false);
  InitializeBudgeter(/*on_done_initializing=*/base::DoNothing());
  DestroyBudgeter();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivateAggregationBudgeterTest,
       BudgeterDestroyedImmediatelyAfterInitialization_DoesNotCrash) {
  base::RunLoop run_loop;
  CreateBudgeterWithoutInitializing(/*exclusively_run_in_memory=*/false);
  InitializeBudgeter(
      /*on_done_initializing=*/base::BindLambdaForTesting([this, &run_loop]() {
        DestroyBudgeter();
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataBasicTest) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key,
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
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
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
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 4);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataCrossesWindowBoundary) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey example_key_1 =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey example_key_2 =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          kExampleTime + PrivateAggregationBudgetKey::TimeWindow::kDuration,
          PrivateAggregationCallerApi::kProtectedAudience);

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
      /*budget=*/(
          PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope -
          1),
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
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
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
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_2, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 5);
}

TEST_F(PrivateAggregationBudgeterTest,
       ClearDataDoesntAffectWindowsOutsideRange) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey key_to_clear =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey key_after =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          kExampleTime + PrivateAggregationBudgetKey::TimeWindow::kDuration,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey key_before =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")),
          kExampleTime - PrivateAggregationBudgetKey::TimeWindow::kDuration,
          PrivateAggregationCallerApi::kProtectedAudience);

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
      /*budget=*/(
          PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope -
          2),
      key_to_clear, expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/1, key_after, expect_approved);

  // The full budget has been used across the three time windows.
  budgeter()->ConsumeBudget(
      /*budget=*/1, key_after,
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
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
  // `(PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope -
  // 2)` that we can use.
  budgeter()->ConsumeBudget(
      /*budget=*/(
          PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope -
          2),
      key_after, expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/1, key_after,
      base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 7);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataAllApisAffected) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey protected_audience_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey shared_storage_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kSharedStorage);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      protected_audience_key, expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, protected_audience_key, expect_insufficient_budget);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
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
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      protected_audience_key, expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      shared_storage_key, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 7);
}

TEST_F(PrivateAggregationBudgeterTest, ClearAllDataBasicTest) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key,
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
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
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
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 4);
}

TEST_F(PrivateAggregationBudgeterTest, ClearAllDataNullTimes) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key,
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
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
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
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 4);
}

TEST_F(PrivateAggregationBudgeterTest, ClearAllDataNullStartNonNullEndTime) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key,
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
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
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
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key, base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        ++num_queries_processed;
      }));
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 4);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataFilterSelectsOrigins) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.example/"));

  PrivateAggregationBudgetKey example_key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginA, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey example_key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginB, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_a, expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_a, expect_insufficient_budget);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_b, expect_approved);

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
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_a, expect_approved);
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_b, expect_insufficient_budget);
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 7);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataAllTimeFilterSelectsOrigins) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.example/"));

  PrivateAggregationBudgetKey example_key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginA, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey example_key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginB, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_a, expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_a, expect_insufficient_budget);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_b, expect_approved);

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
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_a, expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_b, expect_insufficient_budget);
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 7);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataUnusedSameSiteOrigin) {
  base::HistogramTester histogram;
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  const url::Origin kOriginA =
      url::Origin::Create(GURL("https://a.domain.example/"));
  const url::Origin kOriginB =
      url::Origin::Create(GURL("https://b.domain.example/"));

  PrivateAggregationBudgetKey example_key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginA, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey example_key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginB, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_a, expect_approved);

  // Maximum budget has been used so this should fail. This should not record
  // kOriginB in the budget storage.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_b, expect_insufficient_budget);

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(
      kExampleTime, kExampleTime,
      base::BindLambdaForTesting([&](const blink::StorageKey& storage_key) {
        return storage_key == blink::StorageKey::CreateFirstParty(kOriginB);
      }),
      base::BindLambdaForTesting([&]() {
        ++num_queries_processed;
        run_loop.Quit();
      }));

  // Nothing should've been cleared as the origin was not recorded, so attempts
  // to use budget should still fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_a, expect_insufficient_budget);
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_b, expect_insufficient_budget);
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 5);
  histogram.ExpectUniqueSample(
      "PrivacySandbox.PrivateAggregation.Budgeter."
      "NumReportingOriginsStoredPerSite",
      /*sample=*/1, /*expected_bucket_count=*/4);
}

TEST_F(PrivateAggregationBudgeterTest, ClearDataSameSiteOriginUsed) {
  base::HistogramTester histogram;

  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  const url::Origin kOriginA =
      url::Origin::Create(GURL("https://a.domain.example/"));
  const url::Origin kOriginB =
      url::Origin::Create(GURL("https://b.domain.example/"));

  PrivateAggregationBudgetKey example_key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginA, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey example_key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginB, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/(
          PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope -
          1),
      example_key_a, expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_b, expect_approved);

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(
      kExampleTime, kExampleTime,
      base::BindLambdaForTesting([&](const blink::StorageKey& storage_key) {
        return storage_key == blink::StorageKey::CreateFirstParty(kOriginB);
      }),
      base::BindLambdaForTesting([&]() {
        ++num_queries_processed;
        run_loop.Quit();
      }));

  // After clearing, we can use the full budget again for the cleared site as
  // both origins were recorded.
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_a, expect_approved);
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 4);

  histogram.ExpectTotalCount(
      "PrivacySandbox.PrivateAggregation.Budgeter."
      "NumReportingOriginsStoredPerSite",
      /*expected_count=*/3);
  histogram.ExpectBucketCount(
      "PrivacySandbox.PrivateAggregation.Budgeter."
      "NumReportingOriginsStoredPerSite",
      /*sample=*/1, /*expected_count=*/1);
  histogram.ExpectBucketCount(
      "PrivacySandbox.PrivateAggregation.Budgeter."
      "NumReportingOriginsStoredPerSite",
      /*sample=*/2, /*expected_count=*/2);
}

TEST_F(PrivateAggregationBudgeterTest,
       ClearDataOriginLastUsedBeforeDeletionWindow) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  const url::Origin kOriginA =
      url::Origin::Create(GURL("https://a.domain.example/"));
  const url::Origin kOriginB =
      url::Origin::Create(GURL("https://b.domain.example/"));

  PrivateAggregationBudgetKey example_key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginA, kExampleTime - base::Minutes(5),
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey example_key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginB, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/(
          PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope -
          1),
      example_key_a, expect_approved);

  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_b, expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_b, expect_insufficient_budget);

  // `ClearData()` runs its callback after a round trip in the db task runner,
  // so its callback is invoked last.
  base::RunLoop run_loop;
  budgeter()->ClearData(
      kExampleTime - base::Minutes(4), kExampleTime,
      base::BindLambdaForTesting([&](const blink::StorageKey& storage_key) {
        return storage_key == blink::StorageKey::CreateFirstParty(kOriginA);
      }),
      base::BindLambdaForTesting([&]() {
        ++num_queries_processed;
        run_loop.Quit();
      }));

  // The clearing should have deleted nothing as kOriginA was last used before
  // the deletion window.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_b, expect_insufficient_budget);

  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 5);
}

TEST_F(PrivateAggregationBudgeterTest,
       BudgeterDestroyedImmedatelyAfterClearData_CallbackStillRun) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          url::Origin::Create(GURL("https://a.example/")), kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key,
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

TEST_F(PrivateAggregationBudgeterTest, ClearDataByDataKey) {
  int num_queries_processed = 0;

  CreateAndInitializeBudgeterThenWait();

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.example/"));

  PrivateAggregationBudgetKey example_key_a =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginA, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  PrivateAggregationBudgetKey example_key_b =
      PrivateAggregationBudgetKey::CreateForTesting(
          kOriginB, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience);

  base::RepeatingCallback<void(RequestResult)> expect_approved =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kApproved);
            ++num_queries_processed;
          });
  base::RepeatingCallback<void(RequestResult)> expect_insufficient_budget =
      base::BindLambdaForTesting(
          [&num_queries_processed](RequestResult result) {
            EXPECT_EQ(result, RequestResult::kInsufficientSmallerScopeBudget);
            ++num_queries_processed;
          });

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_a, expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_a, expect_insufficient_budget);

  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_b, expect_approved);

  // Maximum budget has been used so this should fail.
  budgeter()->ConsumeBudget(
      /*budget=*/1, example_key_b, expect_insufficient_budget);

  std::set<PrivateAggregationDataModel::DataKey> keys;
  budgeter()->GetAllDataKeys(base::BindLambdaForTesting(
      [&keys](std::set<PrivateAggregationDataModel::DataKey> data_keys) {
        keys = std::move(data_keys);
      }));
  ASSERT_THAT(keys, testing::SizeIs(2));

  base::RunLoop run_loop;
  budgeter()->DeleteByDataKey(*keys.begin(), base::BindLambdaForTesting([&]() {
    ++num_queries_processed;
    run_loop.Quit();
  }));

  // After clearing, we can use the full budget again for the cleared origin.
  // Other origins aren't affected.
  budgeter()->ConsumeBudget(
      /*budget=*/PrivateAggregationBudgeter::kSmallerScopeValues
          .max_budget_per_scope,
      example_key_a, expect_approved);
  budgeter()->ConsumeBudget(/*budget=*/1, example_key_b,
                            expect_insufficient_budget);
  run_loop.Run();
  EXPECT_EQ(num_queries_processed, 7);
}

TEST_F(PrivateAggregationBudgeterTest, StaleDataClearedOnInitialization) {
  CreateAndInitializeBudgeterThenWait();

  int64_t latest_window_start =
      PrivateAggregationBudgetKey::TimeWindow(base::Time::Now())
          .start_time()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds();

  int64_t stale_window_start =
      latest_window_start - PrivateAggregationBudgeter::kLargerScopeValues
                                .budget_scope_duration.InMicroseconds();

  AddBudgetValueAtTimestamp(CreateBudgetKey(), /*value=*/1,
                            /*timestamp=*/stale_window_start);

  EXPECT_EQ(NumberOfSitesInStorage(), 1);

  // Ensure database has a chance to persist storage.
  EnsureDbFlushes();

  DestroyBudgeter();
  CreateAndInitializeBudgeterThenWait();

  // Ensure the (zero-delay) clean up task has a chance to run.
  task_environment_.FastForwardBy(base::TimeDelta());

  // The stale data should've been cleared.
  EXPECT_EQ(NumberOfSitesInStorage(), 0);
}

TEST_F(PrivateAggregationBudgeterTest, StaleDataClearedAfterConsumeBudget) {
  CreateAndInitializeBudgeterThenWait();

  int64_t latest_window_start =
      PrivateAggregationBudgetKey::TimeWindow(base::Time::Now())
          .start_time()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds();

  int64_t stale_window_start =
      latest_window_start - PrivateAggregationBudgeter::kLargerScopeValues
                                .budget_scope_duration.InMicroseconds();

  AddBudgetValueAtTimestamp(CreateBudgetKey(), /*value=*/1,
                            /*timestamp=*/stale_window_start);

  PrivateAggregationBudgetKey non_stale_key =
      PrivateAggregationBudgetKey::CreateForTesting(
          /*origin=*/url::Origin::Create(GURL("https://b.example/")),
          /*api_invocation_time=*/kExampleTime,
          /*api=*/PrivateAggregationCallerApi::kProtectedAudience);
  base::RunLoop run_loop;
  budgeter()->ConsumeBudget(
      /*budget=*/1, non_stale_key,
      /*on_done=*/base::BindLambdaForTesting([&](RequestResult result) {
        EXPECT_EQ(result, RequestResult::kApproved);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(NumberOfSitesInStorage(), 2);

  task_environment_.FastForwardBy(
      PrivateAggregationBudgeter::kMinStaleDataCleanUpGap);

  // The stale data should've been cleared.
  EXPECT_EQ(NumberOfSitesInStorage(), 1);
}

}  // namespace

}  // namespace content

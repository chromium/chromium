// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budgeter.h"

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budget_storage.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/proto/private_aggregation_budgets.pb.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ValidityStatus = PrivateAggregationBudgeter::BudgetValidityStatus;

static constexpr PrivateAggregationCallerApi kAllApis[] = {
    PrivateAggregationCallerApi::kProtectedAudience,
    PrivateAggregationCallerApi::kSharedStorage};

int64_t SerializeTimeForStorage(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

void RecordBudgetValidity(ValidityStatus status) {
  static_assert(
      ValidityStatus::kContainsNonPositiveValue == ValidityStatus::kMaxValue,
      "Bump version of "
      "PrivacySandbox.PrivateAggregation.Budgeter."
      "BudgetValidityStatus histogram.");

  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.Budgeter.BudgetValidityStatus2",
      status);
}

void ComputeAndRecordBudgetValidity(
    google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
        budget_entries,
    int64_t earliest_window_in_larger_scope_start,
    int64_t current_window_start) {
  if (budget_entries->empty()) {
    RecordBudgetValidity(ValidityStatus::kValidAndEmpty);
    return;
  }

  constexpr int64_t kWindowDuration =
      PrivateAggregationBudgetKey::TimeWindow::kDuration.InMicroseconds();

  ValidityStatus status = ValidityStatus::kValid;

  for (proto::PrivateAggregationBudgetEntry& elem : *budget_entries) {
    int64_t entry_start = elem.entry_start_timestamp();
    int budget = elem.budget_used();

    if (budget <= 0) {
      RecordBudgetValidity(ValidityStatus::kContainsNonPositiveValue);
      return;

    } else if (entry_start % kWindowDuration != 0) {
      RecordBudgetValidity(
          ValidityStatus::kContainsTimestampNotRoundedToMinute);
      return;

    } else if (budget > PrivateAggregationBudgeter::kSmallerScopeValues
                            .max_budget_per_scope) {
      // It should not be possible for any one-minute period to have usage
      // exceeding the ten-minute limit.
      RecordBudgetValidity(ValidityStatus::kContainsValueExceedingLimit);
      return;

    } else if (entry_start >= current_window_start + kWindowDuration) {
      RecordBudgetValidity(ValidityStatus::kContainsTimestampInFuture);
      return;

    } else if (entry_start < earliest_window_in_larger_scope_start) {
      // Data older than 24 hours is no longer needed (for either scope).
      status = ValidityStatus::kValidButContainsStaleWindow;
    }
  }

  // As the budget data for both scopes is stored in the same entries, we expect
  // to maintain data for a period representing up to 24 hours in duration.
  constexpr int64_t kMaximumWindowStartDifference =
      PrivateAggregationBudgeter::kLargerScopeValues.budget_scope_duration
          .InMicroseconds() -
      kWindowDuration;
  const auto minmax = std::ranges::minmax(
      *budget_entries, /*comp=*/{},
      &proto::PrivateAggregationBudgetEntry::entry_start_timestamp);

  CHECK_EQ(kMaximumWindowStartDifference,
           current_window_start - earliest_window_in_larger_scope_start);
  if (minmax.max.entry_start_timestamp() - minmax.min.entry_start_timestamp() >
      kMaximumWindowStartDifference) {
    RecordBudgetValidity(ValidityStatus::kSpansMoreThanADay);
    return;
  }

  RecordBudgetValidity(status);
}

void RecordLockHoldDuration(base::TimeDelta lock_hold_duration) {
  base::UmaHistogramTimes(
      "PrivacySandbox.PrivateAggregation.Budgeter.LockHoldDuration",
      lock_hold_duration);
}

google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
GetBudgetEntries(PrivateAggregationCallerApi caller_api,
                 proto::PrivateAggregationBudgets& budgets) {
  switch (caller_api) {
    case PrivateAggregationCallerApi::kProtectedAudience:
      return budgets.mutable_protected_audience_budgets();
    case PrivateAggregationCallerApi::kSharedStorage:
      return budgets.mutable_shared_storage_budgets();
  }
}

// Returns whether any entries were deleted.
bool CleanUpStaleBudgetEntries(
    google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
        budget_entries,
    const int64_t earliest_window_in_larger_scope_start) {
  auto to_remove = std::ranges::remove_if(
      *budget_entries, [&earliest_window_in_larger_scope_start](
                           const proto::PrivateAggregationBudgetEntry& elem) {
        return elem.entry_start_timestamp() <
               earliest_window_in_larger_scope_start;
      });
  bool was_modified = !to_remove.empty();
  budget_entries->erase(to_remove.begin(), to_remove.end());
  return was_modified;
}

// Returns whether any entries were deleted.
bool CleanUpStaleReportingOrigins(
    google::protobuf::RepeatedPtrField<proto::ReportingOrigin>*
        reporting_origins,
    const int64_t earliest_window_in_larger_scope_start) {
  auto to_remove = std::ranges::remove_if(
      *reporting_origins, [&earliest_window_in_larger_scope_start](
                              const proto::ReportingOrigin& elem) {
        return elem.last_used_timestamp() <
               earliest_window_in_larger_scope_start;
      });
  bool was_modified = !to_remove.empty();
  reporting_origins->erase(to_remove.begin(), to_remove.end());
  return was_modified;
}

// `current_window_start` should be in microseconds since the Windows epoch,
// e.g. a value returned by `SerializeTimeForStorage()`. Returns a value in
// microseconds since the Windows epoch.
int64_t CalculateEarliestWindowStartInScope(
    int64_t current_window_start,
    base::TimeDelta budget_scope_duration) {
  return current_window_start +
         PrivateAggregationBudgetKey::TimeWindow::kDuration.InMicroseconds() -
         budget_scope_duration.InMicroseconds();
}

using RequestResult = PrivateAggregationBudgeter::RequestResult;

RequestResult TestBudgetUsageAgainstLimits(
    base::CheckedNumeric<int> total_budget_used_smaller_scope,
    base::CheckedNumeric<int> total_budget_used_larger_scope) {
  if (!total_budget_used_smaller_scope.IsValid() ||
      !total_budget_used_larger_scope.IsValid()) {
    return RequestResult::kRequestedMoreThanTotalBudget;
  }
  if (total_budget_used_smaller_scope.ValueOrDie() >
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope) {
    return RequestResult::kInsufficientSmallerScopeBudget;
  }
  if (total_budget_used_larger_scope.ValueOrDie() >
      PrivateAggregationBudgeter::kLargerScopeValues.max_budget_per_scope) {
    return RequestResult::kInsufficientLargerScopeBudget;
  }
  return PrivateAggregationBudgeter::RequestResult::kApproved;
}

void UpdateOverallResultWithNewResult(RequestResult& overall_result,
                                      RequestResult new_result) {
  // Fatal errors should have been processed before `QueryBudget()` calls this.
  CHECK_LE(new_result, RequestResult::kRequestedMoreThanTotalBudget);

  // Any budget failures override the initial/default `kApproved` state.
  if (new_result == RequestResult::kApproved) {
    return;
  } else if (overall_result == RequestResult::kApproved) {
    overall_result = new_result;
    return;
  }

  // This failure case overrides all others.
  if (new_result == RequestResult::kRequestedMoreThanTotalBudget) {
    overall_result = new_result;
  } else if (overall_result == RequestResult::kRequestedMoreThanTotalBudget) {
    return;
  }

  // At this point, each result must be either `kInsufficientSmallerScopeBudget`
  // or `kInsufficientLargerScopeBudget`. The results should agree as only one
  // case should be triggered by a query.
  CHECK_EQ(overall_result, new_result);
}

}  // namespace

PrivateAggregationBudgeter::BudgetQueryResult::BudgetQueryResult(
    RequestResult overall_result,
    std::vector<ResultForContribution> result_for_each_contribution)
    : overall_result(overall_result),
      result_for_each_contribution(std::move(result_for_each_contribution)) {}

PrivateAggregationBudgeter::BudgetQueryResult::BudgetQueryResult(
    BudgetQueryResult&& other) = default;
PrivateAggregationBudgeter::BudgetQueryResult&
PrivateAggregationBudgeter::BudgetQueryResult::operator=(
    BudgetQueryResult&& other) = default;
PrivateAggregationBudgeter::BudgetQueryResult::~BudgetQueryResult() = default;

PrivateAggregationBudgeter::InspectBudgetCallResult::InspectBudgetCallResult(
    BudgetQueryResult query_result,
    std::optional<Lock> lock,
    PendingReportLimitResult pending_report_limit_result)
    : query_result(std::move(query_result)),
      lock(std::move(lock)),
      pending_report_limit_result(pending_report_limit_result) {}

PrivateAggregationBudgeter::InspectBudgetCallResult::InspectBudgetCallResult(
    InspectBudgetCallResult&& other) = default;
PrivateAggregationBudgeter::InspectBudgetCallResult&
PrivateAggregationBudgeter::InspectBudgetCallResult::operator=(
    InspectBudgetCallResult&& other) = default;
PrivateAggregationBudgeter::InspectBudgetCallResult::
    ~InspectBudgetCallResult() = default;

PrivateAggregationBudgeter::PrivateAggregationBudgeter(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner,
    bool exclusively_run_in_memory,
    base::FilePath path_to_db_dir)
    : db_task_runner_(std::move(db_task_runner)),
      exclusively_run_in_memory_(exclusively_run_in_memory),
      path_to_db_dir_(std::move(path_to_db_dir)) {
  CHECK(db_task_runner_);
}

PrivateAggregationBudgeter::PrivateAggregationBudgeter() = default;

PrivateAggregationBudgeter::~PrivateAggregationBudgeter() {
  if (shutdown_initializing_storage_) {
    // As the budget storage's lifetime is extended until initialization is
    // complete, its destructor could run after browser shutdown has begun (when
    // tasks can no longer be posted). We post the database deletion task now
    // instead.
    std::move(shutdown_initializing_storage_).Run();
  }
  if (IsBudgeterLocked()) {
    RecordLockHoldDuration(locked_timer_->Elapsed());
  }
}

void PrivateAggregationBudgeter::EnsureStorageInitializationBegun() {
  if (storage_status_ != StorageStatus::kPendingInitialization) {
    return;
  }
  storage_status_ = StorageStatus::kInitializing;

  shutdown_initializing_storage_ = PrivateAggregationBudgetStorage::CreateAsync(
      db_task_runner_, exclusively_run_in_memory_,
      // This move is safe because storage will only be initialized once.
      std::move(path_to_db_dir_),
      /*on_done_initializing=*/
      base::BindOnce(&PrivateAggregationBudgeter::OnStorageDoneInitializing,
                     weak_factory_.GetWeakPtr()));
}

void PrivateAggregationBudgeter::ConsumeBudget(
    int budget,
    const PrivateAggregationBudgetKey& budget_key,
    base::OnceCallback<void(RequestResult)> on_done) {
  ConsumeBudget(budget, budget_key, /*minimum_value_for_metrics=*/0,
                std::move(on_done));
}

void PrivateAggregationBudgeter::ConsumeBudget(
    int budget,
    const PrivateAggregationBudgetKey& budget_key,
    int minimum_value_for_metrics,
    base::OnceCallback<void(RequestResult)> on_done) {
  CHECK(!base::FeatureList::IsEnabled(
      blink::features::kPrivateAggregationApiErrorReporting));

  // The budgeter can't be locked unless the error reporting feature is enabled.
  CHECK(!IsBudgeterLocked());
  EnsureStorageInitializationBegun();

  if (storage_status_ == StorageStatus::kInitializing) {
    if (pending_calls_.size() >= kMaxPendingCalls) {
      std::move(on_done).Run(RequestResult::kTooManyPendingCalls);
      return;
    }

    // `base::Unretained` is safe as `pending_calls_` is owned by `this`.
    pending_calls_.push_back(base::BindOnce(
        &PrivateAggregationBudgeter::ConsumeBudgetImpl, base::Unretained(this),
        budget, budget_key, minimum_value_for_metrics, std::move(on_done)));
  } else {
    ConsumeBudgetImpl(budget, budget_key, minimum_value_for_metrics,
                      std::move(on_done));
  }
}

bool PrivateAggregationBudgeter::IsBudgeterLocked() const {
  return locked_timer_.has_value();
}

void PrivateAggregationBudgeter::InspectBudgetAndLock(
    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
        contributions,
    const PrivateAggregationBudgetKey& budget_key,
    base::OnceCallback<void(InspectBudgetCallResult)> result_callback) {
  CHECK(base::FeatureList::IsEnabled(
      blink::features::kPrivateAggregationApiErrorReporting));
  EnsureStorageInitializationBegun();

  if (storage_status_ == StorageStatus::kInitializing || IsBudgeterLocked()) {
    if (pending_calls_.size() >= kMaxPendingCalls) {
      std::move(result_callback)
          .Run(InspectBudgetCallResult(
              {RequestResult::kTooManyPendingCalls, {}}, std::nullopt,
              PendingReportLimitResult::kAtLimit));
      return;
    }

    // Determines whether this call will cause the limit to be reached.
    PendingReportLimitResult pending_report_limit_result =
        (pending_calls_.size() + 1 == kMaxPendingCalls)
            ? PendingReportLimitResult::kAtLimit
            : PendingReportLimitResult::kNotAtLimit;

    // `base::Unretained` is safe as `pending_calls_` is owned by `this`.
    // TODO(crbug.com/405772004): Consider ways to avoid copies of
    // `contributions` and `budget_key`, especially if the storage layer is
    // changed to be always asynchronous.
    pending_calls_.push_back(base::BindOnce(
        &PrivateAggregationBudgeter::InspectBudgetAndLockImpl,
        base::Unretained(this), contributions, budget_key,
        pending_report_limit_result, std::move(result_callback)));
  } else {
    InspectBudgetAndLockImpl(contributions, budget_key,
                             PendingReportLimitResult::kNotAtLimit,
                             std::move(result_callback));
  }
}

void PrivateAggregationBudgeter::ConsumeBudget(
    Lock lock,
    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
        contributions,
    const PrivateAggregationBudgetKey& budget_key,
    base::OnceCallback<void(BudgetQueryResult)> result_callback) {
  CHECK(base::FeatureList::IsEnabled(
      blink::features::kPrivateAggregationApiErrorReporting));

  // If a lock was vended, initialization must be complete.
  CHECK(DidStorageInitializationSucceed());
  CHECK(IsBudgeterLocked());
  RecordLockHoldDuration(locked_timer_->Elapsed());

  BudgetQueryResult query_result =
      QueryBudget(contributions, budget_key, /*consume_budget=*/true);

  std::move(result_callback).Run(std::move(query_result));

  locked_timer_ = std::nullopt;  // Unlocks budgeter.

  ProcessAllPendingCalls();
}

void PrivateAggregationBudgeter::InspectBudgetAndLockImpl(
    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
        contributions,
    const PrivateAggregationBudgetKey& budget_key,
    PendingReportLimitResult pending_report_limit_result,
    base::OnceCallback<void(InspectBudgetCallResult)> result_callback) {
  if (!DidStorageInitializationSucceed()) {
    std::move(result_callback)
        .Run({{RequestResult::kStorageInitializationFailed, {}},
              std::nullopt,
              pending_report_limit_result});
    return;
  }

  BudgetQueryResult query_results =
      QueryBudget(contributions, budget_key, /*consume_budget=*/false);

  // This is the only fatal error that can occur from `QueryBudget()`.
  if (query_results.overall_result == RequestResult::kBadValuesOnDisk) {
    std::move(result_callback)
        .Run({std::move(query_results), std::nullopt,
              pending_report_limit_result});
    return;
  }

  std::move(result_callback)
      .Run(InspectBudgetCallResult(std::move(query_results), VendLock(),
                                   pending_report_limit_result));
}

PrivateAggregationBudgeter::Lock PrivateAggregationBudgeter::VendLock() {
  CHECK(!IsBudgeterLocked());

  locked_timer_ = base::ElapsedTimer();
  return Lock();
}

void PrivateAggregationBudgeter::OnUserVisibleTaskStarted() {
  // When a user visible task is queued or running, we use a higher priority.
  // We do this even if the storage hasn't finished initializing.
  ++num_pending_user_visible_tasks_;
  db_task_runner_->UpdatePriority(base::TaskPriority::USER_VISIBLE);
}

void PrivateAggregationBudgeter::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    base::OnceClosure done) {
  OnUserVisibleTaskStarted();

  EnsureStorageInitializationBegun();

  done = base::BindOnce(&PrivateAggregationBudgeter::OnUserVisibleTaskComplete,
                        weak_factory_.GetWeakPtr())
             .Then(std::move(done));

  if (storage_status_ == StorageStatus::kInitializing || IsBudgeterLocked()) {
    // To ensure that data deletion always succeeds, we don't check
    // `pending_calls.size()` here.

    // `base::Unretained` is safe as `pending_calls_` is owned by `this`.
    pending_calls_.push_back(base::BindOnce(
        &PrivateAggregationBudgeter::ClearDataImpl, base::Unretained(this),
        delete_begin, delete_end, std::move(filter), std::move(done)));
  } else {
    ClearDataImpl(delete_begin, delete_end, std::move(filter), std::move(done));
  }
}

void PrivateAggregationBudgeter::OnUserVisibleTaskComplete() {
  CHECK_GT(num_pending_user_visible_tasks_, 0);
  --num_pending_user_visible_tasks_;

  // No more pending tasks, so we can reset the priority.
  if (num_pending_user_visible_tasks_ == 0) {
    db_task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);
  }
}

void PrivateAggregationBudgeter::OnStorageDoneInitializing(
    std::unique_ptr<PrivateAggregationBudgetStorage> storage) {
  CHECK(shutdown_initializing_storage_);
  CHECK(!storage_);
  CHECK_EQ(storage_status_, StorageStatus::kInitializing);

  if (storage) {
    storage_status_ = StorageStatus::kOpen;
    storage_ = std::move(storage);
  } else {
    storage_status_ = StorageStatus::kInitializationFailed;
  }
  shutdown_initializing_storage_.Reset();

  ProcessAllPendingCalls();

  // No-op if storage initialization failed.
  CleanUpStaleDataSoon();
}

void PrivateAggregationBudgeter::ProcessAllPendingCalls() {
  CHECK(!IsBudgeterLocked());

  // Avoid recursion.
  if (process_all_pending_calls_in_progress_) {
    return;
  }
  base::AutoReset<bool> auto_reset(&process_all_pending_calls_in_progress_,
                                   true);

  // Avoid a simple for loop in case the vector is mutated during the processing
  // of one of the pending calls or its callback.
  while (!pending_calls_.empty()) {
    base::OnceClosure cb = std::move(pending_calls_.front());
    pending_calls_.erase(pending_calls_.begin());

    std::move(cb).Run();
    // Executing an earlier pending `InspectBudgetAndLock()` call may have
    // locked the budgeter. In this case, `ProcessAllPendingCalls()` will be run
    // again after the corresponding `ConsumeBudget()` call.
    if (IsBudgeterLocked()) {
      return;
    }
  }
}

void PrivateAggregationBudgeter::GetAllDataKeys(
    base::OnceCallback<void(std::set<PrivateAggregationDataModel::DataKey>)>
        callback) {
  OnUserVisibleTaskStarted();

  EnsureStorageInitializationBegun();

  base::OnceCallback<void(std::set<PrivateAggregationDataModel::DataKey>)>
      impl_callback = std::move(callback).Then(
          base::BindOnce(&PrivateAggregationBudgeter::OnUserVisibleTaskComplete,
                         weak_factory_.GetWeakPtr()));

  if (storage_status_ == StorageStatus::kInitializing) {
    // `base::Unretained` is safe as `pending_calls_` is owned by `this`.
    pending_calls_.push_back(
        base::BindOnce(&PrivateAggregationBudgeter::GetAllDataKeysImpl,
                       base::Unretained(this), std::move(impl_callback)));
  } else {
    return GetAllDataKeysImpl(std::move(impl_callback));
  }
}

void PrivateAggregationBudgeter::GetAllDataKeysImpl(
    base::OnceCallback<void(std::set<PrivateAggregationDataModel::DataKey>)>
        callback) {
  if (!DidStorageInitializationSucceed()) {
    std::move(callback).Run(std::set<PrivateAggregationDataModel::DataKey>());
    return;
  }

  std::set<PrivateAggregationDataModel::DataKey> keys;
  for (const auto& [site_key, budgets] :
       storage_->budgets_data()->GetAllCached()) {
    for (const proto::ReportingOrigin& elem :
         budgets.reporting_origins_for_deletion()) {
      url::Origin reporting_origin = url::Origin::Create(GURL(elem.origin()));
      if (reporting_origin.opaque()) {
        continue;
      }
      keys.emplace(std::move(reporting_origin));
    }
  }
  std::move(callback).Run(std::move(keys));
}

void PrivateAggregationBudgeter::DeleteByDataKey(
    const PrivateAggregationDataModel::DataKey& key,
    base::OnceClosure callback) {
  ClearData(/*delete_begin=*/base::Time::Min(),
            /*delete_end=*/base::Time::Max(),
            /*filter=*/
            base::BindRepeating(
                std::equal_to<blink::StorageKey>(),
                blink::StorageKey::CreateFirstParty(key.reporting_origin())),
            std::move(callback));
}

// TODO(crbug.com/40229006): Consider enumerating different error cases and log
// metrics and/or expose to callers.
void PrivateAggregationBudgeter::ConsumeBudgetImpl(
    int additional_budget,
    const PrivateAggregationBudgetKey& budget_key,
    int minimum_value_for_metrics,
    base::OnceCallback<void(RequestResult)> on_done) {
  CHECK_GT(additional_budget, 0);

  if (!DidStorageInitializationSucceed()) {
    std::move(on_done).Run(RequestResult::kStorageInitializationFailed);
    return;
  }

  // This hack is used to avoid needing to copy all the values to a new vector
  // when `kPrivateAggregationApiErrorReporting` is enabled.
  // TODO(crbug.com/381788013): Remove this entire flow when the feature flag is
  // removed after being fully launched.
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      fake_contribution_vector{
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/0, /*value=*/int32_t{additional_budget},
              /*filtering_id=*/0)};

  BudgetQueryResult query_result =
      QueryBudget(fake_contribution_vector, budget_key, /*consume_budget=*/true,
                  minimum_value_for_metrics);

  // We ignore the per-contribution result as provides no extra value given the
  // contributions' values were summed before querying.
  std::move(on_done).Run(query_result.overall_result);
}

PrivateAggregationBudgeter::BudgetQueryResult
PrivateAggregationBudgeter::QueryBudget(
    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
        contributions,
    const PrivateAggregationBudgetKey& budget_key,
    bool consume_budget,
    int minimum_value_for_metrics) {
  if (base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiErrorReporting)) {
    // This argument should only be set for the non-error reporting flow.
    CHECK_EQ(minimum_value_for_metrics, 0);
  }

  static_assert(
      kSmallerScopeValues.max_budget_per_scope <
          kLargerScopeValues.max_budget_per_scope,
      "The larger scope must have a larger budget than the smaller scope.");

  if (contributions.empty()) {
    return {RequestResult::kApproved, {}};
  }

  RequestResult overall_result = RequestResult::kApproved;

  // Note: when `kPrivateAggregationApiErrorReporting` is disabled, a vector
  // with one element is passed in, so the `CheckedNumeric` is superfluous.
  base::CheckedNumeric<int> total_budget_needed = std::accumulate(
      contributions.begin(), contributions.end(),
      /*init=*/base::CheckedNumeric<int>(0), /*op=*/
      [](base::CheckedNumeric<int> running_sum,
         const blink::mojom::AggregatableReportHistogramContribution&
             contribution) { return running_sum + contribution.value; });
  if (!total_budget_needed.IsValid() ||
      total_budget_needed.ValueOrDie() >
          kSmallerScopeValues.max_budget_per_scope) {
    UpdateOverallResultWithNewResult(
        overall_result, RequestResult::kRequestedMoreThanTotalBudget);

    if (!base::FeatureList::IsEnabled(
            blink::features::kPrivateAggregationApiErrorReporting)) {
      // We early return as only the overall result will be used.
      CHECK_EQ(contributions.size(), 1u);

      return {overall_result, {}};
    }
  }

  std::string site_key = net::SchemefulSite(budget_key.origin()).Serialize();

  // If there is no budget proto stored for this origin already, we use the
  // default initialization of `budgets` (untouched by `TryGetData()`).
  // TODO(crbug.com/381788013): Consider caching this proto between calls to
  // `InspectBudgetAndLock()` and `ConsumeBudget()`.
  proto::PrivateAggregationBudgets budgets;
  storage_->budgets_data()->TryGetData(site_key, &budgets);

  const int64_t current_window_start =
      SerializeTimeForStorage(budget_key.time_window().start_time());
  CHECK_EQ(current_window_start % base::Time::kMicrosecondsPerMinute, 0);

  // Budget windows must start on or after this timestamp to be counted in the
  // current 10 minutes and day (for the smaller and larger scopes,
  // respectively).
  int64_t earliest_window_in_smaller_scope_start =
      CalculateEarliestWindowStartInScope(
          current_window_start, kSmallerScopeValues.budget_scope_duration);
  int64_t earliest_window_in_larger_scope_start =
      CalculateEarliestWindowStartInScope(
          current_window_start, kLargerScopeValues.budget_scope_duration);

  google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
      budget_entries = GetBudgetEntries(budget_key.caller_api(), budgets);

  ComputeAndRecordBudgetValidity(budget_entries,
                                 earliest_window_in_larger_scope_start,
                                 current_window_start);

  proto::PrivateAggregationBudgetEntry* window_for_key = nullptr;
  base::CheckedNumeric<int> total_budget_used_smaller_scope = 0;
  base::CheckedNumeric<int> total_budget_used_larger_scope = 0;

  for (proto::PrivateAggregationBudgetEntry& elem : *budget_entries) {
    if (elem.entry_start_timestamp() < earliest_window_in_larger_scope_start) {
      continue;
    }
    if (elem.entry_start_timestamp() == current_window_start) {
      window_for_key = &elem;
    }

    // Protect against bad values on disk
    if (elem.budget_used() <= 0) {
      return {RequestResult::kBadValuesOnDisk, {}};
    }

    if (elem.entry_start_timestamp() >=
        earliest_window_in_smaller_scope_start) {
      total_budget_used_smaller_scope += elem.budget_used();
    }
    total_budget_used_larger_scope += elem.budget_used();
  }

  if (TestBudgetUsageAgainstLimits(total_budget_used_smaller_scope,
                                   total_budget_used_larger_scope) !=
      RequestResult::kApproved) {
    return {RequestResult::kBadValuesOnDisk, {}};
  }

  base::CheckedNumeric<int> additional_budget = 0;

  // Note: when `kPrivateAggregationApiErrorReporting` is disabled,
  // `request_results` will be equal to `{overall_result}`.
  std::vector<ResultForContribution> request_results;
  request_results.reserve(contributions.size());

  std::ranges::transform(contributions, std::back_inserter(request_results),
                         [&](auto& contribution) {
                           CHECK_GE(contribution.value, 0);

                           RequestResult budget_increase_request_result =
                               TestBudgetUsageAgainstLimits(
                                   total_budget_used_smaller_scope +
                                       additional_budget + contribution.value,
                                   total_budget_used_larger_scope +
                                       additional_budget + contribution.value);

                           bool was_approved = budget_increase_request_result ==
                                               RequestResult::kApproved;
                           if (was_approved) {
                             additional_budget += contribution.value;

                             // The budget test would've failed if the sum
                             // exceeds limits.
                             CHECK(additional_budget.IsValid());
                           }
                           UpdateOverallResultWithNewResult(
                               overall_result, budget_increase_request_result);
                           return was_approved
                                      ? ResultForContribution::kApproved
                                      : ResultForContribution::kDenied;
                         });

  if (!consume_budget) {
    return {overall_result, std::move(request_results)};
  }

  if (additional_budget.ValueOrDie() > 0) {
    if (!window_for_key) {
      window_for_key = budget_entries->Add();
      window_for_key->set_entry_start_timestamp(current_window_start);
      window_for_key->set_budget_used(0);
    }
    int budget_used_for_key =
        (window_for_key->budget_used() + additional_budget).ValueOrDie();
    CHECK_GT(budget_used_for_key, 0);
    CHECK_LE(budget_used_for_key, kSmallerScopeValues.max_budget_per_scope);
    window_for_key->set_budget_used(budget_used_for_key);
  }

  google::protobuf::RepeatedPtrField<proto::ReportingOrigin>*
      reporting_origins_for_deletion =
          budgets.mutable_reporting_origins_for_deletion();

  if (additional_budget.ValueOrDie() > 0) {
    std::string reporting_origin_serialized = budget_key.origin().Serialize();
    proto::ReportingOrigin* reporting_origin_entry = nullptr;

    auto reporting_origin_entry_it = std::ranges::find_if(
        *reporting_origins_for_deletion,
        [&reporting_origin_serialized](const proto::ReportingOrigin& elem) {
          return elem.origin() == reporting_origin_serialized;
        });
    if (reporting_origin_entry_it != reporting_origins_for_deletion->end()) {
      reporting_origin_entry = &*reporting_origin_entry_it;
    }

    if (!reporting_origin_entry) {
      reporting_origin_entry = reporting_origins_for_deletion->Add();
      reporting_origin_entry->set_origin(
          std::move(reporting_origin_serialized));
    }
    reporting_origin_entry->set_last_used_timestamp(current_window_start);
  }

  if (additional_budget.ValueOrDie() == 0 &&
      !base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiErrorReporting)) {
    bool would_minimum_value_be_approved =
        TestBudgetUsageAgainstLimits(
            total_budget_used_smaller_scope + minimum_value_for_metrics,
            total_budget_used_larger_scope + minimum_value_for_metrics) ==
        PrivateAggregationBudgeter::RequestResult::kApproved;
    base::UmaHistogramBoolean(
        "PrivacySandbox.PrivateAggregation.Budgeter."
        "EnoughBudgetForAnyValueIfNotEnoughOverall",
        would_minimum_value_be_approved);
  }

  base::UmaHistogramCounts100(
      "PrivacySandbox.PrivateAggregation.Budgeter."
      "NumReportingOriginsStoredPerSite",
      reporting_origins_for_deletion->size());

  storage_->budgets_data()->UpdateData(site_key, budgets);

  CleanUpStaleDataSoon();

  return {overall_result, std::move(request_results)};
}

void PrivateAggregationBudgeter::ClearDataImpl(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    base::OnceClosure done) {
  if (!DidStorageInitializationSucceed()) {
    std::move(done).Run();
    return;
  }

  // Treat null times as unbounded lower or upper range. This is used by
  // browsing data remover.
  if (delete_begin.is_null()) {
    delete_begin = base::Time::Min();
  }

  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }

  bool is_all_time_covered = delete_begin.is_min() && delete_end.is_max();

  if (is_all_time_covered && filter.is_null()) {
    storage_->budgets_data()->DeleteAllData();

    // Runs `done` once flushing is complete.
    storage_->budgets_data()->FlushDataToDisk(std::move(done));
    return;
  }

  // Ensure we round down to capture any time windows that partially overlap.
  const int64_t serialized_delete_begin = SerializeTimeForStorage(
      PrivateAggregationBudgetKey::TimeWindow(delete_begin).start_time());

  // No need to round up as we compare against the time window's start time.
  const int64_t serialized_delete_end = SerializeTimeForStorage(delete_end);

  std::vector<std::string> sites_to_delete;

  for (const auto& [site_key, budgets] :
       storage_->budgets_data()->GetAllCached()) {
    for (const proto::ReportingOrigin& elem :
         budgets.reporting_origins_for_deletion()) {
      // If the filter matches the origin and the origin was last used on or
      // after the beginning of the deletion window, we include this site.
      // This may result in more data being deleted than strictly necessary.
      if ((filter.is_null() ||
           filter.Run(blink::StorageKey::CreateFirstParty(
               url::Origin::Create(GURL(elem.origin()))))) &&
          (is_all_time_covered ||
           serialized_delete_begin <= elem.last_used_timestamp())) {
        sites_to_delete.push_back(site_key);
        break;
      }
    }
  }

  if (is_all_time_covered) {
    storage_->budgets_data()->DeleteData(sites_to_delete);

    // Runs `done` once flushing is complete.
    storage_->budgets_data()->FlushDataToDisk(std::move(done));
    return;
  }

  const int64_t earliest_window_in_larger_scope_start =
      CalculateEarliestWindowStartInScope(
          /*current_window_start=*/SerializeTimeForStorage(
              PrivateAggregationBudgetKey::TimeWindow(base::Time::Now())
                  .start_time()),
          kLargerScopeValues.budget_scope_duration);

  for (const std::string& site_key : sites_to_delete) {
    proto::PrivateAggregationBudgets budgets;
    storage_->budgets_data()->TryGetData(site_key, &budgets);

    for (PrivateAggregationCallerApi caller_api : kAllApis) {
      google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
          budget_entries = GetBudgetEntries(caller_api, budgets);
      CHECK(budget_entries);

      auto to_remove = std::ranges::remove_if(
          *budget_entries,
          [=](const proto::PrivateAggregationBudgetEntry& elem) {
            return elem.entry_start_timestamp() >= serialized_delete_begin &&
                   elem.entry_start_timestamp() <= serialized_delete_end;
          });
      budget_entries->erase(to_remove.begin(), to_remove.end());

      CleanUpStaleBudgetEntries(budget_entries,
                                earliest_window_in_larger_scope_start);
    }

    for (proto::ReportingOrigin& elem :
         *budgets.mutable_reporting_origins_for_deletion()) {
      // If the last used time is in the deletion window, we update it to the
      // start of the window. We do this even for reporting origins that don't
      // match the filter as the whole site's data was deleted. This may result
      // in more data being deleted than strictly necessary.
      if (elem.last_used_timestamp() >= serialized_delete_begin &&
          elem.last_used_timestamp() <= serialized_delete_end) {
        elem.set_last_used_timestamp(serialized_delete_begin);
      }
    }

    CleanUpStaleReportingOrigins(
        budgets.mutable_reporting_origins_for_deletion(),
        earliest_window_in_larger_scope_start);

    storage_->budgets_data()->UpdateData(site_key, budgets);
  }

  // Force the database to be flushed immediately instead of waiting up to
  // `PrivateAggregationBudgetStorage::kFlushDelay`. Runs the `done` callback
  // once flushing is complete.
  storage_->budgets_data()->FlushDataToDisk(std::move(done));
}

void PrivateAggregationBudgeter::CleanUpStaleDataSoon() {
  if (!DidStorageInitializationSucceed()) {
    return;
  }

  if (clean_up_stale_data_timer_.IsRunning()) {
    return;
  }

  // Wait for `kMinStaleDataCleanUpGap` to pass between invocations.
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks earliest_allowed_clean_up_time =
      last_clean_up_time_ + kMinStaleDataCleanUpGap;

  // If enough time has already passed, post a zero-delay task as it does not
  // need to be invoked synchronously.
  base::TimeDelta wait_time =
      std::max(earliest_allowed_clean_up_time - now, base::TimeDelta());

  clean_up_stale_data_timer_.Start(
      FROM_HERE, wait_time,
      base::BindOnce(&PrivateAggregationBudgeter::CleanUpStaleData,
                     weak_factory_.GetWeakPtr()));
}

void PrivateAggregationBudgeter::CleanUpStaleData() {
  CHECK(DidStorageInitializationSucceed());

  last_clean_up_time_ = base::TimeTicks::Now();

  std::vector<std::string> all_sites;

  for (const auto& [site_key, budgets] :
       storage_->budgets_data()->GetAllCached()) {
    all_sites.push_back(site_key);
  }

  const int64_t earliest_non_stale_window_start =
      CalculateEarliestWindowStartInScope(
          /*current_window_start=*/SerializeTimeForStorage(
              PrivateAggregationBudgetKey::TimeWindow(base::Time::Now())
                  .start_time()),
          kLargerScopeValues.budget_scope_duration);

  for (const std::string& site_key : all_sites) {
    proto::PrivateAggregationBudgets budgets;
    bool success = storage_->budgets_data()->TryGetData(site_key, &budgets);
    CHECK(success);

    bool was_modified = false;

    for (PrivateAggregationCallerApi caller_api : kAllApis) {
      google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
          budget_entries = GetBudgetEntries(caller_api, budgets);
      CHECK(budget_entries);

      was_modified |= CleanUpStaleBudgetEntries(
          budget_entries, earliest_non_stale_window_start);
    }

    google::protobuf::RepeatedPtrField<proto::ReportingOrigin>*
        reporting_origins_for_deletion =
            budgets.mutable_reporting_origins_for_deletion();

    was_modified |= CleanUpStaleReportingOrigins(
        reporting_origins_for_deletion, earliest_non_stale_window_start);

    if (!was_modified) {
      continue;
    }

    bool is_entry_empty = budgets.protected_audience_budgets().empty() &&
                          budgets.shared_storage_budgets().empty() &&
                          budgets.reporting_origins_for_deletion().empty();
    if (is_entry_empty) {
      storage_->budgets_data()->DeleteData({site_key});
    } else {
      storage_->budgets_data()->UpdateData(site_key, budgets);
    }
  }
}

bool PrivateAggregationBudgeter::DidStorageInitializationSucceed() const {
  switch (storage_status_) {
    case StorageStatus::kPendingInitialization:
    case StorageStatus::kInitializing:
      NOTREACHED();
    case StorageStatus::kInitializationFailed:
      return false;
    case StorageStatus::kOpen:
      return true;
  }
}

// LINT.IfChange(ComputeOverallRequestResult)
RequestResult PrivateAggregationBudgeter::CombineRequestResults(
    RequestResult inspect_budget_result,
    RequestResult consume_budget_result) {
  // We can combine the results by simply taking the maximum as any fatal error
  // can only occur once (and should be the final result) and any insufficient
  // budget value should override a `kApproved` result.
  return std::max(inspect_budget_result, consume_budget_result);
}
// LINT.ThenChange(//content/browser/private_aggregation/private_aggregation_budgeter.h:RequestResult)

}  // namespace content

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budgeter.h"

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budget_storage.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/proto/private_aggregation_budgets.pb.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
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
  const auto minmax = base::ranges::minmax(
      *budget_entries, /*comp=*/{},
      &proto::PrivateAggregationBudgetEntry::entry_start_timestamp);

  CHECK_EQ(kMaximumWindowStartDifference,
           current_window_start - earliest_window_in_larger_scope_start);
  if (minmax.second.entry_start_timestamp() -
          minmax.first.entry_start_timestamp() >
      kMaximumWindowStartDifference) {
    RecordBudgetValidity(ValidityStatus::kSpansMoreThanADay);
    return;
  }

  RecordBudgetValidity(status);
}

google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
GetBudgetEntries(PrivateAggregationCallerApi api,
                 proto::PrivateAggregationBudgets& budgets) {
  switch (api) {
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
  auto new_end = base::ranges::remove_if(
      *budget_entries, [&earliest_window_in_larger_scope_start](
                           const proto::PrivateAggregationBudgetEntry& elem) {
        return elem.entry_start_timestamp() <
               earliest_window_in_larger_scope_start;
      });
  bool was_modified = new_end != budget_entries->end();
  budget_entries->erase(new_end, budget_entries->end());
  return was_modified;
}

// Returns whether any entries were deleted.
bool CleanUpStaleReportingOrigins(
    google::protobuf::RepeatedPtrField<proto::ReportingOrigin>*
        reporting_origins,
    const int64_t earliest_window_in_larger_scope_start) {
  auto new_end = base::ranges::remove_if(
      *reporting_origins, [&earliest_window_in_larger_scope_start](
                              const proto::ReportingOrigin& elem) {
        return elem.last_used_timestamp() <
               earliest_window_in_larger_scope_start;
      });
  bool was_modified = new_end != reporting_origins->end();
  reporting_origins->erase(new_end, reporting_origins->end());
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

PrivateAggregationBudgeter::RequestResult TestBudgetUsageAgainstLimits(
    base::CheckedNumeric<int> total_budget_used_smaller_scope,
    base::CheckedNumeric<int> total_budget_used_larger_scope) {
  if (!total_budget_used_smaller_scope.IsValid() ||
      !total_budget_used_larger_scope.IsValid()) {
    return PrivateAggregationBudgeter::RequestResult::kBadValuesOnDisk;
  }
  if (total_budget_used_smaller_scope.ValueOrDie() >
      PrivateAggregationBudgeter::kSmallerScopeValues.max_budget_per_scope) {
    return PrivateAggregationBudgeter::RequestResult::
        kInsufficientSmallerScopeBudget;
  }
  if (total_budget_used_larger_scope.ValueOrDie() >
      PrivateAggregationBudgeter::kLargerScopeValues.max_budget_per_scope) {
    return PrivateAggregationBudgeter::RequestResult::
        kInsufficientLargerScopeBudget;
  }
  return PrivateAggregationBudgeter::RequestResult::kApproved;
}

}  // namespace

PrivateAggregationBudgeter::PrivateAggregationBudgeter(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner,
    bool exclusively_run_in_memory,
    const base::FilePath& path_to_db_dir)
    : db_task_runner_(std::move(db_task_runner)) {
  CHECK(db_task_runner_);

  initialize_storage_ = base::BindOnce(
      &PrivateAggregationBudgeter::InitializeStorage,
      weak_factory_.GetWeakPtr(), exclusively_run_in_memory, path_to_db_dir);
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
}

void PrivateAggregationBudgeter::EnsureStorageInitializationBegun() {
  if (storage_status_ == StorageStatus::kPendingInitialization) {
    CHECK(initialize_storage_);
    std::move(initialize_storage_).Run();
  }
}

void PrivateAggregationBudgeter::InitializeStorage(
    bool exclusively_run_in_memory,
    base::FilePath path_to_db_dir) {
  CHECK_EQ(storage_status_, StorageStatus::kPendingInitialization);

  storage_status_ = StorageStatus::kInitializing;
  shutdown_initializing_storage_ = PrivateAggregationBudgetStorage::CreateAsync(
      db_task_runner_, exclusively_run_in_memory, std::move(path_to_db_dir),
      /*on_done_initializing=*/
      base::BindOnce(&PrivateAggregationBudgeter::OnStorageDoneInitializing,
                     weak_factory_.GetWeakPtr()));
  CHECK(initialize_storage_.is_null());
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

  if (storage_status_ == StorageStatus::kInitializing) {
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
  for (base::OnceClosure& cb : pending_calls_) {
    std::move(cb).Run();
  }
  pending_calls_.clear();
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

  static_assert(
      kSmallerScopeValues.max_budget_per_scope <
          kLargerScopeValues.max_budget_per_scope,
      "The larger scope must have a larger budget than the smaller scope.");
  if (additional_budget > kSmallerScopeValues.max_budget_per_scope) {
    std::move(on_done).Run(RequestResult::kRequestedMoreThanTotalBudget);
    return;
  }

  std::string site_key = net::SchemefulSite(budget_key.origin()).Serialize();

  // If there is no budget proto stored for this origin already, we use the
  // default initialization of `budgets` (untouched by `TryGetData()`).
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
      budget_entries = GetBudgetEntries(budget_key.api(), budgets);

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
      std::move(on_done).Run(RequestResult::kBadValuesOnDisk);
      return;
    }

    if (elem.entry_start_timestamp() >=
        earliest_window_in_smaller_scope_start) {
      total_budget_used_smaller_scope += elem.budget_used();
    }
    total_budget_used_larger_scope += elem.budget_used();
  }

  RequestResult budget_increase_request_result = TestBudgetUsageAgainstLimits(
      total_budget_used_smaller_scope + additional_budget,
      total_budget_used_larger_scope + additional_budget);

  if (budget_increase_request_result == RequestResult::kApproved) {
    if (!window_for_key) {
      window_for_key = budget_entries->Add();
      window_for_key->set_entry_start_timestamp(current_window_start);
      window_for_key->set_budget_used(0);
    }
    int budget_used_for_key = window_for_key->budget_used() + additional_budget;
    CHECK_GT(budget_used_for_key, 0);
    CHECK_LE(budget_used_for_key, kSmallerScopeValues.max_budget_per_scope);
    window_for_key->set_budget_used(budget_used_for_key);
  }

  google::protobuf::RepeatedPtrField<proto::ReportingOrigin>*
      reporting_origins_for_deletion =
          budgets.mutable_reporting_origins_for_deletion();

  if (budget_increase_request_result == RequestResult::kApproved) {
    std::string reporting_origin_serialized = budget_key.origin().Serialize();
    proto::ReportingOrigin* reporting_origin_entry = nullptr;

    auto reporting_origin_entry_it = base::ranges::find_if(
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

  if (budget_increase_request_result != RequestResult::kApproved) {
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
  std::move(on_done).Run(budget_increase_request_result);

  CleanUpStaleDataSoon();
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

    for (PrivateAggregationCallerApi api : kAllApis) {
      google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
          budget_entries = GetBudgetEntries(api, budgets);
      CHECK(budget_entries);

      auto new_end = base::ranges::remove_if(
          *budget_entries,
          [=](const proto::PrivateAggregationBudgetEntry& elem) {
            return elem.entry_start_timestamp() >= serialized_delete_begin &&
                   elem.entry_start_timestamp() <= serialized_delete_end;
          });
      budget_entries->erase(new_end, budget_entries->end());

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

    for (PrivateAggregationCallerApi api : kAllApis) {
      google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetEntry>*
          budget_entries = GetBudgetEntries(api, budgets);
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

bool PrivateAggregationBudgeter::DidStorageInitializationSucceed() {
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

}  // namespace content

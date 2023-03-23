// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budgeter.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
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
#include "content/browser/private_aggregation/proto/private_aggregation_budgets.pb.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ValidityStatus = PrivateAggregationBudgeter::BudgetValidityStatus;

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
      "PrivacySandbox.PrivateAggregation.Budgeter.BudgetValidityStatus",
      status);
}

void ComputeAndRecordBudgetValidity(
    google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetPerHour>*
        hourly_budgets,
    int64_t earliest_window_in_scope_start,
    int64_t current_window_start) {
  if (hourly_budgets->empty()) {
    RecordBudgetValidity(ValidityStatus::kValidAndEmpty);
    return;
  }

  constexpr int64_t kWindowDuration =
      PrivateAggregationBudgetKey::TimeWindow::kDuration.InMicroseconds();

  ValidityStatus status = ValidityStatus::kValid;

  for (proto::PrivateAggregationBudgetPerHour& elem : *hourly_budgets) {
    int64_t hour_start = elem.hour_start_timestamp();
    int budget = elem.budget_used();

    if (budget <= 0) {
      RecordBudgetValidity(ValidityStatus::kContainsNonPositiveValue);
      return;

    } else if (hour_start % kWindowDuration != 0) {
      RecordBudgetValidity(ValidityStatus::kContainsTimestampNotRoundedToHour);
      return;

    } else if (budget >
               blink::features::kPrivateAggregationApiMaxBudgetPerScope.Get()) {
      RecordBudgetValidity(ValidityStatus::kContainsValueExceedingLimit);
      return;

    } else if (hour_start >= current_window_start + kWindowDuration) {
      RecordBudgetValidity(ValidityStatus::kContainsTimestampInFuture);
      return;

    } else if (hour_start < earliest_window_in_scope_start) {
      status = ValidityStatus::kValidButContainsStaleWindow;
    }
  }

  constexpr int64_t kMaximumWindowStartDifference =
      PrivateAggregationBudgeter::kBudgetScopeDuration.InMicroseconds() -
      kWindowDuration;
  const auto minmax = base::ranges::minmax(
      *hourly_budgets, /*comp=*/{},
      &proto::PrivateAggregationBudgetPerHour::hour_start_timestamp);

  DCHECK_EQ(kMaximumWindowStartDifference,
            current_window_start - earliest_window_in_scope_start);
  if (minmax.second.hour_start_timestamp() -
          minmax.first.hour_start_timestamp() >
      kMaximumWindowStartDifference) {
    RecordBudgetValidity(ValidityStatus::kSpansMoreThanADay);
    return;
  }

  RecordBudgetValidity(status);
}

google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetPerHour>*
GetHourlyBudgets(PrivateAggregationBudgetKey::Api api,
                 proto::PrivateAggregationBudgets& budgets) {
  switch (api) {
    case PrivateAggregationBudgetKey::Api::kFledge:
      return budgets.mutable_fledge_budgets();
    case PrivateAggregationBudgetKey::Api::kSharedStorage:
      return budgets.mutable_shared_storage_budgets();
  }
}

}  // namespace

PrivateAggregationBudgeter::PrivateAggregationBudgeter(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner,
    bool exclusively_run_in_memory,
    const base::FilePath& path_to_db_dir)
    : db_task_runner_(std::move(db_task_runner)) {
  DCHECK(db_task_runner_);
  shutdown_initializing_storage_ = PrivateAggregationBudgetStorage::CreateAsync(
      db_task_runner_, exclusively_run_in_memory, path_to_db_dir,
      /*on_done_initializing=*/
      base::BindOnce(&PrivateAggregationBudgeter::OnStorageDoneInitializing,
                     weak_factory_.GetWeakPtr()));
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

void PrivateAggregationBudgeter::ConsumeBudget(
    int budget,
    const PrivateAggregationBudgetKey& budget_key,
    base::OnceCallback<void(RequestResult)> on_done) {
  if (storage_status_ == StorageStatus::kInitializing) {
    if (pending_calls_.size() >= kMaxPendingCalls) {
      std::move(on_done).Run(RequestResult::kTooManyPendingCalls);
      return;
    }

    // `base::Unretained` is safe as `pending_calls_` is owned by `this`.
    pending_calls_.push_back(base::BindOnce(
        &PrivateAggregationBudgeter::ConsumeBudgetImpl, base::Unretained(this),
        budget, budget_key, std::move(on_done)));
  } else {
    ConsumeBudgetImpl(budget, budget_key, std::move(on_done));
  }
}

void PrivateAggregationBudgeter::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    base::OnceClosure done) {
  // When a clear data task is queued or running, we use a higher priority. We
  // do this even if the storage hasn't finished initializing.
  ++num_pending_clear_data_tasks_;
  db_task_runner_->UpdatePriority(base::TaskPriority::USER_VISIBLE);

  done = base::BindOnce(&PrivateAggregationBudgeter::OnClearDataComplete,
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

void PrivateAggregationBudgeter::OnClearDataComplete() {
  DCHECK_GT(num_pending_clear_data_tasks_, 0);
  --num_pending_clear_data_tasks_;

  // No more clear data tasks, so we can reset the priority.
  if (num_pending_clear_data_tasks_ == 0)
    db_task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);
}

void PrivateAggregationBudgeter::OnStorageDoneInitializing(
    std::unique_ptr<PrivateAggregationBudgetStorage> storage) {
  DCHECK(shutdown_initializing_storage_);
  DCHECK(!storage_);
  DCHECK_EQ(storage_status_, StorageStatus::kInitializing);

  if (storage) {
    storage_status_ = StorageStatus::kOpen;
    storage_ = std::move(storage);
  } else {
    storage_status_ = StorageStatus::kInitializationFailed;
  }
  shutdown_initializing_storage_.Reset();

  ProcessAllPendingCalls();
}

void PrivateAggregationBudgeter::ProcessAllPendingCalls() {
  for (base::OnceClosure& cb : pending_calls_) {
    std::move(cb).Run();
  }
  pending_calls_.clear();
}

// TODO(crbug.com/1336733): Consider enumerating different error cases and log
// metrics and/or expose to callers.
void PrivateAggregationBudgeter::ConsumeBudgetImpl(
    int additional_budget,
    const PrivateAggregationBudgetKey& budget_key,
    base::OnceCallback<void(RequestResult)> on_done) {
  const int kMaxBudgetPerScope =
      blink::features::kPrivateAggregationApiMaxBudgetPerScope.Get();

  switch (storage_status_) {
    case StorageStatus::kInitializing:
      NOTREACHED();
      break;
    case StorageStatus::kInitializationFailed:
      std::move(on_done).Run(RequestResult::kStorageInitializationFailed);
      return;
    case StorageStatus::kOpen:
      break;
  }

  if (additional_budget <= 0) {
    std::move(on_done).Run(RequestResult::kInvalidRequest);
    return;
  }
  if (additional_budget > kMaxBudgetPerScope) {
    std::move(on_done).Run(RequestResult::kRequestedMoreThanTotalBudget);
    return;
  }

  std::string origin_key = budget_key.origin().Serialize();

  // If there is no budget proto stored for this origin already, we use the
  // default initialization of `budgets` (untouched by `TryGetData()`).
  proto::PrivateAggregationBudgets budgets;
  storage_->budgets_data()->TryGetData(origin_key, &budgets);

  google::protobuf::RepeatedPtrField<proto::PrivateAggregationBudgetPerHour>*
      hourly_budgets = GetHourlyBudgets(budget_key.api(), budgets);
  DCHECK(hourly_budgets);

  const int64_t current_window_start =
      SerializeTimeForStorage(budget_key.time_window().start_time());
  DCHECK_EQ(current_window_start % base::Time::kMicrosecondsPerHour, 0);

  // Budget windows must start on or after this timestamp to be counted in the
  // current day.
  const int64_t earliest_window_in_scope_start =
      current_window_start +
      PrivateAggregationBudgetKey::TimeWindow::kDuration.InMicroseconds() -
      kBudgetScopeDuration.InMicroseconds();

  ComputeAndRecordBudgetValidity(hourly_budgets, earliest_window_in_scope_start,
                                 current_window_start);

  proto::PrivateAggregationBudgetPerHour* window_for_key = nullptr;
  base::CheckedNumeric<int> total_budget_used = 0;
  bool should_clean_up_stale_budgets = false;

  for (proto::PrivateAggregationBudgetPerHour& elem : *hourly_budgets) {
    if (elem.hour_start_timestamp() < earliest_window_in_scope_start) {
      should_clean_up_stale_budgets = true;
      continue;
    }
    if (elem.hour_start_timestamp() == current_window_start) {
      window_for_key = &elem;
    }

    // Protect against bad values on disk
    if (elem.budget_used() <= 0) {
      std::move(on_done).Run(RequestResult::kBadValuesOnDisk);
      return;
    }

    total_budget_used += elem.budget_used();
  }

  total_budget_used += additional_budget;

  RequestResult budget_increase_request_result;
  if (!total_budget_used.IsValid()) {
    budget_increase_request_result = RequestResult::kBadValuesOnDisk;
  } else if (total_budget_used.ValueOrDie() > kMaxBudgetPerScope) {
    budget_increase_request_result = RequestResult::kInsufficientBudget;
  } else {
    budget_increase_request_result = RequestResult::kApproved;
  }

  if (budget_increase_request_result == RequestResult::kApproved) {
    if (!window_for_key) {
      window_for_key = hourly_budgets->Add();
      window_for_key->set_hour_start_timestamp(current_window_start);
      window_for_key->set_budget_used(0);
    }
    int budget_used_for_key = window_for_key->budget_used() + additional_budget;
    DCHECK_GT(budget_used_for_key, 0);
    DCHECK_LE(budget_used_for_key, kMaxBudgetPerScope);
    window_for_key->set_budget_used(budget_used_for_key);
  }

  if (should_clean_up_stale_budgets) {
    auto new_end = std::remove_if(
        hourly_budgets->begin(), hourly_budgets->end(),
        [&earliest_window_in_scope_start](
            const proto::PrivateAggregationBudgetPerHour& elem) {
          return elem.hour_start_timestamp() < earliest_window_in_scope_start;
        });
    hourly_budgets->erase(new_end, hourly_budgets->end());
  }

  if (budget_increase_request_result == RequestResult::kApproved ||
      should_clean_up_stale_budgets) {
    storage_->budgets_data()->UpdateData(origin_key, budgets);
  }
  std::move(on_done).Run(budget_increase_request_result);
}

void PrivateAggregationBudgeter::ClearDataImpl(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    base::OnceClosure done) {
  switch (storage_status_) {
    case StorageStatus::kInitializing:
      NOTREACHED();
      break;
    case StorageStatus::kInitializationFailed:
      std::move(done).Run();
      return;
    case StorageStatus::kOpen:
      break;
  }

  // Treat null times as unbounded lower or upper range. This is used by
  // browsing data remover.
  if (delete_begin.is_null())
    delete_begin = base::Time::Min();

  if (delete_end.is_null())
    delete_end = base::Time::Max();

  bool is_all_time_covered = delete_begin.is_min() && delete_end.is_max();

  if (is_all_time_covered && filter.is_null()) {
    storage_->budgets_data()->DeleteAllData();

    // Runs `done` once flushing is complete.
    storage_->budgets_data()->FlushDataToDisk(std::move(done));
    return;
  }

  std::vector<std::string> origins_to_delete;

  for (const auto& [origin_key, budgets] :
       storage_->budgets_data()->GetAllCached()) {
    if (filter.is_null() ||
        filter.Run(blink::StorageKey::CreateFromStringForTesting(origin_key))) {
      origins_to_delete.push_back(origin_key);
    }
  }

  if (is_all_time_covered) {
    storage_->budgets_data()->DeleteData(origins_to_delete);

    // Runs `done` once flushing is complete.
    storage_->budgets_data()->FlushDataToDisk(std::move(done));
    return;
  }

  // Ensure we round down to capture any time windows that partially overlap.
  int64_t serialized_delete_begin = SerializeTimeForStorage(
      PrivateAggregationBudgetKey::TimeWindow(delete_begin).start_time());

  // No need to round up as we compare against the time window's start time.
  int64_t serialized_delete_end = SerializeTimeForStorage(delete_end);

  for (const std::string& origin_key : origins_to_delete) {
    proto::PrivateAggregationBudgets budgets;
    storage_->budgets_data()->TryGetData(origin_key, &budgets);

    static constexpr PrivateAggregationBudgetKey::Api kAllApis[] = {
        PrivateAggregationBudgetKey::Api::kFledge,
        PrivateAggregationBudgetKey::Api::kSharedStorage};

    for (PrivateAggregationBudgetKey::Api api : kAllApis) {
      google::protobuf::RepeatedPtrField<
          proto::PrivateAggregationBudgetPerHour>* hourly_budgets =
          GetHourlyBudgets(api, budgets);
      DCHECK(hourly_budgets);

      auto new_end = std::remove_if(
          hourly_budgets->begin(), hourly_budgets->end(),
          [=](const proto::PrivateAggregationBudgetPerHour& elem) {
            return elem.hour_start_timestamp() >= serialized_delete_begin &&
                   elem.hour_start_timestamp() <= serialized_delete_end;
          });
      hourly_budgets->erase(new_end, hourly_budgets->end());
    }
    storage_->budgets_data()->UpdateData(origin_key, budgets);
  }

  // Force the database to be flushed immediately instead of waiting up to
  // `PrivateAggregationBudgetStorage::kFlushDelay`. Runs the `done` callback
  // once flushing is complete.
  storage_->budgets_data()->FlushDataToDisk(std::move(done));
}

}  // namespace content

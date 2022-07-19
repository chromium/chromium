// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budgeter.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budget_storage.h"
#include "content/browser/private_aggregation/proto/private_aggregation_budgets.pb.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace content {

namespace {

int64_t SerializeTimeForStorage(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
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
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    bool exclusively_run_in_memory,
    const base::FilePath& path_to_db_dir) {
  DCHECK(db_task_runner);
  shutdown_initializing_storage_ = PrivateAggregationBudgetStorage::CreateAsync(
      std::move(db_task_runner), exclusively_run_in_memory, path_to_db_dir,
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
    base::OnceCallback<void(bool)> on_done) {
  if (storage_status_ == StorageStatus::kInitializing) {
    if (pending_consume_budget_calls_.size() >= kMaxPendingCalls) {
      std::move(on_done).Run(false);
      return;
    }

    // `base::Unretained` is safe as `pending_consume_budget_calls_` is owned by
    // `this`.
    pending_consume_budget_calls_.push_back(base::BindOnce(
        &PrivateAggregationBudgeter::ConsumeBudgetImpl, base::Unretained(this),
        budget, budget_key, std::move(on_done)));
  } else {
    ConsumeBudgetImpl(budget, budget_key, std::move(on_done));
  }
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
  for (base::OnceClosure& cb : pending_consume_budget_calls_) {
    std::move(cb).Run();
  }
  pending_consume_budget_calls_.clear();
}

// TODO(crbug.com/1336733): Consider enumerating different error cases and log
// metrics and/or expose to callers.
void PrivateAggregationBudgeter::ConsumeBudgetImpl(
    int additional_budget,
    const PrivateAggregationBudgetKey& budget_key,
    base::OnceCallback<void(bool)> on_done) {
  switch (storage_status_) {
    case StorageStatus::kInitializing:
      NOTREACHED();
      break;
    case StorageStatus::kInitializationFailed:
      std::move(on_done).Run(false);
      return;
    case StorageStatus::kOpen:
      break;
  }

  if (additional_budget <= 0 || additional_budget > kMaxBudgetPerScope) {
    std::move(on_done).Run(false);
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

  // Budget windows must start strictly after this timestamp to be counted in
  // the current day. The storage should not contain any time windows from the
  // future.
  int64_t window_must_start_strictly_after = SerializeTimeForStorage(
      budget_key.time_window().start_time() - kBudgetScopeDuration);

  int64_t window_for_key_begins =
      SerializeTimeForStorage(budget_key.time_window().start_time());
  DCHECK_EQ(window_for_key_begins % base::Time::kMicrosecondsPerHour, 0);

  proto::PrivateAggregationBudgetPerHour* window_for_key = nullptr;
  base::CheckedNumeric<int> total_budget_used = 0;
  bool should_clean_up_stale_budgets = false;
  for (proto::PrivateAggregationBudgetPerHour& elem : *hourly_budgets) {
    if (elem.hour_start_timestamp() <= window_must_start_strictly_after) {
      should_clean_up_stale_budgets = true;
      continue;
    }
    if (elem.hour_start_timestamp() == window_for_key_begins) {
      window_for_key = &elem;
    }

    // Protect against bad values on disk
    if (elem.budget_used() <= 0) {
      std::move(on_done).Run(false);
      return;
    }

    total_budget_used += elem.budget_used();
  }

  total_budget_used += additional_budget;

  bool budget_increase_allowed =
      total_budget_used.IsValid() &&
      (total_budget_used.ValueOrDie() <= kMaxBudgetPerScope);

  if (budget_increase_allowed) {
    if (!window_for_key) {
      window_for_key = hourly_budgets->Add();
      window_for_key->set_hour_start_timestamp(window_for_key_begins);
      window_for_key->set_budget_used(0);
    }
    int budget_used_for_key = window_for_key->budget_used() + additional_budget;
    DCHECK_GT(budget_used_for_key, 0);
    DCHECK_LE(budget_used_for_key, kMaxBudgetPerScope);
    window_for_key->set_budget_used(budget_used_for_key);
  }

  if (should_clean_up_stale_budgets) {
    auto new_end =
        std::remove_if(hourly_budgets->begin(), hourly_budgets->end(),
                       [&window_must_start_strictly_after](
                           const proto::PrivateAggregationBudgetPerHour& elem) {
                         return elem.hour_start_timestamp() <=
                                window_must_start_strictly_after;
                       });
    hourly_budgets->erase(new_end, hourly_budgets->end());
  }

  if (budget_increase_allowed || should_clean_up_stale_budgets) {
    storage_->budgets_data()->UpdateData(origin_key, budgets);
  }
  std::move(on_done).Run(budget_increase_allowed);
}

}  // namespace content

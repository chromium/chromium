// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGETER_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGETER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"

namespace base {
class FilePath;
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace content {

class PrivateAggregationBudgetStorage;

// UI thread class that provides an interface for querying and updating the
// budget used by each key, i.e. the sum of contributions, by interacting with
// the storage layer. This class is responsible for owning the storage class.
class CONTENT_EXPORT PrivateAggregationBudgeter {
 public:
  // Public for testing
  enum class StorageStatus {
    // The database is in the process of being initialized.
    kInitializing,
    // The database initialization did not succeed.
    kInitializationFailed,
    // The database successfully initialized and can be used.
    kOpen,
  };

  // The result of a request to consume some budget. All results other than
  // `kApproved` enumerate different reasons the request was rejected.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RequestResult {
    kApproved = 0,
    kInsufficientBudget = 1,
    kRequestedMoreThanTotalBudget = 2,
    kTooManyPendingCalls = 3,
    kInvalidRequest = 4,
    kStorageInitializationFailed = 5,
    kBadValuesOnDisk = 6,
    kMaxValue = kBadValuesOnDisk,
  };

  // Represents the validity status of the stored budget data for the provided
  // origin and API retrieved during a `ConsumeBudget()` call. In case multiple
  // statuses apply, the first one encountered/detected will be used.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class BudgetValidityStatus {
    kValid = 0,
    kValidAndEmpty = 1,
    kValidButContainsStaleWindow = 2,
    kContainsTimestampInFuture = 3,
    kContainsValueExceedingLimit = 4,
    kContainsTimestampNotRoundedToHour = 5,
    kSpansMoreThanADay = 6,
    kContainsNonPositiveValue = 7,
    kMaxValue = kContainsNonPositiveValue,
  };

  // To avoid unbounded memory growth, limit the number of pending calls during
  // initialization. Data clearing calls can be posted even if it would exceed
  // this limit.
  static constexpr int kMaxPendingCalls = 1000;

  // The total length of time that per-origin per-API budgets are enforced
  // against. Note that there are 24 `PrivateAggregationBudgetKey::TimeWindow`s
  // per `kBudgetScopeDuration`.
  static constexpr base::TimeDelta kBudgetScopeDuration = base::Days(1);
  static_assert(kBudgetScopeDuration %
                    PrivateAggregationBudgetKey::TimeWindow::kDuration ==
                base::TimeDelta());

  // `db_task_runner` should not be nullptr.
  PrivateAggregationBudgeter(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner,
      bool exclusively_run_in_memory,
      const base::FilePath& path_to_db_dir);

  PrivateAggregationBudgeter(const PrivateAggregationBudgeter& other) = delete;
  PrivateAggregationBudgeter& operator=(
      const PrivateAggregationBudgeter& other) = delete;
  virtual ~PrivateAggregationBudgeter();

  // Attempts to consume `budget` for `budget_key`. The callback `on_done` is
  // then run with the appropriate `RequestResult`.
  //
  // The attempt is rejected if it would cause an origin's daily per-API budget
  // to exceed `kMaxBudgetPerScope` (for the 24-hour period ending at the *end*
  // of `budget_key.time_window`, see `kBudgetScopeDuration` and
  // `PrivateAggregationBudgetKey` for more detail). The attempt is also
  // rejected if the requested `budget` is non-positive, if `budget_key.origin`
  // is not potentially trustworthy or if the database is closed. If the
  // database is initializing, this query is queued until the initialization is
  // complete. Otherwise, the budget use is recorded and the attempt is
  // successful. May clean up stale budget storage. Note that this call assumes
  // that budget time windows are non-decreasing. In very rare cases, a network
  // time update could allow budget to be used slightly early. Virtual for
  // testing.
  virtual void ConsumeBudget(int budget,
                             const PrivateAggregationBudgetKey& budget_key,
                             base::OnceCallback<void(RequestResult)> on_done);

  // Deletes all data in storage for any budgets that could have been set
  // between `delete_begin` and `delete_end` time (inclusive). Note that the
  // discrete time windows used may lead to more data being deleted than
  // strictly necessary. Null times are treated as unbounded lower or upper
  // range. If `!filter.is_null()`, budget keys with an origin that does *not*
  // match the `filter` are retained (i.e. not cleared).
  virtual void ClearData(base::Time delete_begin,
                         base::Time delete_end,
                         StoragePartition::StorageKeyMatcherFunction filter,
                         base::OnceClosure done);

  // TODO(crbug.com/1328439): Clear stale data periodically and on startup.

 protected:
  // Should only be used for testing/mocking to avoid creating the underlying
  // storage.
  PrivateAggregationBudgeter();

  // Virtual for testing.
  virtual void OnStorageDoneInitializing(
      std::unique_ptr<PrivateAggregationBudgetStorage> storage);

  StorageStatus storage_status_ = StorageStatus::kInitializing;

 private:
  void ConsumeBudgetImpl(int additional_budget,
                         const PrivateAggregationBudgetKey& budget_key,
                         base::OnceCallback<void(RequestResult)> on_done);
  void ClearDataImpl(base::Time delete_begin,
                     base::Time delete_end,
                     StoragePartition::StorageKeyMatcherFunction filter,
                     base::OnceClosure done);
  void OnClearDataComplete();

  void ProcessAllPendingCalls();

  // While the storage initializes, queues calls (e.g. to `ConsumeBudget()`) in
  // the order the calls are received. Should be empty after storage is
  // initialized. The size is limited to `kMaxPendingCalls` except that
  // `ClearData()` can store additional tasks beyond that limit.
  std::vector<base::OnceClosure> pending_calls_;

  // The task runner for all private aggregation storage operations. Updateable
  // to allow for priority to be temporarily increased to `USER_VISIBLE` when a
  // clear data task is queued or running. Otherwise `BEST_EFFORT` is used.
  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;

  // How many clear data storage tasks are queued or running currently, i.e.
  // have been posted but the reply has not been run.
  int num_pending_clear_data_tasks_ = 0;

  // `nullptr` until initialization is complete or if initialization failed.
  // Otherwise, owned by this class until destruction. Iff present,
  // `storage_status_` should be `kOpen`.
  std::unique_ptr<PrivateAggregationBudgetStorage> storage_;

  // Holds a closure that will shut down the initializing storage until
  // initialization is complete. After then, it is null.
  base::OnceClosure shutdown_initializing_storage_;

  base::WeakPtrFactory<PrivateAggregationBudgeter> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGETER_H_

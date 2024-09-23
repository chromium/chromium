// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGETER_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGETER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/common/content_export.h"
#include "content/public/browser/private_aggregation_data_model.h"
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
    // The database initialization process hasn't started yet.
    kPendingInitialization,
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
    kInsufficientSmallerScopeBudget = 1,
    kInsufficientLargerScopeBudget = 2,
    kRequestedMoreThanTotalBudget = 3,
    kTooManyPendingCalls = 4,
    kStorageInitializationFailed = 5,
    kBadValuesOnDisk = 6,
    kMaxValue = kBadValuesOnDisk,
  };

  // Represents the validity status of the stored budget data for the provided
  // site and API retrieved during a `ConsumeBudget()` call. In case multiple
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
    kContainsTimestampNotRoundedToMinute = 5,
    kSpansMoreThanADay = 6,
    kContainsNonPositiveValue = 7,
    kMaxValue = kContainsNonPositiveValue,
  };

  // Indicates the desired behavior when budget is denied for a report request.
  enum class BudgetDeniedBehavior {
    // Still send a report, but remove all requested contributions.
    kSendNullReport,

    // Drop the report.
    kDontSendReport,
  };

  // Represents the two different types of budgets, which differ on the duration
  // of time that they apply to and what the allowable budget for that time is.
  enum class BudgetScope {
    // Scope is per-site per-API per-10 min
    kSmallerScope,

    // Scope is per-site per-API per-day
    kLargerScope,
  };

  // Encapsulates constants that differ for the two scopes, allowing them to be
  // passed around more easily.
  struct BudgetScopeValues {
    BudgetScope budget_scope;

    // Maximum budget allowed to be claimed for this scope.
    int max_budget_per_scope;

    // The total length of time that per-site per-API budgets are enforced
    // against in this scope. (Note that there are 10 time windows per
    // `kBudgetSmallerScopeDuration` and 1440 time windows per
    // `kBudgetLargerScopeDuration`.)
    base::TimeDelta budget_scope_duration;
  };

  static constexpr BudgetScopeValues kSmallerScopeValues = {
      BudgetScope::kSmallerScope, /*max_budget_per_scope=*/65536,
      /*budget_scope_duration=*/base::Minutes(10)};

  static constexpr BudgetScopeValues kLargerScopeValues = {
      BudgetScope::kLargerScope, /*max_budget_per_scope=*/1048576,
      /*budget_scope_duration=*/base::Days(1)};

  static_assert(kSmallerScopeValues.budget_scope_duration %
                    PrivateAggregationBudgetKey::TimeWindow::kDuration ==
                base::TimeDelta());
  static_assert(kLargerScopeValues.budget_scope_duration %
                    PrivateAggregationBudgetKey::TimeWindow::kDuration ==
                base::TimeDelta());

  // The minimum time that needs to pass between `CleanUpStaleData()` calls to
  // avoid unnecessary computation.
  static constexpr base::TimeDelta kMinStaleDataCleanUpGap = base::Minutes(5);

  // To avoid unbounded memory growth, limit the number of pending calls during
  // initialization. Data clearing calls can be posted even if it would exceed
  // this limit.
  static constexpr int kMaxPendingCalls = 1000;

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
  // The attempt is rejected if it would cause a contribution budget to be
  // exceeded, i.e. if the site's per-10 min per-API budget would exceed
  // `kSmallerScopeValues.max_budget_per_scope` and/or if the site's daily
  // per-API budget would exceed `kLargerScopeValues.max_budget_per_scope` (for
  // the 10-min and 24-hour period, respectively, ending at the *end* of
  // `budget_key.time_window`, see the budget scope durations above and
  // `PrivateAggregationBudgetKey` for more detail). The attempt is also
  // rejected if the requested `budget` is non-positive, if `budget_key.origin`
  // is not potentially trustworthy or if the database is closed. If the
  // database is initializing, this query is queued until the initialization is
  // complete. Otherwise, the budget use is recorded and the attempt is
  // successful. May clean up stale budget storage. Note that this call assumes
  // that budget time windows are non-decreasing. In very rare cases, a network
  // time update could allow budget to be used slightly early. Virtual for
  // testing. `minimum_value_for_metrics` is the minimum value for any of the
  // histogram contributions summed in `budget`; it is only used for metrics.
  virtual void ConsumeBudget(int budget,
                             const PrivateAggregationBudgetKey& budget_key,
                             int minimum_value_for_metrics,
                             base::OnceCallback<void(RequestResult)> on_done);
  // Overload to allow minimum_value_for_metrics to have a default of 0 without
  // compiler complaints about the function being virtual. Used mainly to
  // simplify testing.
  void ConsumeBudget(int budget,
                     const PrivateAggregationBudgetKey& budget_key,
                     base::OnceCallback<void(RequestResult)> on_done);

  // Deletes all data in storage for any budgets that could have been set
  // between `delete_begin` and `delete_end` time (inclusive). Note that the
  // discrete time windows used may lead to more data being deleted than
  // strictly necessary. Null times are treated as unbounded lower or upper
  // range. If `!filter.is_null()`, budget entries without an origin that
  // matches the `filter` are retained (i.e. not cleared). Note that budgets are
  // scoped per-site, not per-origin. So, the budget storage keeps track of any
  // reporting origins used in the last day and will delete that corresponding
  // site's data if the `filter` matches any of those origins.
  virtual void ClearData(base::Time delete_begin,
                         base::Time delete_end,
                         StoragePartition::StorageKeyMatcherFunction filter,
                         base::OnceClosure done);

  // Runs `callback` with all reporting origins as DataKeys for the Browsing
  // Data Model. Partial data will still be returned in the event of an error.
  virtual void GetAllDataKeys(
      base::OnceCallback<void(std::set<PrivateAggregationDataModel::DataKey>)>
          callback);

  // Deletes all data in storage for storage keys matching the provided
  // reporting origin in the data key.
  virtual void DeleteByDataKey(const PrivateAggregationDataModel::DataKey& key,
                               base::OnceClosure callback);

 protected:
  // Should only be used for testing/mocking to avoid creating the underlying
  // storage.
  PrivateAggregationBudgeter();

  // Virtual for testing.
  virtual void OnStorageDoneInitializing(
      std::unique_ptr<PrivateAggregationBudgetStorage> storage);

  StorageStatus storage_status_ = StorageStatus::kPendingInitialization;

 private:
  void EnsureStorageInitializationBegun();

  void InitializeStorage(bool exclusively_run_in_memory,
                         base::FilePath path_to_db_dir);

  void ConsumeBudgetImpl(int additional_budget,
                         const PrivateAggregationBudgetKey& budget_key,
                         int minimum_value_for_metrics,
                         base::OnceCallback<void(RequestResult)> on_done);
  void ClearDataImpl(base::Time delete_begin,
                     base::Time delete_end,
                     StoragePartition::StorageKeyMatcherFunction filter,
                     base::OnceClosure done);
  void GetAllDataKeysImpl(
      base::OnceCallback<void(std::set<PrivateAggregationDataModel::DataKey>)>
          callback);

  void OnUserVisibleTaskStarted();
  void OnUserVisibleTaskComplete();

  void ProcessAllPendingCalls();

  bool DidStorageInitializationSucceed();

  // Deletes any budgeting data that is too old to affect current or future
  // calls to the API.
  void CleanUpStaleData();

  // Runs `CleanUpStaleData()` unless it was run too recently, when it will be
  // run after waiting for `kMinStaleDataCleanUpGap` to pass between calls.
  void CleanUpStaleDataSoon();

  // While the storage initializes, queues calls (e.g. to `ConsumeBudget()`) in
  // the order the calls are received. Should be empty after storage is
  // initialized. The size is limited to `kMaxPendingCalls` except that
  // `ClearData()` can store additional tasks beyond that limit.
  std::vector<base::OnceClosure> pending_calls_;

  // The task runner for all private aggregation storage operations. Updateable
  // to allow for priority to be temporarily increased to `USER_VISIBLE` when a
  // clear data task is queued or running. Otherwise `BEST_EFFORT` is used.
  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;

  // How many user visible storage tasks are queued or running currently, i.e.
  // have been posted but the reply has not been run.
  int num_pending_user_visible_tasks_ = 0;

  // `nullptr` until initialization is complete or if initialization failed.
  // Otherwise, owned by this class until destruction. Iff present,
  // `storage_status_` should be `kOpen`.
  std::unique_ptr<PrivateAggregationBudgetStorage> storage_;

  // Timer used to defer calls to `CleanUpStaleData()` until
  // `kMinStaleDataCleanUpGap` has passed since the last call.
  base::OneShotTimer clean_up_stale_data_timer_;

  // The last time `CleanUpStaleData()` was called, or `base::TimeTicks::Min()`
  // if never called.
  base::TimeTicks last_clean_up_time_ = base::TimeTicks::Min();

  // Holds a closure that will shut down the initializing storage until
  // initialization is complete. After then, it is null.
  base::OnceClosure shutdown_initializing_storage_;

  // When constructing this class, we create a closure that contains the storage
  // initialization parameters. On the first call to `ConsumeBudget` or
  // `ClearData`, the closure is run to call `InitializeStorage`. This ensures
  // that the storage is only initialized when it is needed and avoid incurring
  // delay on startup. After then, it is null;
  base::OnceClosure initialize_storage_;

  base::WeakPtrFactory<PrivateAggregationBudgeter> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGETER_H_

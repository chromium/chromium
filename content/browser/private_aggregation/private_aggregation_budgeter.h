// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGETER_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGETER_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/common/content_export.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"

namespace base {
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
  //
  // LINT.IfChange(RequestResult)
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
  // LINT.ThenChange(//content/browser/private_aggregation/private_aggregation_budgeter.cc:ComputeOverallRequestResult)

  // For a single contribution, whether the budgeter approved its budget usage
  // (including provisionally) or denied it.
  enum class ResultForContribution {
    kApproved,
    kDenied,
  };

  // Note that if the limit has been reached and there is no space for this
  // report, the entire report will be dropped with a fatal error.
  enum class PendingReportLimitResult {
    // Indicates the limit has not been reached.
    kNotAtLimit,

    // Indicates the limit has been reached with this report, i.e. the report
    // can still be processed.
    kAtLimit
  };

  // Used to ensure the budget cannot be modified between calls to
  // `InspectBudgetAndLock()` and `ConsumeBudget()`. Only one instance of this
  // object can exist and is vended to a caller when the budgeter is 'locked'.
  // This avoids the available budget changing between calls for the same report
  // and will allow for certain optimizations.
  class CONTENT_EXPORT Lock {
   public:
    // Move only
    Lock(Lock&& other) = default;
    Lock& operator=(Lock&& other) = default;

    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

    static Lock CreateForTesting() { return Lock(); }

   private:
    friend PrivateAggregationBudgeter;

    Lock() = default;
  };

  struct CONTENT_EXPORT BudgetQueryResult {
    BudgetQueryResult(
        RequestResult overall_result,
        std::vector<ResultForContribution> result_for_each_contribution);

    BudgetQueryResult(BudgetQueryResult&& other);
    BudgetQueryResult& operator=(BudgetQueryResult&& other);

    ~BudgetQueryResult();

    // The result for the entire call, considering all contributions together.
    // (That is, `kApproved` if all contributions were approved,
    // `kInsufficientSmallerScopeBudget` if some were denied due to an
    // insufficient smaller scope budget, etc.)
    RequestResult overall_result;

    // Empty if a fatal error (i.e. `kTooManyPendingCalls`,
    // `kStorageInitializationFailed` or `kBadValuesOnDisk`) occurred.
    std::vector<ResultForContribution> result_for_each_contribution;
  };
  struct CONTENT_EXPORT InspectBudgetCallResult {
    InspectBudgetCallResult(
        BudgetQueryResult query_result,
        std::optional<Lock> lock,
        PendingReportLimitResult pending_report_limit_result);

    InspectBudgetCallResult(InspectBudgetCallResult&& other);
    InspectBudgetCallResult& operator=(InspectBudgetCallResult&& other);

    ~InspectBudgetCallResult();

    BudgetQueryResult query_result;

    // Populated iff a fatal error did not occur (even if provisional budget was
    // denied for all contributions).
    std::optional<Lock> lock;

    PendingReportLimitResult pending_report_limit_result;
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
  struct CONTENT_EXPORT BudgetScopeValues {
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
      base::FilePath path_to_db_dir);

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
  // rejected if the database is closed. If the database is initializing, this
  // query is queued until the initialization is complete. Otherwise, the budget
  // use is recorded and the attempt is successful. May clean up stale budget
  // storage. Note that this call assumes that budget time windows are
  // non-decreasing. In very rare cases, a network time update could allow
  // budget to be used slightly early. Virtual for testing.
  // `minimum_value_for_metrics` is the minimum value for any of the histogram
  // contributions summed in `budget`; it is only used for metrics. `budget`
  // must be positive.
  //
  // Note: can only be used if `kPrivateAggregationApiErrorReporting` is
  // disabled.
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

  // Queries whether there is sufficient budget for each of the `contributions`
  // without consuming any. The `result_callback` is then run with the
  // appropriate `InspectBudgetCallResult`, containing the (hypothetical) result
  // for each contribution, the `Lock` (unless a fatal error occurred) and
  // whether the pending report limit was reached. The callback may be run
  // either synchronously or asynchronously.
  //
  // Even though this call does not consume budget, the result for each
  // contribution is computed as if any earlier items in `contributions` that
  // were approved did indeed consume their budget. See `ConsumeBudget()` below
  // for more detail on budget definitions and time update considerations.
  //
  // The attempt is also rejected if the database is closed. If the database is
  // initializing or locked, this query is queued until the initialization is
  // complete / lock is released. Otherwise, the query is successful.
  //
  // Note: can only be used if `kPrivateAggregationApiErrorReporting` is
  // enabled.
  virtual void InspectBudgetAndLock(
      const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
          contributions,
      const PrivateAggregationBudgetKey& budget_key,
      base::OnceCallback<void(InspectBudgetCallResult)> result_callback);

  // Attempts to consume budget for each of the `contributions`. The
  // `result_callback` is then run with the result for each contribution. A
  // `Lock` obtained from an earlier call to `InspectBudgetAndLock()` must be
  // provided. The callback may be run either synchronously or asynchronously.
  //
  // Each contribution's attempt is rejected if it would cause a contribution
  // budget to be exceeded, i.e. if the site's per-10 min per-API budget would
  // exceed `kSmallerScopeValues.max_budget_per_scope` and/or if the site's
  // daily per-API budget would exceed `kLargerScopeValues.max_budget_per_scope`
  // (for the 10-min and 24-hour period, respectively, ending at the *end* of
  // `budget_key.time_window`, see the budget scope durations above and
  // `PrivateAggregationBudgetKey` for more detail). Otherwise, the attempt is
  // successful (although see additional error cases below).
  //
  // The result for each contribution takes into account any budget used by
  // earlier `contributions`.
  //
  // May clean up stale budget storage. Note that this call assumes that budget
  // time windows are non-decreasing. In very rare cases, a network time update
  // could allow budget to be used slightly early.
  //
  // Note: can only be used if `kPrivateAggregationApiErrorReporting` is
  // enabled.
  virtual void ConsumeBudget(
      Lock lock,
      const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
          contributions,
      const PrivateAggregationBudgetKey& budget_key,
      base::OnceCallback<void(BudgetQueryResult)> result_callback);

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

  // Combines the results from sequential queries to `InspectBudgetAndLock()`
  // and `ConsumeBudget()`.
  static RequestResult CombineRequestResults(
      RequestResult inspect_budget_result,
      RequestResult consume_budget_result);

 protected:
  // Should only be used for testing/mocking to avoid creating the underlying
  // storage.
  PrivateAggregationBudgeter();

  // Called when storage is initialized. Iff initialization failed, `storage`
  // will be nullptr. Virtual for testing.
  virtual void OnStorageDoneInitializing(
      std::unique_ptr<PrivateAggregationBudgetStorage> storage);

  StorageStatus storage_status_ = StorageStatus::kPendingInitialization;

 private:
  // Begins initializing storage asynchronously. Repeat calls are no-ops.
  // Registers a callback to `OnStorageDoneInitializing()`.
  //
  // We initialize storage lazily to keep startup code fast. This laziness also
  // avoids unnecessary work when storage is not needed. So, rather than eagerly
  // initializing storage in the constructor, the first call to `ConsumeBudget`,
  // `ClearData()`, or `GetAllDataKeys()` will call this method.
  void EnsureStorageInitializationBegun();

  bool IsBudgeterLocked() const;

  Lock VendLock();

  void ConsumeBudgetImpl(int additional_budget,
                         const PrivateAggregationBudgetKey& budget_key,
                         int minimum_value_for_metrics,
                         base::OnceCallback<void(RequestResult)> on_done);
  void InspectBudgetAndLockImpl(
      const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
          contributions,
      const PrivateAggregationBudgetKey& budget_key,
      PendingReportLimitResult pending_report_limit_result,
      base::OnceCallback<void(InspectBudgetCallResult)> result_callback);

  BudgetQueryResult QueryBudget(
      const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
          contributions,
      const PrivateAggregationBudgetKey& budget_key,
      bool consume_budget,
      int minimum_value_for_metrics = 0);

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

  bool DidStorageInitializationSucceed() const;

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
  bool process_all_pending_calls_in_progress_ = false;

  // The task runner for all private aggregation storage operations. Updateable
  // to allow for priority to be temporarily increased to `USER_VISIBLE` when a
  // clear data task is queued or running. Otherwise `BEST_EFFORT` is used.
  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;

  // How many user visible storage tasks are queued or running currently, i.e.
  // have been posted but the reply has not been run.
  int num_pending_user_visible_tasks_ = 0;

  // Whether `storage_` should not write to disk.
  bool exclusively_run_in_memory_ = false;

  // Directory where `storage_` should search for its database. Do not use once
  // storage initialization has begun as it will be in an unspecified state.
  base::FilePath path_to_db_dir_;

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

  // When the budgeter is locked, `locked_timer_` is populated with a timer
  // tracking the elapsed time since the `Lock` object was vended to the manager
  // processing the corresponding report. See `Lock` above. Otherwise, this is
  // std::nullopt.
  std::optional<base::ElapsedTimer> locked_timer_;

  base::WeakPtrFactory<PrivateAggregationBudgeter> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGETER_H_

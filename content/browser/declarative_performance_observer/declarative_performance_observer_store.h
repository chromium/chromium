// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DECLARATIVE_PERFORMANCE_OBSERVER_DECLARATIVE_PERFORMANCE_OBSERVER_STORE_H_
#define CONTENT_BROWSER_DECLARATIVE_PERFORMANCE_OBSERVER_DECLARATIVE_PERFORMANCE_OBSERVER_STORE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {

using OriginMatcherFunction = base::RepeatingCallback<bool(const url::Origin&)>;

// Manages the physical SQLite storage backend for Declarative Performance
// Observers.
//
// Implements disk persistence for website opt-in/opt-out configuration on a
// dedicated background task runner (`db_task_runner_`).
class CONTENT_EXPORT DeclarativePerformanceObserverStore {
 public:
  // If `is_in_memory` is true, opens an in-memory database (for incognito
  // mode).
  DeclarativePerformanceObserverStore(
      bool is_in_memory,
      const base::FilePath& profile_path,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner = nullptr,
      base::OnceClosure on_loaded_callback = base::DoNothing());
  ~DeclarativePerformanceObserverStore();

  DeclarativePerformanceObserverStore(
      const DeclarativePerformanceObserverStore&) = delete;
  DeclarativePerformanceObserverStore& operator=(
      const DeclarativePerformanceObserverStore&) = delete;

  // Persists the early failure observation policy for a given website `origin`
  // to disk. If `enabled` is true, registers or enables early failure capture
  // for `origin`. Otherwise, deletes any policy entry for `origin`.
  void SetEarlyFailurePolicy(const url::Origin& origin,
                             bool enabled,
                             base::OnceClosure callback = base::DoNothing());

  // Determines whether early failure capture is enabled for `origin`. Returns
  // true if enabled.
  // Note: Lookups might return false during the initial "warm-up" phase before
  // the database load completes.
  bool HasEarlyFailurePolicy(const url::Origin& origin);

  // Serializes and stores an early navigation failure report for `origin`.
  void StoreEarlyFailureReport(const url::Origin& origin,
                               base::DictValue report,
                               base::OnceClosure callback = base::DoNothing());

  // Retrieves and deletes all stored early failure reports for `origin`.
  void TakeEarlyFailureReports(
      const url::Origin& origin,
      base::OnceCallback<void(base::ListValue)> callback);

  // Wipes all stored policies and reports for a specific origin.
  // The database connection remains open.
  void ClearDataForOrigin(const url::Origin& origin,
                          base::OnceClosure callback = base::DoNothing());

  // Wipes stored policies and reports for any origin matching `filter`.
  void ClearDataWithFilter(OriginMatcherFunction filter,
                           base::OnceClosure callback = base::DoNothing());

  // Catastrophically razes the entire database and closes the connection
  // to surrender Operating System file locks.
  void ClearAllData(base::OnceClosure callback = base::DoNothing());

  // Sets the physical storage quota limit for testing purposes.
  void SetQuotaLimitForTesting(size_t quota_limit_bytes,
                               base::OnceClosure callback = base::DoNothing());

  // Closes the persistent SQLite database connection engine and flushes any
  // pending I/O transactions on the background sequence.
  void Close(base::OnceClosure callback = base::DoNothing());

  // Verifies that database tables and indexes are properly configured.
  void CheckSchemaForTesting(  // IN-TEST
      base::OnceCallback<void(bool, bool)> callback);

 private:
  struct LoadedPolicy {
    url::Origin origin;
    base::Time created_at;
  };

  class Backend;

  void OnPoliciesLoadedOnUISequence(
      base::OnceClosure on_loaded_callback,
      std::vector<LoadedPolicy> loaded);

  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  scoped_refptr<Backend> backend_;

  // UI-thread In-Memory Policy Cache for instant 0ns lookups:
  base::flat_map<url::Origin, base::Time> cached_policies_;

  // True if the initial database loading has completed.
  bool loaded_ = false;

  // Tracks origins whose policies were modified on the UI thread before the
  // initial database load completed. This prevents database load results
  // from overwriting newer updates.
  base::flat_set<url::Origin> modified_during_load_;
  std::vector<OriginMatcherFunction> pending_filters_;
  bool clear_all_pending_ = false;

  base::WeakPtrFactory<DeclarativePerformanceObserverStore> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DECLARATIVE_PERFORMANCE_OBSERVER_DECLARATIVE_PERFORMANCE_OBSERVER_STORE_H_

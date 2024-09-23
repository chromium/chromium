// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_

#include <set>
#include <vector>

#include "base/auto_reset.h"
#include "base/debug/crash_logging.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

// Set of WorkersIds that provides faster retrieval/removal of workers by
// extension id, render process id etc.
namespace extensions {

class WorkerIdSet {
 public:
  WorkerIdSet();

  WorkerIdSet(const WorkerIdSet&) = delete;
  WorkerIdSet& operator=(const WorkerIdSet&) = delete;

  ~WorkerIdSet();

  void Add(const WorkerId& worker_id, content::BrowserContext* context);
  bool Remove(const WorkerId& worker_id);
  bool Contains(const WorkerId& worker_id) const;
  std::vector<WorkerId> GetAllForExtension(
      const ExtensionId& extension_id) const;
  std::vector<WorkerId> GetAllForExtension(const ExtensionId& extension_id,
                                           int render_process_id) const;
  std::vector<WorkerId> GetAllForExtension(const ExtensionId& extension_id,
                                           int64_t worker_version_id) const;

  std::vector<WorkerId> GetAllForTesting() const;
  static base::AutoReset<bool> AllowMultipleWorkersPerExtensionForTesting();
  size_t count_for_testing() const { return workers_.size(); }

 private:
  std::set<WorkerId> workers_;
};

namespace debug {

// Helper for adding a set of multiple worker related crash keys.
//
// It is meant to be created when we detect exactly two entries for the same
// worker that will be recorded in `WorkerIdSet::Add()`.
//
// These crash keys set information relevant to when two workers are attempting
// to be added to the `WorkerIdSet`. All keys are logged every time this class
// is instantiated. Each key describes its possible values.
//
//  The "previous" `WorkerId` is the one that existed before
//  `WorkerIdSet::Add()` is called. The "new" `WorkerId` is the duplicate that
//  will be added once `WorkerIdSet::Add()` completes.
class ScopedMultiWorkerCrashKeys {
 public:
  explicit ScopedMultiWorkerCrashKeys(const ExtensionId& extension_id,
                                      const WorkerId& previous_worker_id,
                                      const WorkerId& new_worker_id,
                                      content::BrowserContext* context);
  ~ScopedMultiWorkerCrashKeys();

 private:
  base::debug::ScopedCrashKeyString extension_id_crash_key_;

  // Whether the previous and new worker version ids match. Values are: "yes",
  // or "no".
  base::debug::ScopedCrashKeyString identical_version_ids_crash_key_;
  // The `version_id`s for each worker. A (stringified) int64_t that identifies
  // what version of code a worker is running.
  base::debug::ScopedCrashKeyString previous_worker_version_id_crash_key_;
  base::debug::ScopedCrashKeyString new_worker_version_id_crash_key_;

  // The `content::ServiceWorkerRunningInfo::ServiceWorkerStatus`
  // (`ServiceWorkerVersion::Status`) for the previous and new `WorkerId`s.
  base::debug::ScopedCrashKeyString previous_worker_lifecycle_state_crash_key_;
  base::debug::ScopedCrashKeyString new_worker_lifecycle_state_crash_key_;

  // Whether the renderer process ids for the previous and new worker match.
  // Values are: "yes", or "no".
  base::debug::ScopedCrashKeyString
      identical_worker_render_process_ids_crash_key_;

  // Whether the renderer process id for the previous and new worker for this
  // specific extension are running or not. Values are: "yes", or "no".
  base::debug::ScopedCrashKeyString
      previous_worker_render_process_running_crash_key_;
  base::debug::ScopedCrashKeyString
      new_worker_render_process_running_crash_key_;
};

}  // namespace debug

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_

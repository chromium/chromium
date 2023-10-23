// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_

#include <set>
#include <vector>

#include "base/auto_reset.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension_id.h"

// Set of WorkersIds that provides faster retrieval/removal of workers by
// extension id, render process id etc.
namespace extensions {

class WorkerIdSet {
 public:
  WorkerIdSet();

  WorkerIdSet(const WorkerIdSet&) = delete;
  WorkerIdSet& operator=(const WorkerIdSet&) = delete;

  ~WorkerIdSet();

  void Add(const WorkerId& worker_id);
  bool Remove(const WorkerId& worker_id);
  bool Contains(const WorkerId& worker_id) const;
  std::vector<WorkerId> GetAllForExtension(
      const ExtensionId& extension_id) const;
  std::vector<WorkerId> GetAllForExtension(const ExtensionId& extension_id,
                                           int render_process_id) const;

  std::vector<WorkerId> GetAllForTesting() const;
  static base::AutoReset<bool> AllowMultipleWorkersPerExtensionForTesting();
  size_t count_for_testing() const { return workers_.size(); }

 private:
  std::set<WorkerId> workers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_

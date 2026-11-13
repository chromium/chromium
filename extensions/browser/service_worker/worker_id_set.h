// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/auto_reset.h"
#include "content/public/common/child_process_id.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

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
  bool ContainsForExtension(const ExtensionId& extension_id) const;
  bool ContainsForExtension(const ExtensionId& extension_id,
                            int64_t worker_version_id,
                            int worker_thread_id) const;
  size_t CountForExtension(const ExtensionId& extension_id) const;
  std::optional<WorkerId> FindForExtension(
      const ExtensionId& extension_id,
      int64_t worker_version_id,
      const blink::ServiceWorkerToken& start_token) const;
  std::vector<WorkerId> GetAllForExtension(
      const ExtensionId& extension_id) const;
  std::vector<WorkerId> GetAllForExtension(
      const ExtensionId& extension_id,
      content::ChildProcessId render_process_id) const;
  std::vector<WorkerId> GetAllForExtension(const ExtensionId& extension_id,
                                           int64_t worker_version_id) const;

  std::vector<WorkerId> GetAllForTesting() const;
  static base::AutoReset<bool> AllowMultipleWorkersPerExtensionForTesting();
  size_t count_for_testing() const { return workers_.size(); }

 private:
  struct LookupKey {
    std::string_view extension_id;
    content::ChildProcessId render_process_id;
    int64_t version_id;
    int thread_id;
  };

  struct WorkerIdCompare {
    using is_transparent = void;

    // Must stay in sync with `WorkerId::operator<` (see worker_id.cc), which
    // orders by `(extension_id, render_process_id, version_id, thread_id)` and
    // intentionally omits `start_token`.
    static auto AsTuple(const WorkerId& id) {
      return std::tuple(std::string_view(id.extension_id), id.render_process_id,
                        id.version_id, id.thread_id);
    }
    static auto AsTuple(const LookupKey& key) {
      return std::tuple(key.extension_id, key.render_process_id, key.version_id,
                        key.thread_id);
    }

    template <typename Lhs, typename Rhs>
    bool operator()(const Lhs& lhs, const Rhs& rhs) const {
      return AsTuple(lhs) < AsTuple(rhs);
    }
  };

  static LookupKey LowestKeyFor(std::string_view extension_id);

  std::set<WorkerId, WorkerIdCompare> workers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_

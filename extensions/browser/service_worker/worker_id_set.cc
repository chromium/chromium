// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/worker_id_set.h"

#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/common/child_process_id.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace extensions {

namespace {

constexpr int64_t kSmallestVersionId =
    blink::mojom::kInvalidServiceWorkerVersionId;
constexpr int kSmallestThreadId = -1;
constexpr int kSmallestRenderProcessId =
    content::ChildProcessHost::kInvalidUniqueID;
constexpr int kMaxWorkerCountToReport = 50;

// Prevent check on multiple workers per extension for testing purposes.
bool g_allow_multiple_workers_per_extension = false;

static_assert(kSmallestVersionId < 0,
              "Sentinel version_id must be smaller than any valid version id.");
static_assert(kSmallestThreadId < 0,
              "Sentinel thread_id must be smaller than any valid thread id.");
static_assert(
    kSmallestRenderProcessId < 0,
    "Sentinel render_process_id must be smaller than any valid process id.");

content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus
GetWorkerLifecycleState(const WorkerId& worker_id,
                        content::BrowserContext* context) {
  content::ServiceWorkerContext* sw_context =
      util::GetServiceWorkerContextForExtensionId(worker_id.extension_id,
                                                  context);
  const base::flat_map<int64_t, content::ServiceWorkerRunningInfo>&
      worker_running_info = sw_context->GetRunningServiceWorkerInfos();

  const auto search_worker_running =
      worker_running_info.find(worker_id.version_id);
  return search_worker_running != worker_running_info.end()
             ? search_worker_running->second.version_status
             // Didn't find registered worker as running worker in the //content
             // layer so return unknown status. This indicates we're tracking a
             // worker that is unknown to the lower SW layer.
             : content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
                   kUnknown;
}

void EmitMultiWorkerMetrics(const char* metric_name,
                            const WorkerId& worker_id,
                            content::BrowserContext* context) {
  base::UmaHistogramEnumeration(metric_name,
                                GetWorkerLifecycleState(worker_id, context));
}

}  // namespace

WorkerIdSet::WorkerIdSet() = default;
WorkerIdSet::~WorkerIdSet() = default;

void WorkerIdSet::Add(const WorkerId& worker_id,
                      content::BrowserContext* context) {
  // We should not try to register the same WorkerId multiple times.
  CHECK(!Contains(worker_id));
  CHECK(worker_id.start_token.has_value());

  size_t previous_count = 0;
  auto first_previous_worker = workers_.end();
  for (auto it = workers_.lower_bound(LowestKeyFor(worker_id.extension_id));
       it != workers_.end() && it->extension_id == worker_id.extension_id;
       ++it) {
    if (previous_count == 0) {
      first_previous_worker = it;
    }
    ++previous_count;
  }

  workers_.insert(worker_id);
  size_t new_size = previous_count + 1;
  base::UmaHistogramExactLinear(
      "Extensions.ServiceWorkerBackground.WorkerCountAfterAdd", new_size,
      kMaxWorkerCountToReport);

  if (!g_allow_multiple_workers_per_extension) {
    // TODO(crbug.com/40936639): Enable this CHECK once multiple active workers
    // is resolved.
    // CHECK_LE(new_size, 1u) << "Extension with worker id "
    //                        << worker_id
    //                        << " added additional worker";
  }

  // Only emit our incorrect worker metrics if an unexpected number of workers
  // is present.
  if (new_size == 2) {
    EmitMultiWorkerMetrics(
        "Extensions.ServiceWorkerBackground.MultiWorkerVersionStatus.NewWorker",
        worker_id, context);
    // new_size == 2 guarantees that a previous WorkerId will be present.
    const WorkerId& previous_worker_id = *first_previous_worker;
    EmitMultiWorkerMetrics(
        "Extensions.ServiceWorkerBackground.MultiWorkerVersionStatus."
        "PreviousWorker",
        previous_worker_id, context);
    base::UmaHistogramBoolean(
        "Extensions.ServiceWorkerBackground.MultiWorkerVersionIdMatch",
        worker_id.version_id == previous_worker_id.version_id);
  }
}

bool WorkerIdSet::Remove(const WorkerId& worker_id) {
  bool erased = workers_.erase(worker_id);
  base::UmaHistogramExactLinear(
      "Extensions.ServiceWorkerBackground.WorkerCountAfterRemove",
      CountForExtension(worker_id.extension_id), kMaxWorkerCountToReport);
  return erased;
}

bool WorkerIdSet::ContainsForExtension(const ExtensionId& extension_id) const {
  auto it = workers_.lower_bound(LowestKeyFor(extension_id));
  return it != workers_.end() && it->extension_id == extension_id;
}

bool WorkerIdSet::ContainsForExtension(const ExtensionId& extension_id,
                                       int64_t worker_version_id,
                                       int worker_thread_id) const {
  for (auto it = workers_.lower_bound(LowestKeyFor(extension_id));
       it != workers_.end() && it->extension_id == extension_id; ++it) {
    if (it->version_id == worker_version_id &&
        it->thread_id == worker_thread_id) {
      return true;
    }
  }
  return false;
}

size_t WorkerIdSet::CountForExtension(const ExtensionId& extension_id) const {
  size_t count = 0;
  for (auto it = workers_.lower_bound(LowestKeyFor(extension_id));
       it != workers_.end() && it->extension_id == extension_id; ++it) {
    ++count;
  }
  return count;
}

std::optional<WorkerId> WorkerIdSet::FindForExtension(
    const ExtensionId& extension_id,
    int64_t worker_version_id,
    const blink::ServiceWorkerToken& start_token) const {
  for (auto it = workers_.lower_bound(LowestKeyFor(extension_id));
       it != workers_.end() && it->extension_id == extension_id; ++it) {
    if (it->version_id == worker_version_id && it->start_token == start_token) {
      return *it;
    }
  }
  return std::nullopt;
}

std::vector<WorkerId> WorkerIdSet::GetAllForExtension(
    const ExtensionId& extension_id) const {
  // Construct a key that is guaranteed to be smaller than any key given a
  // |render_process_id|. This facilitates the usage of lower_bound to achieve
  // lg(n) runtime without heap-allocating an ExtensionId copy.
  auto begin_range = workers_.lower_bound(LowestKeyFor(extension_id));
  if (begin_range == workers_.end() ||
      begin_range->extension_id != extension_id) {
    return {};  // No entries.
  }

  auto end_range = std::next(begin_range);
  while (end_range != workers_.end() && end_range->extension_id == extension_id)
    ++end_range;
  return std::vector<WorkerId>(begin_range, end_range);
}

std::vector<WorkerId> WorkerIdSet::GetAllForExtension(
    const ExtensionId& extension_id,
    int64_t worker_version_id) const {
  std::vector<WorkerId> worker_ids;
  for (auto it = workers_.lower_bound(LowestKeyFor(extension_id));
       it != workers_.end() && it->extension_id == extension_id; ++it) {
    if (it->version_id == worker_version_id) {
      worker_ids.push_back(*it);
    }
  }
  return worker_ids;
}

bool WorkerIdSet::Contains(const WorkerId& worker_id) const {
  return workers_.contains(worker_id);
}

std::vector<WorkerId> WorkerIdSet::GetAllForExtension(
    const ExtensionId& extension_id,
    content::ChildProcessId render_process_id) const {
  // See other GetAllForExtension's notes for key construction.
  const LookupKey lowest_key{extension_id, render_process_id,
                             kSmallestVersionId, kSmallestThreadId};

  auto begin_range = workers_.lower_bound(lowest_key);
  if (begin_range == workers_.end() ||
      begin_range->extension_id > extension_id ||
      begin_range->render_process_id > render_process_id) {
    return std::vector<WorkerId>();  // No entries.
  }

  auto end_range = std::next(begin_range);
  while (end_range != workers_.end() &&
         end_range->extension_id == extension_id &&
         // If |extension_id| matches, then |end_range->render_process_id| must
         // be GTE to |render_process_id|.
         end_range->render_process_id == render_process_id) {
    ++end_range;
  }
  return std::vector<WorkerId>(begin_range, end_range);
}

std::vector<WorkerId> WorkerIdSet::GetAllForTesting() const {
  return std::vector<WorkerId>(workers_.begin(), workers_.end());
}

// static
base::AutoReset<bool>
WorkerIdSet::AllowMultipleWorkersPerExtensionForTesting() {
  return base::AutoReset<bool>(&g_allow_multiple_workers_per_extension, true);
}

// static
WorkerIdSet::LookupKey WorkerIdSet::LowestKeyFor(
    std::string_view extension_id) {
  return LookupKey{
      extension_id,
      content::ChildProcessId::FromUnsafeValue(kSmallestRenderProcessId),
      kSmallestVersionId, kSmallestThreadId};
}

}  // namespace extensions

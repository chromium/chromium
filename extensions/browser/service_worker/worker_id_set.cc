// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/worker_id_set.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension_id.h"
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

  std::vector<WorkerId> previous_worker_ids =
      GetAllForExtension(worker_id.extension_id);

  workers_.insert(worker_id);
  size_t new_size = previous_worker_ids.size() + 1;
  base::UmaHistogramExactLinear(
      "Extensions.ServiceWorkerBackground.WorkerCountAfterAdd", new_size,
      kMaxWorkerCountToReport);

  if (!g_allow_multiple_workers_per_extension) {
    // TODO(crbug.com/40936639):Enable this CHECK once multiple active workers
    // is resolved. CHECK_LE(new_size, 1u) << "Extension with worker id " <<
    // worker_id
    //                        << " added additional worker";
  }

  // Only emit our incorrect worker metrics if an unexpected number of workers
  // is present.
  if (new_size == 2) {
    EmitMultiWorkerMetrics(
        "Extensions.ServiceWorkerBackground.MultiWorkerVersionStatus.NewWorker",
        worker_id, context);
    // new_size == 2 guarantees that a previous WorkerId will be present.
    const WorkerId& previous_worker_id = previous_worker_ids.front();
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
      GetAllForExtension(worker_id.extension_id).size(),
      kMaxWorkerCountToReport);
  return erased;
}

std::vector<WorkerId> WorkerIdSet::GetAllForExtension(
    const ExtensionId& extension_id) const {
  // Construct a key that is guaranteed to be smaller than any key given a
  // |render_process_id|. This facilitates the usage of lower_bound to achieve
  // lg(n) runtime.
  WorkerId lowest_id{extension_id, kSmallestRenderProcessId, kSmallestVersionId,
                     kSmallestThreadId};
  auto begin_range = workers_.lower_bound(lowest_id);
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
  for (const auto& worker_id : workers_) {
    if (worker_id.version_id == worker_version_id &&
        worker_id.extension_id == extension_id) {
      worker_ids.push_back(worker_id);
    }
  }
  return worker_ids;
}

bool WorkerIdSet::Contains(const WorkerId& worker_id) const {
  return workers_.count(worker_id) != 0;
}

std::vector<WorkerId> WorkerIdSet::GetAllForExtension(
    const ExtensionId& extension_id,
    int render_process_id) const {
  // See other GetAllForExtension's notes for |id| construction.
  WorkerId id{extension_id, render_process_id, kSmallestVersionId,
              kSmallestThreadId};

  auto begin_range = workers_.lower_bound(id);
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

namespace debug {

namespace {

const char* BoolToCrashKeyValue(bool value) {
  return value ? "yes" : "no";
}

std::string GetVersionIdValue(int64_t version_id) {
  return base::NumberToString(version_id);
}

base::debug::CrashKeyString* GetExtensionIdCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "multi_ext_worker_extension_id", base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetIdenticalVersionIdsCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "multi_ext_worker_identical_version_ids",
      base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetPreviousWorkerVersionIdCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "multi_ext_worker_prev_version_id", base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetNewWorkerVersionIdCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "multi_ext_worker_new_version_id", base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetPreviousWorkerLifecycleStateCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "multi_ext_worker_prev_lifecycle_state",
      base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetNewWorkerLifecycleStateCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "multi_ext_worker_new_lifecycle_state",
      base::debug::CrashKeySize::Size32);
  return crash_key;
}

const char* GetLifecycleStateValue(const WorkerId& worker_id,
                                   content::BrowserContext* context) {
  switch (GetWorkerLifecycleState(worker_id, context)) {
    case content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
        kUnknown:
      return "kUnknown";
    case content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::kNew:
      return "kNew";
    case content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
        kInstalling:
      return "kInstalling";
    case content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
        kInstalled:
      return "kInstalled";
    case content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
        kActivating:
      return "kActivating";
    case content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
        kActivated:
      return "kActivated";
    case content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
        kRedundant:
      return "kRedundant";
  }
}

base::debug::CrashKeyString* GetIdenticalRendererProcessesIdsCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "multi_ext_worker_identical_process_ids",
      base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetPreviousWorkerRendererProcessRunningCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "multi_ext_worker_prev_process_running",
      base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetNewWorkerRendererProcessRunningCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "multi_ext_worker_new_process_running",
      base::debug::CrashKeySize::Size32);
  return crash_key;
}

const char* GetRendererProcessRunningValue(const ExtensionId& extension_id,
                                           int renderer_process_id,
                                           content::BrowserContext* context) {
  ProcessMap* process_map = ProcessMap::Get(context);
  CHECK(process_map);

  return BoolToCrashKeyValue(
      process_map->Contains(extension_id, renderer_process_id));
}

}  // namespace

ScopedMultiWorkerCrashKeys::ScopedMultiWorkerCrashKeys(
    const ExtensionId& extension_id,
    const WorkerId& previous_worker_id,
    const WorkerId& new_worker_id,
    content::BrowserContext* context)
    : extension_id_crash_key_(GetExtensionIdCrashKey(), extension_id),
      identical_version_ids_crash_key_(
          GetIdenticalVersionIdsCrashKey(),
          BoolToCrashKeyValue(previous_worker_id.version_id ==
                              new_worker_id.version_id)),
      previous_worker_version_id_crash_key_(
          GetPreviousWorkerVersionIdCrashKey(),
          GetVersionIdValue(previous_worker_id.version_id)),
      new_worker_version_id_crash_key_(
          GetNewWorkerVersionIdCrashKey(),
          GetVersionIdValue(new_worker_id.version_id)),
      previous_worker_lifecycle_state_crash_key_(
          GetPreviousWorkerLifecycleStateCrashKey(),
          GetLifecycleStateValue(previous_worker_id, context)),
      new_worker_lifecycle_state_crash_key_(
          GetNewWorkerLifecycleStateCrashKey(),
          GetLifecycleStateValue(new_worker_id, context)),
      identical_worker_render_process_ids_crash_key_(
          GetIdenticalRendererProcessesIdsCrashKey(),
          BoolToCrashKeyValue(previous_worker_id.render_process_id ==
                              new_worker_id.render_process_id)),
      previous_worker_render_process_running_crash_key_(
          GetPreviousWorkerRendererProcessRunningCrashKey(),
          GetRendererProcessRunningValue(extension_id,
                                         previous_worker_id.render_process_id,
                                         context)),
      new_worker_render_process_running_crash_key_(
          GetNewWorkerRendererProcessRunningCrashKey(),
          GetRendererProcessRunningValue(extension_id,
                                         new_worker_id.render_process_id,
                                         context)) {}

ScopedMultiWorkerCrashKeys::~ScopedMultiWorkerCrashKeys() = default;

}  // namespace debug

}  // namespace extensions

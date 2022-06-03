// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/worker_id_set.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "content/public/common/child_process_host.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace extensions {

namespace {

constexpr int64_t kSmallestVersionId =
    blink::mojom::kInvalidServiceWorkerVersionId;
constexpr int kSmallestThreadId = -1;
constexpr int kSmallestRenderProcessId =
    content::ChildProcessHost::kInvalidUniqueID;

static_assert(kSmallestVersionId < 0,
              "Sentinel version_id must be smaller than any valid version id.");
static_assert(kSmallestThreadId < 0,
              "Sentinel thread_id must be smaller than any valid thread id.");
static_assert(
    kSmallestRenderProcessId < 0,
    "Sentinel render_process_id must be smaller than any valid process id.");

}  // namespace

WorkerIdSet::WorkerIdSet() = default;
WorkerIdSet::~WorkerIdSet() = default;

void WorkerIdSet::Add(const WorkerId& worker_id) {
  workers_.insert(worker_id);
}

bool WorkerIdSet::Remove(const WorkerId& worker_id) {
  return workers_.erase(worker_id) > 0;
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

}  // namespace extensions

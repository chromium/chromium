// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/worker_id.h"

#include <optional>
#include <tuple>

#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace extensions {

WorkerId::WorkerId() = default;

WorkerId::WorkerId(const ExtensionId& extension_id,
                   int render_process_id,
                   int64_t version_id,
                   int thread_id)
    : extension_id(extension_id),
      render_process_id(render_process_id),
      version_id(version_id),
      thread_id(thread_id) {}

WorkerId::WorkerId(const ExtensionId& extension_id,
                   int render_process_id,
                   int64_t version_id,
                   int thread_id,
                   const blink::ServiceWorkerToken& start_token)
    : extension_id(extension_id),
      render_process_id(render_process_id),
      version_id(version_id),
      thread_id(thread_id),
      start_token(start_token) {}

bool WorkerId::operator<(const WorkerId& other) const {
  // Note: The comparison order of |extension_id| and |render_process_id| below
  // is important as we use lower_bound to search for all workers with a
  // specific |extension_id|, and, optionally, |render_process_id| within a
  // collection of WorkerIds in WorkerIdSet.
  return std::tie(extension_id, render_process_id, version_id, thread_id) <
         std::tie(other.extension_id, other.render_process_id, other.version_id,
                  other.thread_id);
}

bool WorkerId::operator==(const WorkerId& other) const {
  return extension_id == other.extension_id &&
         render_process_id == other.render_process_id &&
         version_id == other.version_id && thread_id == other.thread_id;
}

bool WorkerId::operator!=(const WorkerId& other) const {
  return !this->operator==(other);
}

std::ostream& operator<<(std::ostream& out, const WorkerId& id) {
  return out << "WorkerId{extension_id: " << id.extension_id
             << ", process_id: " << id.render_process_id
             << ", service_worker_version_id: " << id.version_id
             << ", thread_id: " << id.thread_id << "}";
}

}  // namespace extensions

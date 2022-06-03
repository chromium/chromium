// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "extensions/browser/service_worker/worker_id.h"

namespace extensions {

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

}  // namespace extensions

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include "base/process/process.h"
#include "base/process/process_handle.h"

#include <vector>

namespace memory_instrumentation {

// static
bool OSMetrics::FillOSMemoryDump(base::ProcessId pid,
                                 mojom::RawOSMemDump* dump) {
  return false;
}

// static
std::vector<mojom::VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    base::ProcessId) {
  // TODO(https://crbug.com/1412528): Implement this.
  NOTIMPLEMENTED();
  return std::vector<mojom::VmRegionPtr>();
}

}  // namespace memory_instrumentation

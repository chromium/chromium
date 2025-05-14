// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include "base/process/process.h"
#include "base/process/process_handle.h"

#include <lib/zx/job.h>
#include <lib/zx/object.h>
#include <lib/zx/process.h>
#include <zircon/limits.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>
#include <vector>

namespace memory_instrumentation {

// static
bool OSMetrics::FillOSMemoryDump(base::ProcessHandle handle,
                                 const MemDumpFlagSet& flags,
                                 mojom::RawOSMemDump* dump) {
  auto info = GetMemoryInfo(handle);
  if (!info.has_value()) {
    return false;
  }

  dump->platform_private_footprint->rss_anon_bytes = info->rss_anon_bytes;
  dump->platform_private_footprint->vm_swap_bytes = info->vm_swap_bytes;
  dump->resident_set_kb =
      base::saturated_cast<uint32_t>(info->resident_set_bytes / 1024);
  return true;
}

// static
std::vector<mojom::VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    base::ProcessHandle) {
  // TODO(crbug.com/40720107): Implement this.
  NOTIMPLEMENTED();
  return std::vector<mojom::VmRegionPtr>();
}

}  // namespace memory_instrumentation

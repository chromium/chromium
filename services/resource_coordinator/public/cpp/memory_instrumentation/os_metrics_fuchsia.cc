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
bool OSMetrics::FillOSMemoryDump(base::ProcessId pid,
                                 mojom::RawOSMemDump* dump) {
  base::Process process = pid == base::kNullProcessId
                              ? base::Process::Current()
                              : base::Process::Open(pid);
  zx::unowned<zx::process> zx_process(process.Handle());
  zx_info_task_stats_t info;
  zx_status_t status = zx_process->get_info(ZX_INFO_TASK_STATS, &info,
                                            sizeof(info), nullptr, nullptr);
  if (status != ZX_OK) {
    return false;
  }

  size_t rss_bytes = info.mem_private_bytes + info.mem_shared_bytes;
  size_t rss_anon_bytes = info.mem_private_bytes;

  dump->resident_set_kb = rss_bytes / 1024;
  dump->platform_private_footprint->rss_anon_bytes = rss_anon_bytes;
  // Fuchsia has no swap.
  dump->platform_private_footprint->vm_swap_bytes = 0;
  return true;
}

// static
std::vector<mojom::VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    base::ProcessId) {
  // TODO(crbug.com/40720107): Implement this.
  NOTIMPLEMENTED();
  return std::vector<mojom::VmRegionPtr>();
}

}  // namespace memory_instrumentation

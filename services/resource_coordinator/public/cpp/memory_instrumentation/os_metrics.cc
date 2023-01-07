// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include "build/build_config.h"

namespace memory_instrumentation {

// static
bool OSMetrics::FillProcessMemoryMaps(base::ProcessId pid,
                                      mojom::MemoryMapOption mmap_option,
                                      mojom::RawOSMemDump* dump) {
  DCHECK_NE(mmap_option, mojom::MemoryMapOption::NONE);

  std::vector<mojom::VmRegionPtr> results;

#if BUILDFLAG(IS_MAC)
  // On macOS, fetching all memory maps is very slow. See
  // https://crbug.com/826913 and https://crbug.com/1035401.
  results = GetProcessModules(pid);
#else
  results = GetProcessMemoryMaps(pid);

#endif

  if (results.empty())
    return false;

  dump->memory_maps = std::move(results);

  return true;
}

}  // namespace memory_instrumentation

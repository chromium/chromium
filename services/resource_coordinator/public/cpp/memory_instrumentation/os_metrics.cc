// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include "build/build_config.h"

namespace memory_instrumentation {

// static
bool OSMetrics::FillProcessMemoryMaps(base::ProcessHandle handle,
                                      mojom::MemoryMapOption mmap_option,
                                      mojom::RawOSMemDump* dump) {
  DCHECK_NE(mmap_option, mojom::MemoryMapOption::NONE);

  std::vector<mojom::VmRegionPtr> results;

#if BUILDFLAG(IS_MAC)
  // On macOS, fetching all memory maps is very slow. See
  // https://crbug.com/826913 and https://crbug.com/1035401.
  results = GetProcessModules(handle);
#else
  results = GetProcessMemoryMaps(handle);

#endif

  if (results.empty())
    return false;

  dump->memory_maps = std::move(results);

  return true;
}

#if !BUILDFLAG(IS_APPLE)
base::expected<base::ProcessMemoryInfo, base::ProcessUsageError>
OSMetrics::GetMemoryInfo(base::ProcessHandle handle) {
  auto process_metrics =
      (handle == base::kNullProcessHandle)
          ? base::ProcessMetrics::CreateCurrentProcessMetrics()
          : base::ProcessMetrics::CreateProcessMetrics(handle);
  return process_metrics->GetMemoryInfo();
}
#endif  // !BUILDFLAG(IS_APPLE)

}  // namespace memory_instrumentation

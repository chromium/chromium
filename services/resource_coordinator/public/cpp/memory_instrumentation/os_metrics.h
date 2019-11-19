// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_

#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/process/process_handle.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace heap_profiling {
FORWARD_DECLARE_TEST(ProfilingJsonExporterTest, MemoryMaps);
}

namespace memory_instrumentation {

class COMPONENT_EXPORT(
    RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION) OSMetrics {
 public:
  static bool FillOSMemoryDump(base::ProcessId pid, mojom::RawOSMemDump* dump);
  static bool FillProcessMemoryMaps(base::ProcessId,
                                    mojom::MemoryMapOption,
                                    mojom::RawOSMemDump*);
  static std::vector<mojom::VmRegionPtr> GetProcessMemoryMaps(base::ProcessId);

#if defined(OS_LINUX) || defined(OS_ANDROID)
  static void SetProcSmapsForTesting(FILE*);
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

 private:
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, ParseProcSmaps);
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, TestWinModuleReading);
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, TestMachOReading);
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, GetMappedAndResidentPages);
  FRIEND_TEST_ALL_PREFIXES(heap_profiling::ProfilingJsonExporterTest,
                           MemoryMaps);

#if defined(OS_MACOSX)
  static std::vector<mojom::VmRegionPtr> GetProcessModules(base::ProcessId);
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
  // Provides information on the dump state of resident pages.
  enum class MappedAndResidentPagesDumpState {
    // Access to /proc/<pid>/pagemap can be denied for android devices running
    // a kernel version < 4.4.
    kAccessPagemapDenied,
    kFailure,
    kSuccess
  };

  // Depends on /proc/self/pagemap to determine mapped and resident pages
  // within bounds (start_address inclusive and end_address exclusive).
  // It does not use mincore() because it only checks to see
  // if the page is in the cache and up to date.
  // mincore() has no guarantee a page has been mapped by the current process.
  // Guaranteed to work on Android.
  static MappedAndResidentPagesDumpState GetMappedAndResidentPages(
      const size_t start_address,
      const size_t end_address,
      std::vector<uint8_t>* accessed_pages_bitmap);

  // TODO(chiniforooshan): move to /base/process/process_metrics_linux.cc after
  // making sure that peak RSS is useful.
  static size_t GetPeakResidentSetSize(base::ProcessId pid);
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_

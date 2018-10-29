// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/process/process_handle.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace heap_profiling {
FORWARD_DECLARE_TEST(ProfilingJsonExporterTest, MemoryMaps);
};

namespace memory_instrumentation {

class COMPONENT_EXPORT(
    RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION) OSMetrics {
 public:
  static bool FillOSMemoryDump(base::ProcessId pid, mojom::RawOSMemDump* dump);
  static bool FillProcessMemoryMaps(base::ProcessId,
                                    mojom::MemoryMapOption,
                                    mojom::RawOSMemDump*);

 private:
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, ParseProcSmaps);
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, TestWinModuleReading);
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, TestMachOReading);
  FRIEND_TEST_ALL_PREFIXES(heap_profiling::ProfilingJsonExporterTest,
                           MemoryMaps);
  static std::vector<mojom::VmRegionPtr> GetProcessMemoryMaps(base::ProcessId);

#if defined(OS_MACOSX)
  static std::vector<mojom::VmRegionPtr> GetProcessModules(base::ProcessId);
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
  static void SetProcSmapsForTesting(FILE*);
#endif  // defined(OS_LINUX)
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_

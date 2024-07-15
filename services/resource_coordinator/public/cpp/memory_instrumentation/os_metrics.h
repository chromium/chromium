// Copyright 2017 The Chromium Authors
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

#if BUILDFLAG(IS_APPLE)
#include <mach/mach.h>
#endif

namespace heap_profiling {
FORWARD_DECLARE_TEST(ProfilingJsonExporterTest, MemoryMaps);
}

namespace memory_instrumentation {

// This class provides synchronous access to memory metrics for a process with a
// given |pid|. These interfaces have platform-specific restrictions:
//  * On Android, due to sandboxing restrictions, processes can only access
//    memory metrics for themselves. Thus |pid| must be equal to getpid().
//  * On Linux, due to sandboxing restrictions, only the privileged browser
//    process has access to memory metrics for sandboxed child processes.
//  * On Fuchsia, due to the API expecting a ProcessId rather than a
//    ProcessHandle, processes can only access memory metrics for themselves or
//    for children of base::GetDefaultJob().
//
// These restrictions mean that any code that wishes to be cross-platform
// compatible cannot synchronously obtain memory metrics for a |pid|. Instead,
// it must use the async MemoryInstrumentation methods.
class COMPONENT_EXPORT(
    RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION) OSMetrics {
 public:
  // Fills |dump| with memory information about |pid|. See class comments for
  // restrictions on |pid|. |dump.platform_private_footprint| must be allocated
  // before calling this function. If |pid| is null, the pid of the current
  // process is used
  static bool FillOSMemoryDump(base::ProcessId pid, mojom::RawOSMemDump* dump);
#if BUILDFLAG(IS_APPLE)
  static bool FillOSMemoryDumpFromTaskPort(mach_port_t task_port,
                                           mojom::RawOSMemDump* dump);
#endif
  static bool FillProcessMemoryMaps(base::ProcessId,
                                    mojom::MemoryMapOption,
                                    mojom::RawOSMemDump*);
  static std::vector<mojom::VmRegionPtr> GetProcessMemoryMaps(base::ProcessId);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  static void SetProcSmapsForTesting(FILE*);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

 private:
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, ParseProcSmaps);
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, TestWinModuleReading);
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, TestMachOReading);
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, GetMappedAndResidentPages);
  FRIEND_TEST_ALL_PREFIXES(heap_profiling::ProfilingJsonExporterTest,
                           MemoryMaps);

#if BUILDFLAG(IS_MAC)
  static std::vector<mojom::VmRegionPtr> GetProcessModules(base::ProcessId);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // Provides information on the dump state of resident pages. These values are
  // written to logs. New enum values can be added, but existing enums must
  // never be renumbered or deleted and reused.
  enum class MappedAndResidentPagesDumpState {
    // Access to /proc/<pid>/pagemap can be denied for android devices running
    // a kernel version < 4.4.
    kAccessPagemapDenied = 0,
    kFailure = 1,
    kSuccess = 2,

    // Must be equal to the greatest among enumeraiton values.
    kMaxValue = kSuccess
  };

  // Fills out a bitmap of memory pages accessed by the current process that are
  // still in pagecache.
  //
  // Depends on /proc/self/pagemap to determine the mapped and resident pages
  // within bounds (|start_address| inclusive and |end_address| exclusive).
  //
  // Does not use mincore() because the latter only reports resident pages. The
  // mincore() would report a page as resident if that page was accessed from a
  // different process (such as the commonly used prefetch of the native
  // library).
  //
  // Tested only on Android.
  static MappedAndResidentPagesDumpState GetMappedAndResidentPages(
      const size_t start_address,
      const size_t end_address,
      std::vector<uint8_t>* accessed_pages_bitmap);

  // TODO(chiniforooshan): move to /base/process/process_metrics_linux.cc after
  // making sure that peak RSS is useful.
  static size_t GetPeakResidentSetSize(base::ProcessId pid);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_

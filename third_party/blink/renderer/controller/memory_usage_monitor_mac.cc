// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor_mac.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>

#include "base/mac/mac_util.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

// The following code is copied from
// //services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics_mac.cc
// to use task_info API.
namespace {
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
// The |phys_footprint| field was introduced in 10.11.
struct ChromeTaskVMInfo {
  mach_vm_size_t virtual_size;
  integer_t region_count;
  integer_t page_size;
  mach_vm_size_t resident_size;
  mach_vm_size_t resident_size_peak;
  mach_vm_size_t device;
  mach_vm_size_t device_peak;
  mach_vm_size_t internal;
  mach_vm_size_t internal_peak;
  mach_vm_size_t external;
  mach_vm_size_t external_peak;
  mach_vm_size_t reusable;
  mach_vm_size_t reusable_peak;
  mach_vm_size_t purgeable_volatile_pmap;
  mach_vm_size_t purgeable_volatile_resident;
  mach_vm_size_t purgeable_volatile_virtual;
  mach_vm_size_t compressed;
  mach_vm_size_t compressed_peak;
  mach_vm_size_t compressed_lifetime;
  mach_vm_size_t phys_footprint;
};
#else
using ChromeTaskVMInfo = task_vm_info;
#endif  // MAC_OS_X_VERSION_10_11

// Don't simply use sizeof(task_vm_info) / sizeof(natural_t):
// In the 10.15 SDK, this structure is 87 32-bit words long, and in
// mach_types.defs:
//
//   type task_info_t    = array[*:87] of integer_t;
//
// However in the 10.14 SDK, this structure is 42 32-bit words, and in
// mach_types.defs:
//
//   type task_info_t    = array[*:52] of integer_t;
//
// As a result, the 10.15 SDK's task_vm_info won't fit inside the 10.14 SDK's
// task_info_t, so the *rest of the system* (on 10.14 and earlier) can't handle
// calls that request the full 10.15 structure. We have to request a prefix of
// it that 10.14 and earlier can handle by limiting the length we request. The
// rest of the fields just get ignored, but we don't use them anyway.

constexpr mach_msg_type_number_t ChromeTaskVMInfoCount =
    TASK_VM_INFO_REV2_COUNT;

// The count field is in units of natural_t, which is the machine's word size
// (64 bits on all modern machines), but the task_info_t array is in units of
// integer_t, which is 32 bits.
constexpr mach_msg_type_number_t MAX_MIG_SIZE_FOR_1014 =
    52 / (sizeof(natural_t) / sizeof(integer_t));
static_assert(ChromeTaskVMInfoCount <= MAX_MIG_SIZE_FOR_1014,
              "task_vm_info must be small enough for 10.14 MIG interfaces");

static MemoryUsageMonitor* g_instance_for_testing = nullptr;

}  // namespace

// static
MemoryUsageMonitor& MemoryUsageMonitor::Instance() {
  DEFINE_STATIC_LOCAL(MemoryUsageMonitorMac, monitor, ());
  return g_instance_for_testing ? *g_instance_for_testing : monitor;
}

// static
void MemoryUsageMonitor::SetInstanceForTesting(MemoryUsageMonitor* instance) {
  g_instance_for_testing = instance;
}

bool MemoryUsageMonitorMac::CalculateProcessMemoryFootprint(
    uint64_t* private_footprint) {
  // The following code is copied from OSMetrics::FillOSMemoryDump defined in
  // //services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics_mac.cc
  ChromeTaskVMInfo task_vm_info;
  mach_msg_type_number_t count = ChromeTaskVMInfoCount;
  kern_return_t result =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&task_vm_info), &count);
  if (result != KERN_SUCCESS)
    return false;

  uint64_t internal_bytes = task_vm_info.internal;
  uint64_t compressed_bytes = task_vm_info.compressed;
  uint64_t phys_footprint_bytes = 0;

  if (count == ChromeTaskVMInfoCount) {
    phys_footprint_bytes = task_vm_info.phys_footprint;
  }

  // The following code is copied from CalculatePrivateFootprintKB defined in
  // //services/resource_coordinator/memory_instrumentation/queued_request_dispatcher.cc
  if (base::mac::IsAtLeastOS10_12()) {
    *private_footprint = phys_footprint_bytes;
  } else {
    *private_footprint = internal_bytes + compressed_bytes;
  }
  return true;
}

void MemoryUsageMonitorMac::GetProcessMemoryUsage(MemoryUsage& usage) {
  uint64_t private_footprint;
  if (CalculateProcessMemoryFootprint(&private_footprint))
    usage.private_footprint_bytes = static_cast<double>(private_footprint);
}

}  // namespace blink

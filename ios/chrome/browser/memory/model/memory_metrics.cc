// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/memory/model/memory_metrics.h"

#include <mach/mach.h>
#include <mach/task.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/apple/scoped_mach_port.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"

#ifdef ARCH_CPU_64_BITS
#define cr_vm_region vm_region_64
#else
#define cr_vm_region vm_region
#endif

namespace {
// The number of pages returned by host_statistics and vm_region are a count
// of pages of 4096 bytes even when running on arm64 but the constants that
// are exposed (vm_page_size, VM_PAGE_SIZE, host_page_size) are all equals to
// 16384 bytes. So we define our own constant here to convert from page count
// to bytes.
const uint64_t kVMPageSize = 4096;
}

namespace memory_util {

uint64_t GetFreePhysicalBytes() {
  vm_statistics_data_t vmstat;
  mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
  base::apple::ScopedMachSendRight host(mach_host_self());
  kern_return_t result = host_statistics(
      host.get(), HOST_VM_INFO, reinterpret_cast<host_info_t>(&vmstat), &count);
  if (result != KERN_SUCCESS) {
    LOG(ERROR) << "Calling host_statistics failed.";
    return 0;
  }
  return vmstat.free_count * kVMPageSize;
}

uint64_t GetRealMemoryUsedInBytes() {
  task_vm_info task_info_data;
  mach_msg_type_number_t count = sizeof(task_vm_info) / sizeof(natural_t);
  kern_return_t kr =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&task_info_data), &count);
  if (kr != KERN_SUCCESS)
    return 0;

  return task_info_data.resident_size - task_info_data.reusable;
}

uint64_t GetDirtyVMBytes() {
  // Iterate over all VM regions and sum their dirty pages.
  unsigned int total_dirty_pages = 0;
  vm_size_t vm_size = 0;
  kern_return_t result;
  for (vm_address_t address = MACH_VM_MIN_ADDRESS;; address += vm_size) {
    vm_region_extended_info_data_t info;
    mach_msg_type_number_t info_count = VM_REGION_EXTENDED_INFO_COUNT;
    mach_port_t object_name;
    result = cr_vm_region(
        mach_task_self(), &address, &vm_size, VM_REGION_EXTENDED_INFO,
        reinterpret_cast<vm_region_info_t>(&info), &info_count, &object_name);
    if (result == KERN_INVALID_ADDRESS) {
      // The end of the address space has been reached.
      break;
    } else if (result != KERN_SUCCESS) {
      LOG(ERROR) << "Calling vm_region failed with code: " << result;
      break;
    } else {
      total_dirty_pages += info.pages_dirtied;
    }
  }
  return total_dirty_pages * kVMPageSize;
}

uint64_t GetInternalVMBytes() {
  task_vm_info_data_t task_vm_info;
  mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
  kern_return_t result =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&task_vm_info), &count);
  if (result != KERN_SUCCESS) {
    LOG(ERROR) << "Calling task_info failed.";
    return 0;
  }

  return static_cast<uint64_t>(task_vm_info.internal);
}

}  // namespace memory_util

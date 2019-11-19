// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor_android.h"

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include "third_party/blink/public/platform/platform.h"

namespace blink {

namespace {

bool ReadFileContents(int fd, base::span<char> contents) {
  lseek(fd, 0, SEEK_SET);
  int res = read(fd, contents.data(), contents.size() - 1);
  if (res <= 0)
    return false;
  contents.data()[res] = '\0';
  return true;
}

static MemoryUsageMonitor* g_instance_for_testing = nullptr;

}  // namespace

// static
MemoryUsageMonitor& MemoryUsageMonitor::Instance() {
  DEFINE_STATIC_LOCAL(MemoryUsageMonitorAndroid, monitor, ());
  return g_instance_for_testing ? *g_instance_for_testing : monitor;
}

// static
void MemoryUsageMonitor::SetInstanceForTesting(MemoryUsageMonitor* instance) {
  g_instance_for_testing = instance;
}

// Since the measurement is done every second in background, optimizations are
// in place to get just the metrics we need from the proc files. So, this
// calculation exists here instead of using the cross-process memory-infra code.
bool MemoryUsageMonitorAndroid::CalculateProcessMemoryFootprint(
    int statm_fd,
    int status_fd,
    uint64_t* private_footprint,
    uint64_t* swap_footprint,
    uint64_t* vm_size,
    uint64_t* vm_hwm_size) {
  // Get total resident and shared sizes from statm file.
  static size_t page_size = getpagesize();
  uint64_t resident_pages;
  uint64_t shared_pages;
  uint64_t vm_size_pages;
  constexpr uint32_t kMaxLineSize = 4096;
  char line[kMaxLineSize];
  if (!ReadFileContents(statm_fd, line))
    return false;
  int num_scanned = sscanf(line, "%" SCNu64 " %" SCNu64 " %" SCNu64,
                           &vm_size_pages, &resident_pages, &shared_pages);
  if (num_scanned != 3)
    return false;

  // Get swap size from status file. The format is: VmSwap :  10 kB.
  if (!ReadFileContents(status_fd, line))
    return false;
  char* swap_line = strstr(line, "VmSwap");
  if (!swap_line)
    return false;
  num_scanned = sscanf(swap_line, "VmSwap: %" SCNu64 " kB", swap_footprint);
  if (num_scanned != 1)
    return false;

  char* hwm_line = strstr(line, "VmHWM");
  if (!hwm_line)
    return false;
  num_scanned = sscanf(hwm_line, "VmHWM: %" SCNu64 " kB", vm_hwm_size);
  if (num_scanned != 1)
    return false;

  *vm_hwm_size *= 1024;
  *swap_footprint *= 1024;
  *private_footprint =
      (resident_pages - shared_pages) * page_size + *swap_footprint;
  *vm_size = vm_size_pages * page_size;
  return true;
}

void MemoryUsageMonitorAndroid::GetProcessMemoryUsage(MemoryUsage& usage) {
  ResetFileDescriptors();

  if (!statm_fd_.is_valid() || !status_fd_.is_valid())
    return;
  uint64_t private_footprint, swap, vm_size, vm_hwm_size;
  if (CalculateProcessMemoryFootprint(statm_fd_.get(), status_fd_.get(),
                                      &private_footprint, &swap, &vm_size,
                                      &vm_hwm_size)) {
    usage.private_footprint_bytes = static_cast<double>(private_footprint);
    usage.swap_bytes = static_cast<double>(swap);
    usage.vm_size_bytes = static_cast<double>(vm_size);
    usage.peak_resident_bytes = static_cast<double>(vm_hwm_size);
  }
}

void MemoryUsageMonitorAndroid::ResetFileDescriptors() {
  if (file_descriptors_reset_)
    return;
  file_descriptors_reset_ = true;
  // See https://goo.gl/KjWnZP For details about why we read these files from
  // sandboxed renderer. Keep these files open when detection is enabled.
  if (!statm_fd_.is_valid())
    statm_fd_.reset(open("/proc/self/statm", O_RDONLY));
  if (!status_fd_.is_valid())
    status_fd_.reset(open("/proc/self/status", O_RDONLY));
}

void MemoryUsageMonitorAndroid::ReplaceFileDescriptorsForTesting(
    base::File statm_file,
    base::File status_file) {
  statm_fd_.reset(statm_file.TakePlatformFile());
  status_fd_.reset(status_file.TakePlatformFile());
}

}  // namespace blink

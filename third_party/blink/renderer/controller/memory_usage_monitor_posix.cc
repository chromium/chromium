// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor_posix.h"

#include <fcntl.h>
#include <unistd.h>

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

namespace {

bool ReadFileContents(int fd, base::span<char> contents) {
  lseek(fd, 0, SEEK_SET);
  ssize_t res = read(fd, contents.data(), contents.size() - 1);
  if (res <= 0)
    return false;
  contents[res] = '\0';
  return true;
}

static MemoryUsageMonitor* g_instance_for_testing = nullptr;

MemoryUsageMonitorPosix& GetMemoryUsageMonitor() {
  DEFINE_STATIC_LOCAL(MemoryUsageMonitorPosix, monitor, ());
  return monitor;
}

}  // namespace

// static
MemoryUsageMonitor& MemoryUsageMonitor::Instance() {
  return g_instance_for_testing ? *g_instance_for_testing
                                : GetMemoryUsageMonitor();
}

// static
void MemoryUsageMonitor::SetInstanceForTesting(MemoryUsageMonitor* instance) {
  g_instance_for_testing = instance;
}

// Since the measurement is done every second in background, optimizations are
// in place to get just the metrics we need from the proc files. So, this
// calculation exists here instead of using the cross-process memory-infra code.
bool MemoryUsageMonitorPosix::CalculateProcessMemoryFootprint(
    int statm_fd,
    int status_fd,
    uint64_t* private_footprint,
    uint64_t* swap_footprint,
    uint64_t* vm_size,
    uint64_t* vm_hwm_size) {
  // Helper to parse the next whitespace-delimited uint64 from a string_view,
  // advancing past it.
  auto consume_uint64 = [](std::string_view& sv, uint64_t* out) -> bool {
    size_t start = sv.find_first_not_of(" \t");
    if (start == std::string_view::npos) {
      return false;
    }
    sv.remove_prefix(start);
    size_t end = sv.find_first_of(" \t\n");
    if (!base::StringToUint64(sv.substr(0, end), out)) {
      return false;
    }
    sv.remove_prefix(std::min(end, sv.size()));
    return true;
  };

  // Helper to find a field like "VmSwap:  10 kB" and parse its uint64 value.
  auto parse_status_field = [&consume_uint64](std::string_view contents,
                                              std::string_view field_name,
                                              uint64_t* out) -> bool {
    size_t pos = contents.find(field_name);
    if (pos == std::string_view::npos) {
      return false;
    }
    std::string_view rest = contents.substr(pos + field_name.size());
    // Skip past the ":" separator.
    size_t colon = rest.find(':');
    if (colon == std::string_view::npos) {
      return false;
    }
    rest.remove_prefix(colon + 1);
    if (!consume_uint64(rest, out)) {
      return false;
    }
    return rest.starts_with(" kB");
  };

  // Get total resident and shared sizes from statm file.
  static size_t page_size = getpagesize();
  uint64_t resident_pages;
  uint64_t shared_pages;
  uint64_t vm_size_pages;
  constexpr uint32_t kMaxLineSize = 4096;
  char line[kMaxLineSize];
  if (!ReadFileContents(statm_fd, line))
    return false;

  // statm format: "vm_size resident shared ..."
  std::string_view statm_view(line);
  if (!consume_uint64(statm_view, &vm_size_pages) ||
      !consume_uint64(statm_view, &resident_pages) ||
      !consume_uint64(statm_view, &shared_pages)) {
    return false;
  }

  // Get swap size from status file. The format is: VmSwap :  10 kB.
  if (!ReadFileContents(status_fd, line))
    return false;
  std::string_view status_view(line);
  if (!parse_status_field(status_view, "VmSwap", swap_footprint)) {
    return false;
  }
  if (!parse_status_field(status_view, "VmHWM", vm_hwm_size)) {
    return false;
  }

  *vm_hwm_size *= 1024;
  *swap_footprint *= 1024;
  *private_footprint =
      (resident_pages - shared_pages) * page_size + *swap_footprint;
  *vm_size = vm_size_pages * page_size;
  return true;
}

void MemoryUsageMonitorPosix::GetProcessMemoryUsage(MemoryUsage& usage) {
#if BUILDFLAG(IS_ANDROID)
  ResetFileDescriptors();
#endif
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

#if BUILDFLAG(IS_ANDROID)
void MemoryUsageMonitorPosix::ResetFileDescriptors() {
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
#endif

void MemoryUsageMonitorPosix::SetProcFiles(base::File statm_file,
                                           base::File status_file) {
  DCHECK(statm_file.IsValid());
  DCHECK(status_file.IsValid());
  DCHECK_EQ(-1, statm_fd_.get());
  DCHECK_EQ(-1, status_fd_.get());
  statm_fd_.reset(statm_file.TakePlatformFile());
  status_fd_.reset(status_file.TakePlatformFile());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// static
void MemoryUsageMonitorPosix::Bind(
    mojo::PendingReceiver<mojom::blink::MemoryUsageMonitorLinux> receiver) {
  // This should be called only once per process on RenderProcessWillLaunch.
  DCHECK(!GetMemoryUsageMonitor().receiver_.is_bound());
  GetMemoryUsageMonitor().receiver_.Bind(std::move(receiver));
}
#endif

}  // namespace blink

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_discardable_manager.h"

#include <inttypes.h>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"

namespace gpu {

size_t DiscardableCacheSizeLimit() {
// Cache size values are designed to roughly correspond to existing image cache
// sizes for 1-1.5 renderers. These will be updated as more types of data are
// moved to this cache.
#if BUILDFLAG(IS_ANDROID)
  const size_t kLowEndCacheSizeBytes = 1024 * 1024;
  const size_t kNormalCacheSizeBytes = 128 * 1024 * 1024;
#else
  const size_t kNormalCacheSizeBytes = 192 * 1024 * 1024;
  const size_t kLargeCacheSizeBytes = 256 * 1024 * 1024;
  // Device ram threshold at which we move from a normal cache to a large cache.
  // While this is a GPU memory cache, we can't read GPU memory reliably, so we
  // use system ram as a proxy.
  constexpr base::ByteCount kLargeCacheSizeMemoryThreshold = base::GiB(4);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  if (base::SysInfo::IsLowEndDevice()) {
    return kLowEndCacheSizeBytes;
  } else {
    return kNormalCacheSizeBytes;
  }
#else
  if (base::SysInfo::AmountOfPhysicalMemory() <
      kLargeCacheSizeMemoryThreshold) {
    return kNormalCacheSizeBytes;
  } else {
    return kLargeCacheSizeBytes;
  }
#endif
}

size_t DiscardableCacheSizeLimitForPressure(
    size_t base_cache_limit,
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      return base_cache_limit;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      // With moderate pressure, shrink to 1/4 our normal size.
      return base_cache_limit / 4;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // With critical pressure, purge as much as possible.
      return 0;

    default:
      NOTREACHED();
  }
}

ServiceDiscardableManager::GpuDiscardableEntry::GpuDiscardableEntry(
    ServiceDiscardableHandle handle,
    size_t size)
    : handle(handle), size(size) {}
ServiceDiscardableManager::GpuDiscardableEntry::GpuDiscardableEntry(
    const GpuDiscardableEntry& other) = default;
ServiceDiscardableManager::GpuDiscardableEntry::GpuDiscardableEntry(
    GpuDiscardableEntry&& other) = default;
ServiceDiscardableManager::GpuDiscardableEntry::~GpuDiscardableEntry() =
    default;

ServiceDiscardableManager::ServiceDiscardableManager(
    const GpuPreferences& preferences) {
  // In certain cases, SingleThreadTaskRunner::CurrentDefaultHandle isn't set
  // (Android Webview).  Don't register a dump provider in these cases.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "gpu::ServiceDiscardableManager",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

ServiceDiscardableManager::~ServiceDiscardableManager() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool ServiceDiscardableManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;

  if (args.level_of_detail == MemoryDumpLevelOfDetail::kBackground) {
    std::string dump_name =
        base::StringPrintf("gpu/discardable_cache/cache_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(this));
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, 0);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  return true;
}

}  // namespace gpu

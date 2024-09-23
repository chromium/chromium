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
  const int kLargeCacheSizeMemoryThresholdMB = 4 * 1024;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  if (base::SysInfo::IsLowEndDevice()) {
    return kLowEndCacheSizeBytes;
  } else {
    return kNormalCacheSizeBytes;
  }
#else
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <
      kLargeCacheSizeMemoryThresholdMB) {
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
      NOTREACHED_IN_MIGRATION();
      return 0;
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
    const GpuPreferences& preferences)
    : entries_(EntryCache::NO_AUTO_EVICT),
      cache_size_limit_(preferences.force_gpu_mem_discardable_limit_bytes
                            ? preferences.force_gpu_mem_discardable_limit_bytes
                            : DiscardableCacheSizeLimit()) {
  // In certain cases, SingleThreadTaskRunner::CurrentDefaultHandle isn't set
  // (Android Webview).  Don't register a dump provider in these cases.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "gpu::ServiceDiscardableManager",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

ServiceDiscardableManager::~ServiceDiscardableManager() {
#if DCHECK_IS_ON()
  for (const auto& entry : entries_) {
    DCHECK(nullptr == entry.second.unlocked_texture_ref);
  }
#endif
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
                    MemoryAllocatorDump::kUnitsBytes, total_size_);

    if (!entries_.empty()) {
      MemoryAllocatorDump* dump_avg_size =
          pmd->CreateAllocatorDump(dump_name + "/avg_image_size");
      dump_avg_size->AddScalar("average_size", MemoryAllocatorDump::kUnitsBytes,
                               total_size_ / entries_.size());
    }

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (const auto& entry : entries_) {
    std::string dump_name = base::StringPrintf(
        "gpu/discardable_cache/cache_0x%" PRIXPTR "/entry_0x%" PRIXPTR,
        reinterpret_cast<uintptr_t>(this),
        reinterpret_cast<uintptr_t>(entry.second.unlocked_texture_ref.get()));
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, entry.second.size);
  }

  return true;
}

void ServiceDiscardableManager::InsertLockedTexture(
    uint32_t texture_id,
    size_t texture_size,
    gles2::TextureManager* texture_manager,
    ServiceDiscardableHandle handle) {
  auto found = entries_.Get({texture_id, texture_manager});
  if (found != entries_.end()) {
    // We have somehow initialized a texture twice. The client *shouldn't* send
    // this command, but if it does, we will clean up the old entry and use
    // the new one.
    total_size_ -= found->second.size;
    if (found->second.unlocked_texture_ref) {
      texture_manager->ReturnTexture(
          std::move(found->second.unlocked_texture_ref));
    }
    entries_.Erase(found);
  }

  total_size_ += texture_size;
  entries_.Put(GpuDiscardableEntryKey{texture_id, texture_manager},
               GpuDiscardableEntry{handle, texture_size});
  EnforceCacheSizeLimit(cache_size_limit_);
}

bool ServiceDiscardableManager::UnlockTexture(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager,
    gles2::TextureRef** texture_to_unbind) {
  *texture_to_unbind = nullptr;

  auto found = entries_.Get({texture_id, texture_manager});
  if (found == entries_.end())
    return false;

  found->second.handle.Unlock();
  if (--found->second.service_ref_count_ == 0) {
    found->second.unlocked_texture_ref =
        texture_manager->TakeTexture(texture_id);
    *texture_to_unbind = found->second.unlocked_texture_ref.get();
  }

  return true;
}

bool ServiceDiscardableManager::LockTexture(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager) {
  auto found = entries_.Peek({texture_id, texture_manager});
  if (found == entries_.end())
    return false;

  ++found->second.service_ref_count_;
  if (found->second.unlocked_texture_ref) {
    texture_manager->ReturnTexture(
        std::move(found->second.unlocked_texture_ref));
  }

  return true;
}

void ServiceDiscardableManager::OnTextureManagerDestruction(
    gles2::TextureManager* texture_manager) {
  for (auto& entry : entries_) {
    if (entry.first.texture_manager == texture_manager &&
        entry.second.unlocked_texture_ref) {
      texture_manager->ReturnTexture(
          std::move(entry.second.unlocked_texture_ref));
    }
  }
}

void ServiceDiscardableManager::OnTextureDeleted(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager) {
  auto found = entries_.Get({texture_id, texture_manager});
  if (found == entries_.end())
    return;

  found->second.handle.ForceDelete();
  total_size_ -= found->second.size;
  entries_.Erase(found);
}

void ServiceDiscardableManager::OnContextLost() {
  auto iter = entries_.begin();
  while (iter != entries_.end()) {
    iter->second.handle.ForceDelete();
    if (iter->second.unlocked_texture_ref)
      iter->second.unlocked_texture_ref->ForceContextLost();

    total_size_ -= iter->second.size;
    iter = entries_.Erase(iter);
  }
}

void ServiceDiscardableManager::OnTextureSizeChanged(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager,
    size_t new_size) {
  auto found = entries_.Get({texture_id, texture_manager});
  if (found == entries_.end())
    return;

  total_size_ -= found->second.size;
  found->second.size = new_size;
  total_size_ += found->second.size;

  EnforceCacheSizeLimit(cache_size_limit_);
}

void ServiceDiscardableManager::HandleMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  size_t limit = DiscardableCacheSizeLimitForPressure(cache_size_limit_,
                                                      memory_pressure_level);
  EnforceCacheSizeLimit(limit);
}

void ServiceDiscardableManager::EnforceCacheSizeLimit(size_t limit) {
  for (auto it = entries_.rbegin(); it != entries_.rend();) {
    if (total_size_ <= limit) {
      return;
    }
    if (!it->second.handle.Delete()) {
      ++it;
      continue;
    }

    total_size_ -= it->second.size;

    gles2::TextureManager* texture_manager = it->first.texture_manager;
    uint32_t texture_id = it->first.texture_id;

    // While unlocked, we hold the texture ref. Return this to the texture
    // manager for cleanup.
    texture_manager->ReturnTexture(std::move(it->second.unlocked_texture_ref));

    // Erase before calling texture_manager->RemoveTexture, to avoid attempting
    // to remove the texture from entries_ twice.
    it = entries_.Erase(it);
    texture_manager->RemoveTexture(texture_id);
  }
}

bool ServiceDiscardableManager::IsEntryLockedForTesting(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager) const {
  auto found = entries_.Peek({texture_id, texture_manager});
  CHECK(found != entries_.end());

  return found->second.handle.IsLockedForTesting();
}

gles2::TextureRef* ServiceDiscardableManager::UnlockedTextureRefForTesting(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager) const {
  auto found = entries_.Peek({texture_id, texture_manager});
  CHECK(found != entries_.end());

  return found->second.unlocked_texture_ref.get();
}

}  // namespace gpu

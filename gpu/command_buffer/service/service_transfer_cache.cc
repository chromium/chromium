// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_transfer_cache.h"

#include <inttypes.h>

#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/trace_util.h"

namespace gpu {
namespace {

// Put an arbitrary (high) limit on number of cache entries to prevent
// unbounded handle growth with tiny entries.
static size_t kMaxCacheEntries = 2000;

// Alias the image entry to its skia counterpart, taking ownership of the
// memory and preventing double counting.
//
// TODO(ericrk): Move this into ServiceImageTransferCacheEntry - here for now
// due to ui/gl dependency.
void DumpMemoryForImageTransferCacheEntry(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& dump_name,
    const cc::ServiceImageTransferCacheEntry* entry) {
  using base::trace_event::MemoryAllocatorDump;
  DCHECK(entry->image());

  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, entry->CachedSize());

  GrBackendTexture image_backend_texture =
      entry->image()->getBackendTexture(false /* flushPendingGrContextIO */);
  GrGLTextureInfo info;
  if (image_backend_texture.getGLTextureInfo(&info)) {
    auto guid = gl::GetGLTextureRasterGUIDForTracing(info.fID);
    pmd->CreateSharedGlobalAllocatorDump(guid);
    // Importance of 3 gives this dump priority over the dump made by Skia
    // (importance 2), attributing memory here.
    const int kImportance = 3;
    pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
  }
}

// Alias each texture of the YUV image entry to its Skia texture counterpart,
// taking ownership of the memory and preventing double counting.
//
// Because hardware-decoded images do not have knowledge of the individual plane
// sizes, we allow |plane_sizes| to be empty and report the aggregate size for
// plane_0 and give plane_1 and plane_2 size 0.
//
// TODO(ericrk): Move this into ServiceImageTransferCacheEntry - here for now
// due to ui/gl dependency.
void DumpMemoryForYUVImageTransferCacheEntry(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& dump_base_name,
    const cc::ServiceImageTransferCacheEntry* entry) {
  using base::trace_event::MemoryAllocatorDump;
  DCHECK(entry->image());
  DCHECK(entry->is_yuv());

  std::vector<size_t> plane_sizes = entry->GetPlaneCachedSizes();
  for (size_t i = 0u; i < entry->num_planes(); ++i) {
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
        dump_base_name +
        base::StringPrintf("/plane_%0u", base::checked_cast<uint32_t>(i)));
    if (plane_sizes.empty()) {
      // Hardware-decoded image case.
      dump->AddScalar(MemoryAllocatorDump::kNameSize,
                      MemoryAllocatorDump::kUnitsBytes,
                      (i == SkYUVAIndex::kY_Index) ? entry->CachedSize() : 0u);
    } else {
      DCHECK_EQ(plane_sizes.size(), entry->num_planes());
      dump->AddScalar(MemoryAllocatorDump::kNameSize,
                      MemoryAllocatorDump::kUnitsBytes, plane_sizes.at(i));
    }

    // If entry->image() is backed by multiple textures,
    // getBackendTexture() would end up flattening them to RGB, which is
    // undesirable.
    GrBackendTexture image_backend_texture =
        entry->GetPlaneImage(i)->getBackendTexture(
            false /* flushPendingGrContextIO */);
    GrGLTextureInfo info;
    if (image_backend_texture.getGLTextureInfo(&info)) {
      auto guid = gl::GetGLTextureRasterGUIDForTracing(info.fID);
      pmd->CreateSharedGlobalAllocatorDump(guid);
      // Importance of 3 gives this dump priority over the dump made by Skia
      // (importance 2), attributing memory here.
      const int kImportance = 3;
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }
}

}  // namespace

ServiceTransferCache::CacheEntryInternal::CacheEntryInternal(
    base::Optional<ServiceDiscardableHandle> handle,
    std::unique_ptr<cc::ServiceTransferCacheEntry> entry)
    : handle(handle), entry(std::move(entry)) {}

ServiceTransferCache::CacheEntryInternal::~CacheEntryInternal() {}

ServiceTransferCache::CacheEntryInternal::CacheEntryInternal(
    CacheEntryInternal&& other) = default;

ServiceTransferCache::CacheEntryInternal&
ServiceTransferCache::CacheEntryInternal::operator=(
    CacheEntryInternal&& other) = default;

ServiceTransferCache::ServiceTransferCache()
    : entries_(EntryCache::NO_AUTO_EVICT),
      cache_size_limit_(DiscardableCacheSizeLimit()),
      max_cache_entries_(kMaxCacheEntries) {
  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "gpu::ServiceTransferCache", base::ThreadTaskRunnerHandle::Get());
  }
}

ServiceTransferCache::~ServiceTransferCache() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool ServiceTransferCache::CreateLockedEntry(const EntryKey& key,
                                             ServiceDiscardableHandle handle,
                                             GrContext* context,
                                             base::span<uint8_t> data) {
  auto found = entries_.Peek(key);
  if (found != entries_.end())
    return false;

  std::unique_ptr<cc::ServiceTransferCacheEntry> entry =
      cc::ServiceTransferCacheEntry::Create(key.entry_type);
  if (!entry)
    return false;

  if (!entry->Deserialize(context, data))
    return false;

  total_size_ += entry->CachedSize();
  if (key.entry_type == cc::TransferCacheEntryType::kImage) {
    total_image_count_++;
    total_image_size_ += entry->CachedSize();
  }
  entries_.Put(key, CacheEntryInternal(handle, std::move(entry)));
  EnforceLimits();
  return true;
}

void ServiceTransferCache::CreateLocalEntry(
    const EntryKey& key,
    std::unique_ptr<cc::ServiceTransferCacheEntry> entry) {
  if (!entry)
    return;

  DCHECK_EQ(entry->Type(), key.entry_type);
  DeleteEntry(key);

  total_size_ += entry->CachedSize();
  if (key.entry_type == cc::TransferCacheEntryType::kImage) {
    total_image_count_++;
    total_image_size_ += entry->CachedSize();
  }

  entries_.Put(key, CacheEntryInternal(base::nullopt, std::move(entry)));
  EnforceLimits();
}

bool ServiceTransferCache::UnlockEntry(const EntryKey& key) {
  auto found = entries_.Peek(key);
  if (found == entries_.end())
    return false;

  if (!found->second.handle)
    return false;
  found->second.handle->Unlock();
  return true;
}

template <typename Iterator>
Iterator ServiceTransferCache::ForceDeleteEntry(Iterator it) {
  if (it->second.handle)
    it->second.handle->ForceDelete();

  DCHECK_GE(total_size_, it->second.entry->CachedSize());
  total_size_ -= it->second.entry->CachedSize();
  if (it->first.entry_type == cc::TransferCacheEntryType::kImage) {
    total_image_count_--;
    total_image_size_ -= it->second.entry->CachedSize();
  }
  return entries_.Erase(it);
}

bool ServiceTransferCache::DeleteEntry(const EntryKey& key) {
  auto found = entries_.Peek(key);
  if (found == entries_.end())
    return false;

  ForceDeleteEntry(found);
  return true;
}

cc::ServiceTransferCacheEntry* ServiceTransferCache::GetEntry(
    const EntryKey& key) {
  auto found = entries_.Get(key);
  if (found == entries_.end())
    return nullptr;
  return found->second.entry.get();
}

void ServiceTransferCache::EnforceLimits() {
  for (auto it = entries_.rbegin(); it != entries_.rend();) {
    if (total_size_ <= cache_size_limit_ &&
        entries_.size() <= max_cache_entries_) {
      return;
    }
    if (it->second.handle && !it->second.handle->Delete()) {
      ++it;
      continue;
    }

    total_size_ -= it->second.entry->CachedSize();
    if (it->first.entry_type == cc::TransferCacheEntryType::kImage) {
      total_image_count_--;
      total_image_size_ -= it->second.entry->CachedSize();
    }
    it = entries_.Erase(it);
  }
}

void ServiceTransferCache::PurgeMemory(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      // This function is only called with moderate or critical pressure.
      NOTREACHED();
      return;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      cache_size_limit_ = cache_size_limit_ / 4;
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      cache_size_limit_ = 0u;
      break;
  }

  EnforceLimits();
  cache_size_limit_ = DiscardableCacheSizeLimit();
}

void ServiceTransferCache::DeleteAllEntriesForDecoder(int decoder_id) {
  for (auto it = entries_.rbegin(); it != entries_.rend();) {
    if (it->first.decoder_id != decoder_id) {
      ++it;
      continue;
    }
    it = ForceDeleteEntry(it);
  }
}

bool ServiceTransferCache::CreateLockedHardwareDecodedImageEntry(
    int decoder_id,
    uint32_t entry_id,
    ServiceDiscardableHandle handle,
    GrContext* context,
    std::vector<sk_sp<SkImage>> plane_images,
    cc::YUVDecodeFormat plane_images_format,
    SkYUVColorSpace yuv_color_space,
    size_t buffer_byte_size,
    bool needs_mips) {
  EntryKey key(decoder_id, cc::TransferCacheEntryType::kImage, entry_id);
  auto found = entries_.Peek(key);
  if (found != entries_.end())
    return false;

  // Create the service-side image transfer cache entry.
  auto entry = std::make_unique<cc::ServiceImageTransferCacheEntry>();
  if (!entry->BuildFromHardwareDecodedImage(
          context, std::move(plane_images), plane_images_format,
          yuv_color_space, buffer_byte_size, needs_mips)) {
    return false;
  }

  // Insert it in the transfer cache.
  total_size_ += entry->CachedSize();
  if (key.entry_type == cc::TransferCacheEntryType::kImage) {
    total_image_count_++;
    total_image_size_ += entry->CachedSize();
  }
  entries_.Put(key, CacheEntryInternal(handle, std::move(entry)));
  EnforceLimits();
  return true;
}

bool ServiceTransferCache::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;

  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name =
        base::StringPrintf("gpu/transfer_cache/cache_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(this));
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, total_image_size_);

    if (total_image_count_ > 0) {
      MemoryAllocatorDump* dump_avg_size =
          pmd->CreateAllocatorDump(dump_name + "/avg_image_size");
      const size_t avg_image_size =
          total_image_size_ / (total_image_count_ * 1.0);
      dump_avg_size->AddScalar("average_size", MemoryAllocatorDump::kUnitsBytes,
                               avg_image_size);
    }

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (auto it = entries_.begin(); it != entries_.end(); it++) {
    auto entry_type = it->first.entry_type;
    const auto* entry = it->second.entry.get();
    const cc::ServiceImageTransferCacheEntry* image_entry = nullptr;

    if (entry_type == cc::TransferCacheEntryType::kImage) {
      image_entry =
          static_cast<const cc::ServiceImageTransferCacheEntry*>(entry);
    }

    if (image_entry && image_entry->fits_on_gpu()) {
      std::string dump_base_name = base::StringPrintf(
          "gpu/transfer_cache/cache_0x%" PRIXPTR "/gpu/entry_0x%" PRIXPTR,
          reinterpret_cast<uintptr_t>(this),
          reinterpret_cast<uintptr_t>(entry));
      if (image_entry->is_yuv()) {
        DumpMemoryForYUVImageTransferCacheEntry(pmd, dump_base_name,
                                                image_entry);
      } else {
        DumpMemoryForImageTransferCacheEntry(pmd, dump_base_name, image_entry);
      }
    } else {
      std::string dump_name = base::StringPrintf(
          "gpu/transfer_cache/cache_0x%" PRIXPTR "/cpu/entry_0x%" PRIXPTR,
          reinterpret_cast<uintptr_t>(this),
          reinterpret_cast<uintptr_t>(entry));
      MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
      dump->AddScalar(MemoryAllocatorDump::kNameSize,
                      MemoryAllocatorDump::kUnitsBytes, entry->CachedSize());
    }
  }

  return true;
}

ServiceTransferCache::EntryKey::EntryKey(int decoder_id,
                                         cc::TransferCacheEntryType entry_type,
                                         uint32_t entry_id)
    : decoder_id(decoder_id), entry_type(entry_type), entry_id(entry_id) {}

}  // namespace gpu

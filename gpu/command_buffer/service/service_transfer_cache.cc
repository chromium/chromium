// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_transfer_cache.h"

#include <inttypes.h>

#include <utility>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gl/trace_util.h"

namespace gpu {
namespace {

// Put an arbitrary (high) limit on number of cache entries to prevent
// unbounded handle growth with tiny entries.
static size_t kMaxCacheEntries = 2000;

constexpr base::TimeDelta kOldEntryCutoffTimeDelta = base::Seconds(25);
constexpr base::TimeDelta kOldEntryPruneInterval = base::Seconds(30);

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

  GrBackendTexture image_backend_texture;
  if (SkImages::GetBackendTextureFromImage(
          entry->image(), &image_backend_texture,
          false /* flushPendingGrContextIO */)) {
    GrGLTextureInfo info;
    if (GrBackendTextures::GetGLTextureInfo(image_backend_texture, &info)) {
      auto guid = gl::GetGLTextureRasterGUIDForTracing(info.fID);
      pmd->CreateSharedGlobalAllocatorDump(guid);
      // Importance of 3 gives this dump priority over the dump made by Skia
      // (importance 2), attributing memory here.
      const int kImportance = 3;
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
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
  if (plane_sizes.empty()) {
    // This entry corresponds to an unmipped hardware decoded image.
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
        dump_base_name + base::StringPrintf("/dma_buf"));
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, entry->CachedSize());
    // We don't need to establish shared ownership of the dump with Skia: the
    // reason is that Skia doesn't own the textures for hardware decoded images,
    // so it won't count them in its memory dump (because
    // SkiaGpuTraceMemoryDump::shouldDumpWrappedObjects() returns false).
    return;
  }

  for (size_t i = 0u; i < entry->num_planes(); ++i) {
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
        dump_base_name +
        base::StringPrintf("/plane_%0u", base::checked_cast<uint32_t>(i)));
    DCHECK_EQ(plane_sizes.size(), entry->num_planes());
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, plane_sizes.at(i));

    // If entry->image() is backed by multiple textures,
    // getBackendTexture() would end up flattening them to RGB, which is
    // undesirable.
    GrBackendTexture image_backend_texture;
    if (SkImages::GetBackendTextureFromImage(
            entry->GetPlaneImage(i), &image_backend_texture,
            false /* flushPendingGrContextIO */)) {
      GrGLTextureInfo info;
      if (GrBackendTextures::GetGLTextureInfo(image_backend_texture, &info)) {
        auto guid = gl::GetGLTextureRasterGUIDForTracing(info.fID);
        pmd->CreateSharedGlobalAllocatorDump(guid);
        // Importance of 3 gives this dump priority over the dump made by Skia
        // (importance 2), attributing memory here.
        const int kImportance = 3;
        pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
      }
    }
  }
}

}  // namespace

ServiceTransferCache::CacheEntryInternal::CacheEntryInternal(
    std::optional<ServiceDiscardableHandle> handle,
    std::unique_ptr<cc::ServiceTransferCacheEntry> entry)
    : handle(handle), entry(std::move(entry)) {}

ServiceTransferCache::CacheEntryInternal::~CacheEntryInternal() {
  if (entry) {
    UMA_HISTOGRAM_COUNTS_1M("GPU.TransferCache.ReusedTimes", num_reuse);
    UMA_HISTOGRAM_LONG_TIMES("GPU.TransferCache.TimeSinceLastUseOnDelete",
                             base::TimeTicks::Now() - last_use);
  }
}

ServiceTransferCache::CacheEntryInternal::CacheEntryInternal(
    CacheEntryInternal&& other) = default;

ServiceTransferCache::CacheEntryInternal&
ServiceTransferCache::CacheEntryInternal::operator=(
    CacheEntryInternal&& other) = default;

ServiceTransferCache::ServiceTransferCache(
    const GpuPreferences& preferences,
    base::RepeatingClosure flush_callback)
    : flush_callback_(std::move(flush_callback)),
      entries_(EntryCache::NO_AUTO_EVICT),
      cache_size_limit_(preferences.force_gpu_mem_discardable_limit_bytes
                            ? preferences.force_gpu_mem_discardable_limit_bytes
                            : DiscardableCacheSizeLimit()),
      max_cache_entries_(kMaxCacheEntries) {
  // In certain cases, SingleThreadTaskRunner::CurrentDefaultHandle isn't set
  // (Android Webview).  Don't register a dump provider in these cases.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "gpu::ServiceTransferCache",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

ServiceTransferCache::~ServiceTransferCache() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool ServiceTransferCache::CreateLockedEntry(
    const EntryKey& key,
    ServiceDiscardableHandle handle,
    GrDirectContext* context,
    skgpu::graphite::Recorder* graphite_recorder,
    base::span<uint8_t> data) {
  auto found = entries_.Peek(key);
  if (found != entries_.end()) {
    return false;
  }

  std::unique_ptr<cc::ServiceTransferCacheEntry> entry =
      cc::ServiceTransferCacheEntry::Create(key.entry_type);
  if (!entry) {
    return false;
  }

  if (!entry->Deserialize(context, graphite_recorder, data)) {
    return false;
  }

  total_size_ += entry->CachedSize();
  if (key.entry_type == cc::TransferCacheEntryType::kImage) {
    total_image_count_++;
    total_image_size_ += entry->CachedSize();
  }
  entries_.Put(key, CacheEntryInternal(handle, std::move(entry)));
  EnforceLimits();
  MaybePostPruneOldEntries();
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

  entries_.Put(key, CacheEntryInternal(std::nullopt, std::move(entry)));
  EnforceLimits();
  MaybePostPruneOldEntries();
}

bool ServiceTransferCache::UnlockEntry(const EntryKey& key) {
  auto found = entries_.Peek(key);
  if (found == entries_.end())
    return false;

  if (!found->second.handle)
    return false;
  found->second.handle->Unlock();
  MaybePostPruneOldEntries();
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
  auto entry = entries_.Get(key);
  bool found = entry != entries_.end();
  UMA_HISTOGRAM_BOOLEAN("GPU.TransferCache.EntryFound", found);
  if (!found) {
    return nullptr;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta last_use_delta = now - entry->second.last_use;
  if (last_use_delta > entry->second.max_last_use_delta) {
    entry->second.max_last_use_delta = last_use_delta;
  }
  entry->second.last_use = now;
  entry->second.num_reuse++;
  UMA_HISTOGRAM_LONG_TIMES("GPU.TransferCache.TimeSinceLastUse",
                           last_use_delta);
  UMA_HISTOGRAM_LONG_TIMES("GPU.TransferCache.MaxHistoricalTimeSinceLastUse",
                           entry->second.max_last_use_delta);
  return entry->second.entry.get();
}

void ServiceTransferCache::EnforceLimits() {
  RemoveOldEntriesUntil([&](EntryCache::reverse_iterator it) {
    return total_size_ <= cache_size_limit_ &&
           entries_.size() <= max_cache_entries_;
  });
}

void ServiceTransferCache::MaybePostPruneOldEntries() {
  if (!features::EnablePruneOldTransferCacheEntries()) {
    return;
  }
  if (!base::SingleThreadTaskRunner::HasCurrentDefault()) {
    // No task runner in unit tests.
    return;
  }

  if (prune_old_entries_timer_.IsRunning()) {
    request_post_prune_old_entries_while_pending_ = true;
    return;
  }
  prune_old_entries_timer_.Start(FROM_HERE, kOldEntryPruneInterval, this,
                                 &ServiceTransferCache::PruneOldEntries);
}

void ServiceTransferCache::PruneOldEntries() {
  base::TimeTicks now = base::TimeTicks::Now();

  int removed_count =
      RemoveOldEntriesUntil([&](EntryCache::reverse_iterator it) {
        return now - it->second.last_use < kOldEntryCutoffTimeDelta;
      });
  if (removed_count && flush_callback_) {
    flush_callback_.Run();
  }

  if (request_post_prune_old_entries_while_pending_) {
    request_post_prune_old_entries_while_pending_ = false;
    MaybePostPruneOldEntries();
  }
}

int ServiceTransferCache::RemoveOldEntriesUntil(
    base::FunctionRef<bool(EntryCache::reverse_iterator)> should_stop) {
  int removed_count = 0;
  for (auto it = entries_.rbegin(); it != entries_.rend();) {
    if (should_stop(it)) {
      break;
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
    removed_count++;
  }
  return removed_count;
}

void ServiceTransferCache::PurgeMemory(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  base::AutoReset<size_t> reset_limit(
      &cache_size_limit_, DiscardableCacheSizeLimitForPressure(
                              cache_size_limit_, memory_pressure_level));
  EnforceLimits();
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
    GrDirectContext* context,
    std::vector<sk_sp<SkImage>> plane_images,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
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
          context, std::move(plane_images), plane_config, subsampling,
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
  MaybePostPruneOldEntries();
  return true;
}

bool ServiceTransferCache::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;

  if (args.level_of_detail == MemoryDumpLevelOfDetail::kBackground) {
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

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/gr_shader_cache.h"

#include <inttypes.h>

#include "base/auto_reset.h"
#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

namespace gpu {
namespace raster {
namespace {

std::string MakeString(const SkData* data) {
  return std::string(static_cast<const char*>(data->data()), data->size());
}

sk_sp<SkData> MakeData(const std::string& str) {
  return SkData::MakeWithCopy(str.c_str(), str.length());
}

}  // namespace

GrShaderCache::GrShaderCache(size_t max_cache_size_bytes, Client* client)
    : cache_size_limit_(max_cache_size_bytes),
      store_(Store::NO_AUTO_EVICT),
      client_(client),
      enable_vk_pipeline_cache_(
          base::FeatureList::IsEnabled(features::kEnableVkPipelineCache)) {
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "GrShaderCache",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

GrShaderCache::~GrShaderCache() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

sk_sp<SkData> GrShaderCache::load(const SkData& key) {
  TRACE_EVENT0("gpu", "GrShaderCache::load");
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(current_client_id(), kInvalidClientId);

  CacheKey cache_key(SkData::MakeWithoutCopy(key.data(), key.size()));
  auto it = store_.Get(cache_key);
  UMA_HISTOGRAM_BOOLEAN("Gpu.GrShaderCacheLoadHitInCache",
                        (it != store_.end()));

  if (it == store_.end())
    return nullptr;

  if (it->second.prefetched_but_not_read) {
    it->second.prefetched_but_not_read = false;
    // Skia just loaded shader that was loaded from the disk. We assume it
    // happens during VkGraphicsPipeline creation and might alter the pipeline
    // cache. We'll store it to disk later when we're idle.

    // Note, there is no reliable way to check if this is VkGraphicsPipeline
    // entry or not, so we don't distinguish here and will try to store pipeline
    // cache even if we just loaded it. It should happen only once per GrContext
    // creation.
    need_store_pipeline_cache_ = true;
  }
  WriteToDisk(it->first, &it->second);
  return it->second.data;
}

void GrShaderCache::store(const SkData& key, const SkData& data) {
  TRACE_EVENT0("gpu", "GrShaderCache::store");
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(current_client_id(), kInvalidClientId);

  CacheKey cache_key(SkData::MakeWithCopy(key.data(), key.size()));

  if (data.size() > cache_size_limit_)
    return;
  EnforceLimits(data.size());

  auto existing_it = store_.Get(cache_key);
  if (existing_it != store_.end()) {
    // Skia may ignore the cached entry and regenerate a shader if it fails to
    // link, in which case replace the current version with the latest one.
    EraseFromCache(existing_it);
  }

  CacheData cache_data(SkData::MakeWithCopy(data.data(), data.size()));
  auto it = AddToCache(cache_key, std::move(cache_data));

  WriteToDisk(it->first, &it->second);

  // Skia just stored new shader, we assume it happens during VkGraphicsPipeline
  // creation and might alter the pipeline cache. We'll store it to disk later
  // when we're idle.
  need_store_pipeline_cache_ = true;
}

void GrShaderCache::PopulateCache(const std::string& key,
                                  const std::string& data) {
  TRACE_EVENT0("gpu", "GrShaderCache::PopulateCache");
  base::AutoLock auto_lock(lock_);

  std::string decoded_key;
  base::Base64Decode(key, &decoded_key);
  CacheKey cache_key(MakeData(decoded_key));

  if (data.length() > cache_size_limit_) {
    return;
  }

  EnforceLimits(data.size());

  // If we already have this in the cache, skia may have stored it before it
  // was loaded off the disk cache. Its better to keep the latest version
  // generated version than overwriting it here.
  if (store_.Get(cache_key) != store_.end()) {
    return;
  }

  CacheData cache_data(MakeData(data));
  auto it = AddToCache(cache_key, std::move(cache_data));

  // This was loaded off the disk cache, no need to push this back for disk
  // write.
  it->second.pending_disk_write = false;
  it->second.prefetched_but_not_read = true;
}

GrShaderCache::Store::iterator GrShaderCache::AddToCache(CacheKey key,
                                                         CacheData data) {
  lock_.AssertAcquired();
  auto it = store_.Put(key, std::move(data));
  curr_size_bytes_ += it->second.data->size();
  return it;
}

template <typename Iterator>
void GrShaderCache::EraseFromCache(Iterator it) {
  lock_.AssertAcquired();
  DCHECK_GE(curr_size_bytes_, it->second.data->size());

  curr_size_bytes_ -= it->second.data->size();
  store_.Erase(it);
}

void GrShaderCache::CacheClientIdOnDisk(int32_t client_id) {
  base::AutoLock auto_lock(lock_);
  client_ids_to_cache_on_disk_.insert(client_id);
}

void GrShaderCache::PurgeMemory(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  base::AutoLock auto_lock(lock_);
  size_t original_limit = cache_size_limit_;

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      return;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      cache_size_limit_ = cache_size_limit_ / 4;
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      cache_size_limit_ = 0;
      break;
  }

  EnforceLimits(0u);
  cache_size_limit_ = original_limit;
}

bool GrShaderCache::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                 base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock auto_lock(lock_);
  using base::trace_event::MemoryAllocatorDump;
  std::string dump_name =
      base::StringPrintf("gpu/gr_shader_cache/cache_0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, curr_size_bytes_);

  return true;
}

size_t GrShaderCache::num_cache_entries() const {
  base::AutoLock auto_lock(lock_);
  return store_.size();
}

size_t GrShaderCache::curr_size_bytes_for_testing() const {
  base::AutoLock auto_lock(lock_);
  return curr_size_bytes_;
}

void GrShaderCache::WriteToDisk(const CacheKey& key, CacheData* data) {
  lock_.AssertAcquired();
  DCHECK_NE(current_client_id(), kInvalidClientId);

  if (!data->pending_disk_write)
    return;

  // Only cache the shader on disk if this client id is permitted.
  if (client_ids_to_cache_on_disk_.count(current_client_id()) == 0)
    return;

  data->pending_disk_write = false;

  std::string encoded_key = base::Base64Encode(MakeString(key.data.get()));
  client_->StoreShader(encoded_key, MakeString(data->data.get()));
}

void GrShaderCache::EnforceLimits(size_t size_needed) {
  lock_.AssertAcquired();
  DCHECK_LE(size_needed, cache_size_limit_);

  while (size_needed + curr_size_bytes_ > cache_size_limit_)
    EraseFromCache(store_.rbegin());
}

void GrShaderCache::StoreVkPipelineCacheIfNeeded(GrDirectContext* gr_context) {
  // This method must be called only by one gpu thread which is gpu main
  // thread. Calling it from multiple gpu threads and hence multiple context is
  // redundant and expensive since each GrContext will have same key. Hence
  // adding a DCHECK here.
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);

  bool need_store_pipeline_cache = false;
  {
    base::AutoLock auto_lock(lock_);
    need_store_pipeline_cache = need_store_pipeline_cache_;
  }

  if (enable_vk_pipeline_cache_ && need_store_pipeline_cache) {
    {
      gr_context->storeVkPipelineCacheData();
      {
        base::AutoLock auto_lock(lock_);
        need_store_pipeline_cache_ = false;
      }
    }
  }
}

int32_t GrShaderCache::current_client_id() const {
  lock_.AssertAcquired();
  auto it = current_client_id_.find(base::PlatformThread::CurrentId());
  if (it != current_client_id_.end())
    return it->second;
  return kInvalidClientId;
}

GrShaderCache::ScopedCacheUse::ScopedCacheUse(GrShaderCache* cache,
                                              int32_t client_id)
    : cache_(cache) {
  base::AutoLock auto_lock(cache_->lock_);
  DCHECK_EQ(cache_->current_client_id(), kInvalidClientId);
  DCHECK_NE(client_id, kInvalidClientId);
  cache_->current_client_id_[base::PlatformThread::CurrentId()] = client_id;
}

GrShaderCache::ScopedCacheUse::~ScopedCacheUse() {
  base::AutoLock auto_lock(cache_->lock_);
  cache_->current_client_id_.erase(base::PlatformThread::CurrentId());
}

GrShaderCache::CacheKey::CacheKey(sk_sp<SkData> data) : data(std::move(data)) {
  hash = base::FastHash(base::span(this->data->bytes(), this->data->size()));
}
GrShaderCache::CacheKey::CacheKey(const CacheKey& other) = default;
GrShaderCache::CacheKey::CacheKey(CacheKey&& other) = default;
GrShaderCache::CacheKey& GrShaderCache::CacheKey::operator=(
    const CacheKey& other) = default;
GrShaderCache::CacheKey& GrShaderCache::CacheKey::operator=(CacheKey&& other) =
    default;
GrShaderCache::CacheKey::~CacheKey() = default;

bool GrShaderCache::CacheKey::operator==(const CacheKey& other) const {
  return data->equals(other.data.get());
}

GrShaderCache::CacheData::CacheData(sk_sp<SkData> data)
    : data(std::move(data)) {}
GrShaderCache::CacheData::CacheData(CacheData&& other) = default;
GrShaderCache::CacheData& GrShaderCache::CacheData::operator=(
    CacheData&& other) = default;
GrShaderCache::CacheData::~CacheData() = default;

bool GrShaderCache::CacheData::operator==(const CacheData& other) const {
  return data->equals(other.data.get());
}

}  // namespace raster
}  // namespace gpu

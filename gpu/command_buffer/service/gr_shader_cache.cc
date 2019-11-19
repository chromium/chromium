// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gr_shader_cache.h"

#include <inttypes.h>

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"

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
      client_(client) {
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "GrShaderCache", base::ThreadTaskRunnerHandle::Get());
  }
}

GrShaderCache::~GrShaderCache() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

sk_sp<SkData> GrShaderCache::load(const SkData& key) {
  TRACE_EVENT0("gpu", "GrShaderCache::load");
  DCHECK_NE(current_client_id_, kInvalidClientId);

  CacheKey cache_key(SkData::MakeWithoutCopy(key.data(), key.size()));
  auto it = store_.Get(cache_key);
  if (it == store_.end())
    return nullptr;

  WriteToDisk(it->first, &it->second);
  return it->second.data;
}

void GrShaderCache::store(const SkData& key, const SkData& data) {
  TRACE_EVENT0("gpu", "GrShaderCache::store");
  DCHECK_NE(current_client_id_, kInvalidClientId);

  if (data.size() > cache_size_limit_)
    return;
  EnforceLimits(data.size());

  CacheKey cache_key(SkData::MakeWithCopy(key.data(), key.size()));
  auto existing_it = store_.Get(cache_key);
  if (existing_it != store_.end()) {
    // Skia may ignore the cached entry and regenerate a shader if it fails to
    // link, in which case replace the current version with the latest one.
    EraseFromCache(existing_it);
  }

  CacheData cache_data(SkData::MakeWithCopy(data.data(), data.size()));
  auto it = AddToCache(cache_key, std::move(cache_data));

  WriteToDisk(it->first, &it->second);
}

void GrShaderCache::PopulateCache(const std::string& key,
                                  const std::string& data) {
  TRACE_EVENT0("gpu", "GrShaderCache::PopulateCache");
  if (data.length() > cache_size_limit_)
    return;

  EnforceLimits(data.size());

  // If we already have this in the cache, skia may have populated it before it
  // was loaded off the disk cache. Its better to keep the latest version
  // generated version than overwriting it here.
  std::string decoded_key;
  base::Base64Decode(key, &decoded_key);
  CacheKey cache_key(MakeData(decoded_key));
  if (store_.Get(cache_key) != store_.end())
    return;

  CacheData cache_data(MakeData(data));
  auto it = AddToCache(cache_key, std::move(cache_data));

  // This was loaded off the disk cache, no need to push this back for disk
  // write.
  it->second.pending_disk_write = false;
}

GrShaderCache::Store::iterator GrShaderCache::AddToCache(CacheKey key,
                                                         CacheData data) {
  auto it = store_.Put(key, std::move(data));
  curr_size_bytes_ += it->second.data->size();
  return it;
}

template <typename Iterator>
void GrShaderCache::EraseFromCache(Iterator it) {
  DCHECK_GE(curr_size_bytes_, it->second.data->size());

  curr_size_bytes_ -= it->second.data->size();
  store_.Erase(it);
}

void GrShaderCache::CacheClientIdOnDisk(int32_t client_id) {
  client_ids_to_cache_on_disk_.insert(client_id);
}

void GrShaderCache::PurgeMemory(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  size_t original_limit = cache_size_limit_;

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      // This function is only called with moderate or critical pressure.
      NOTREACHED();
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
  using base::trace_event::MemoryAllocatorDump;
  std::string dump_name =
      base::StringPrintf("gpu/gr_shader_cache/cache_0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, curr_size_bytes_);

  return true;
}

void GrShaderCache::WriteToDisk(const CacheKey& key, CacheData* data) {
  DCHECK_NE(current_client_id_, kInvalidClientId);

  if (!data->pending_disk_write)
    return;

  // Only cache the shader on disk if this client id is permitted.
  if (client_ids_to_cache_on_disk_.count(current_client_id_) == 0)
    return;

  data->pending_disk_write = false;

  std::string encoded_key;
  base::Base64Encode(MakeString(key.data.get()), &encoded_key);
  client_->StoreShader(encoded_key, MakeString(data->data.get()));
}

void GrShaderCache::EnforceLimits(size_t size_needed) {
  DCHECK_LE(size_needed, cache_size_limit_);

  while (size_needed + curr_size_bytes_ > cache_size_limit_)
    EraseFromCache(store_.rbegin());
}

GrShaderCache::ScopedCacheUse::ScopedCacheUse(GrShaderCache* cache,
                                              int32_t client_id)
    : cache_(cache) {
  DCHECK_EQ(cache_->current_client_id_, kInvalidClientId);
  DCHECK_NE(client_id, kInvalidClientId);

  cache_->current_client_id_ = client_id;
}

GrShaderCache::ScopedCacheUse::~ScopedCacheUse() {
  cache_->current_client_id_ = kInvalidClientId;
}

GrShaderCache::CacheKey::CacheKey(sk_sp<SkData> data) : data(std::move(data)) {
  hash = base::Hash(this->data->data(), this->data->size());
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

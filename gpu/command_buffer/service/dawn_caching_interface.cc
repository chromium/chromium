// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_caching_interface.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/config/gpu_preferences.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"

namespace gpu::webgpu {

DawnCachingInterface::DawnCachingInterface(ScopedDiskCacheBackend backend,
                                           DecoderClient* decoder_client)
    : backend_(std::move(backend)), decoder_client_(decoder_client) {}

DawnCachingInterface::~DawnCachingInterface() = default;

size_t DawnCachingInterface::LoadData(const void* key,
                                      size_t key_size,
                                      void* value_out,
                                      size_t value_size) {
  if (backend() == nullptr) {
    return 0u;
  }
  std::string key_str(static_cast<const char*>(key), key_size);

  disk_cache::EntryResult entry_result = backend()->OpenOrCreateEntry(
      key_str, net::RequestPriority::DEFAULT_PRIORITY, base::DoNothing());
  if (entry_result.net_error() != net::OK) {
    return 0u;
  }
  disk_cache::ScopedEntryPtr entry(entry_result.ReleaseEntry());
  size_t size = entry->GetDataSize(0) > 0 ? entry->GetDataSize(0) : 0u;

  if (value_size == 0 && value_out == nullptr) {
    return size;
  }
  if (value_size != size) {
    return 0u;
  }

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::WrappedIOBuffer>(
          static_cast<const char*>(value_out));
  int status = entry->ReadData(0, 0, buffer.get(), size, base::DoNothing());
  return status > 0 ? size_t(status) : 0u;
}

void DawnCachingInterface::StoreData(const void* key,
                                     size_t key_size,
                                     const void* value,
                                     size_t value_size) {
  if (backend() == nullptr || value == nullptr || value_size <= 0) {
    return;
  }
  std::string key_str(static_cast<const char*>(key), key_size);

  disk_cache::EntryResult entry_result = backend()->OpenOrCreateEntry(
      key_str, net::RequestPriority::DEFAULT_PRIORITY, base::DoNothing());
  if (entry_result.net_error() != net::OK) {
    return;
  }
  disk_cache::ScopedEntryPtr entry(entry_result.ReleaseEntry());

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::WrappedIOBuffer>(
          static_cast<const char*>(value));
  entry->WriteData(0, 0, buffer.get(), value_size, base::DoNothing(), false);

  // Send the cache entry to be stored on the host-side if applicable.
  if (decoder_client_) {
    std::string value_str(static_cast<const char*>(value), value_size);
    decoder_client_->CacheBlob(gpu::GpuDiskCacheType::kDawnWebGPU, key_str,
                               value_str);
  }
}

DawnCachingInterfaceFactory::DawnCachingInterfaceFactory(BackendFactory factory)
    : backend_factory_(factory) {}

DawnCachingInterfaceFactory::DawnCachingInterfaceFactory()
    : DawnCachingInterfaceFactory(base::BindRepeating(
          &DawnCachingInterfaceFactory::CreateDefaultInMemoryBackend)) {}

DawnCachingInterfaceFactory::~DawnCachingInterfaceFactory() = default;

ScopedDiskCacheBackend
DawnCachingInterfaceFactory::CreateDefaultInMemoryBackend() {
  disk_cache::BackendResult result = disk_cache::CreateCacheBackend(
      net::CacheType::MEMORY_CACHE, net::CACHE_BACKEND_DEFAULT,
      /*file_operations=*/nullptr, base::FilePath(),
      /*max_bytes=*/GetDefaultGpuDiskCacheSize(),
      disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, base::DoNothing());

  // In-memory cache initialization should generally not fail. If it does,
  // currently just logging an error. All operations check that |backend_| is
  // valid so nothing bad will happen and we will fail silently with no cache
  // hits.
  if (result.net_error != net::OK) {
    LOG(WARNING) << "Unable to create an in-memory cache to back "
                    "DawnCachingInterface(s).";
    return base::MakeRefCounted<RefCountedDiskCacheBackend>(nullptr);
  }
  return base::MakeRefCounted<RefCountedDiskCacheBackend>(
      std::move(result.backend));
}

std::unique_ptr<DawnCachingInterface>
DawnCachingInterfaceFactory::CreateInstance(
    const gpu::GpuDiskCacheHandle& handle,
    DecoderClient* decoder_client) {
  DCHECK(gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnWebGPU);

  if (const auto it = backends_.find(handle); it != backends_.end()) {
    return base::WrapUnique(
        new DawnCachingInterface(it->second, decoder_client));
  }

  ScopedDiskCacheBackend backend = backend_factory_.Run();
  if (backend->data.get() != nullptr) {
    backends_[handle] = backend;
  }
  return base::WrapUnique(
      new DawnCachingInterface(std::move(backend), decoder_client));
}

std::unique_ptr<DawnCachingInterface>
DawnCachingInterfaceFactory::CreateInstance() {
  return base::WrapUnique(new DawnCachingInterface(backend_factory_.Run()));
}

void DawnCachingInterfaceFactory::ReleaseHandle(
    const gpu::GpuDiskCacheHandle& handle) {
  DCHECK(gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnWebGPU);
  backends_.erase(handle);
}

}  // namespace gpu::webgpu

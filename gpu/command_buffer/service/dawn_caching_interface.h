// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_CACHING_INTERFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_CACHING_INTERFACE_H_

#include <dawn/platform/DawnPlatform.h>

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/linked_list.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/service/memory_cache.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"

namespace gpu::webgpu {

class DawnCachingInterfaceFactory;

// Provides a wrapper class around an in-memory DawnMemoryCache and a disk
// cache.
class GPU_GLES2_EXPORT DawnCachingInterface
    : public dawn::platform::CachingInterface {
 public:
  using CacheBlobCallback =
      base::RepeatingCallback<void(const std::string& key,
                                   const std::string& blob)>;

  ~DawnCachingInterface() override;

  size_t LoadData(const void* key,
                  size_t key_size,
                  void* value_out,
                  size_t value_size) override;
  void StoreData(const void* key,
                 size_t key_size,
                 const void* value,
                 size_t value_size) override;

 private:
  friend class DawnCachingInterfaceFactory;

  // Simplified accessor to the backend.
  MemoryCache* memory_cache() { return memory_cache_backend_.get(); }

  // Constructor is private because creation of interfaces should be deferred to
  // the factory.
  explicit DawnCachingInterface(scoped_refptr<MemoryCache> backend,
                                CacheBlobCallback callback = {});

  // Caching interface owns a reference to the backend.
  scoped_refptr<MemoryCache> memory_cache_backend_ = nullptr;

  // The callback provides ability to store cache entries to persistent disk.
  CacheBlobCallback cache_blob_callback_;
};

// Factory class for producing and managing DawnCachingInterfaces.
// Creating/using caching interfaces through the factory guarantees that we will
// not run into issues where backends are being initialized with the same
// parameters leading to blockage.
class GPU_GLES2_EXPORT DawnCachingInterfaceFactory
    : public base::trace_event::MemoryDumpProvider {
 public:
  // Factory for backend creation, especially for testing.
  using BackendFactory = base::RepeatingCallback<scoped_refptr<MemoryCache>()>;

  explicit DawnCachingInterfaceFactory(BackendFactory factory);
  DawnCachingInterfaceFactory();
  ~DawnCachingInterfaceFactory() override;

  // Returns a pointer to a DawnCachingInterface, creating a backend for it if
  // necessary. For handle based instances, the factory keeps a reference to the
  // backend until ReleaseHandle below is called.
  std::unique_ptr<DawnCachingInterface> CreateInstance(
      const gpu::GpuDiskCacheHandle& handle,
      DawnCachingInterface::CacheBlobCallback callback = {});

  // Returns a pointer to a DawnCachingInterface that owns the in memory
  // backend. This is used for incognito cases where the cache should not be
  // persisted to disk.
  std::unique_ptr<DawnCachingInterface> CreateInstance();

  // Releases the factory held reference of the handle's backend. Generally this
  // is the last reference which means that the in-memory disk cache will be
  // destroyed and the resources reclaimed. The factory needs to hold an extra
  // reference in order to avoid potential races where the browser may be about
  // to reuse the same handle, but the last reference on the GPU side has just
  // been released causing us to clear the in-memory disk cache too early. When
  // that happens, the disk cache entries are not re-sent over to the GPU
  // process. To avoid this, when the browser's last reference goes away, it
  // notifies the GPU process, and the last reference held by the factory is
  // released.
  void ReleaseHandle(const gpu::GpuDiskCacheHandle& handle);

  void PurgeMemory(base::MemoryPressureLevel memory_pressure_level);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  scoped_refptr<MemoryCache> GetOrCreateMemoryCache(
      const gpu::GpuDiskCacheHandle& handle);

  // Creates a default backend for assignment.
  static scoped_refptr<MemoryCache> CreateDefaultInMemoryBackend();

  // Factory to create backends.
  BackendFactory backend_factory_;

  // Map that holds existing backends.
  base::flat_map<gpu::GpuDiskCacheHandle, scoped_refptr<MemoryCache>> backends_;
};

}  // namespace gpu::webgpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_CACHING_INTERFACE_H_

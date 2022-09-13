// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_CACHING_INTERFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_CACHING_INTERFACE_H_

#include <dawn/platform/DawnPlatform.h>

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "net/disk_cache/disk_cache.h"

namespace gpu {

class DecoderClient;

namespace webgpu {

class DawnCachingInterfaceFactory;

using RefCountedDiskCacheBackend =
    base::RefCountedData<std::unique_ptr<disk_cache::Backend>>;
using ScopedDiskCacheBackend = scoped_refptr<RefCountedDiskCacheBackend>;

// Provides a wrapper class around an in-memory disk_cache::Backend. This class
// was originally designed to handle both disk and in-memory cache backends, but
// because it lives on the GPU process and does not have permissions (due to
// sandbox restrictions) to disk, the disk functionality was removed. Should it
// become necessary to provide interfaces over a disk level disk_cache::Backend,
// please refer to the file history for reference. Note that the big difference
// between in-memory and disk backends are the sync vs async nature of the two
// respectively. Because we are only handling in-memory backends now, the logic
// can be simplified to handle everything synchronously.
class GPU_GLES2_EXPORT DawnCachingInterface
    : public dawn::platform::CachingInterface {
 public:
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
  disk_cache::Backend* backend() { return backend_->data.get(); }

  // Constructor is private because creation of interfaces should be deferred to
  // the factory.
  explicit DawnCachingInterface(ScopedDiskCacheBackend backend,
                                DecoderClient* decoder_client = nullptr);

  // Caching interface owns a reference to the backend.
  ScopedDiskCacheBackend backend_ = nullptr;

  // Decoder client provides ability to store cache entries to persistent disk.
  // The client is not owned by this class and needs to be valid throughout the
  // interfaces lifetime.
  raw_ptr<DecoderClient> decoder_client_ = nullptr;
};

// Factory class for producing and managing DawnCachingInterfaces.
// Creating/using caching interfaces through the factory guarantees that we will
// not run into issues where backends are being initialized with the same
// parameters leading to blockage.
class GPU_GLES2_EXPORT DawnCachingInterfaceFactory {
 public:
  // Factory for backend creation, especially for testing.
  using BackendFactory = base::RepeatingCallback<ScopedDiskCacheBackend()>;

  explicit DawnCachingInterfaceFactory(BackendFactory factory);
  DawnCachingInterfaceFactory();
  ~DawnCachingInterfaceFactory();

  // Returns a pointer to a DawnCachingInterface, creating a backend for it if
  // necessary. For handle based instances, the factory keeps a reference to the
  // backend until ReleaseHandle below is called.
  std::unique_ptr<DawnCachingInterface> CreateInstance(
      const gpu::GpuDiskCacheHandle& handle,
      DecoderClient* decoder_client = nullptr);

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

 private:
  // Creates a scoped disk cache backend for assignment.
  static ScopedDiskCacheBackend CreateDefaultInMemoryBackend();

  // Factory to create backends.
  BackendFactory backend_factory_;

  // Map that holds existing backends.
  std::map<gpu::GpuDiskCacheHandle, ScopedDiskCacheBackend> backends_;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_CACHING_INTERFACE_H_

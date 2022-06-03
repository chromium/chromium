// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_CACHING_INTERFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_CACHING_INTERFACE_H_

#include <dawn_platform/DawnPlatform.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/gpu_gles2_export.h"
#include "net/disk_cache/disk_cache.h"

namespace gpu::webgpu {

class GPU_GLES2_EXPORT DawnCachingInterface
    : public dawn::platform::CachingInterface {
 public:
  // Default constructor takes a valid cache type (either disk or memory at the
  // moment), a maximum cache size in bytes (0 can be passed for default), and a
  // file path to locate the cache files.
  DawnCachingInterface(net::CacheType cache_type,
                       int64_t cache_size,
                       const base::FilePath& path);

  // Factory for backend injection. Note that to inject a backend, we need to
  // pass a factory to the constructor and not the direct Backend because the
  // Backend must be created inside the thread owned by the caching interface in
  // order for it to be usable. By passing a factory, we can post the factory to
  // the thread. The signal should be signalled once the backend initialization
  // has completed.
  using CacheBackendFactory =
      base::OnceCallback<void(std::unique_ptr<disk_cache::Backend>*,
                              base::WaitableEvent* signal,
                              net::Error* error)>;
  static std::unique_ptr<DawnCachingInterface> CreateForTesting(
      CacheBackendFactory factory);

  ~DawnCachingInterface() override;

  // Returns a non net::OK error code if a failure occurs during initialization.
  net::Error Init();

  // Synchronously loads from the cache. Blocks for a timeout to wait for the
  // asynchronous backend, returning early if we were unable to complete the
  // request. If we return due to a timeout, we always return 0 to indicate no
  // reading occurred, and nothing should be written to `value_out`.
  size_t LoadData(const void* key,
                  size_t key_size,
                  void* value_out,
                  size_t value_size) override;

  // Posts a task to write out to the cache that may run asynchronously.
  void StoreData(const void* key,
                 size_t key_size,
                 const void* value,
                 size_t value_size) override;

 private:
  static void DefaultCacheBackendFactory(
      net::CacheType cache_type,
      int64_t cache_size,
      const base::FilePath& path,
      std::unique_ptr<disk_cache::Backend>* backend,
      base::WaitableEvent* signal,
      net::Error* error);

  // Factory constructor. Note this is exposed externally for testing via
  // CreateForTesting.
  explicit DawnCachingInterface(CacheBackendFactory factory);

  // Temporarily saves the factory so that we can use it when we call Init.
  CacheBackendFactory factory_;

  // Background thread owned by the caching interface used to post tasks to the
  // disk_cache::Backend. Note that this additional thread is necessary because
  // the backends expect the thread they are created on to not block, however we
  // are blocking to appear synchronous. Without this, we can cause a deadlock
  // when waiting.
  scoped_refptr<base::SingleThreadTaskRunner> backend_thread_;

  // Caching interface owns the backend. Note that backends initialized using
  // the same backing file path will block on one another at initialization.
  // This means that only one such backend will successfully be created, and
  // thus users should be careful to avoid creating the same backend arguments
  // unintentionally.
  std::unique_ptr<disk_cache::Backend, base::OnTaskRunnerDeleter> backend_;
};

}  // namespace gpu::webgpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_CACHING_INTERFACE_H_

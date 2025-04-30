// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_MANAGER_SINGLETON_H_
#define CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_MANAGER_SINGLETON_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/host/host_gpu_memory_buffer_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"

namespace content {

// This class ensures that there is at most one instance of
// |viz::HostGpuMemoryBufferManager| in content at any given time. Code in
// content must use this class to access the instance.

class GpuMemoryBufferManagerSingleton : public viz::HostGpuMemoryBufferManager {
 public:
  explicit GpuMemoryBufferManagerSingleton(int client_id);
  GpuMemoryBufferManagerSingleton(const GpuMemoryBufferManagerSingleton&) =
      delete;
  GpuMemoryBufferManagerSingleton& operator=(
      const GpuMemoryBufferManagerSingleton&) = delete;
  ~GpuMemoryBufferManagerSingleton() override;

  static GpuMemoryBufferManagerSingleton* GetInstance();

  void AddObserver(gpu::GpuMemoryBufferManagerObserver* observer) final;
  void RemoveObserver(gpu::GpuMemoryBufferManagerObserver* observer) final;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_MANAGER_SINGLETON_H_

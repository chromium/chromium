// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_SEMAPHORE_POOL_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_SEMAPHORE_POOL_H_

#include <vulkan/vulkan_core.h>

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class SharedContextState;

class GPU_GLES2_EXPORT ExternalSemaphorePool {
 public:
  explicit ExternalSemaphorePool(SharedContextState* shared_context_state);
  ~ExternalSemaphorePool();

  ExternalSemaphorePool(const ExternalSemaphorePool&) = delete;
  ExternalSemaphorePool& operator=(const ExternalSemaphorePool&) = delete;

  // Get a semaphore from the pool. If there is no semaphore available in the
  // pool, a new semaphore will be created.
  ExternalSemaphore GetOrCreateSemaphore();

  // Return a semaphore to the pool. It can be reused or released immediately.
  void ReturnSemaphore(ExternalSemaphore semaphore);

  // Return semaphores to the pool. They can be reused or released immediately.
  void ReturnSemaphores(std::vector<ExternalSemaphore> semaphores);

  // Return semaphores to the pool. They cannot be reused or released until all
  // submitted GPU work is finished.
  void ReturnSemaphoresWithFenceHelper(
      std::vector<ExternalSemaphore> semaphores);

 private:
  const raw_ptr<SharedContextState> shared_context_state_;
  base::circular_deque<ExternalSemaphore> semaphores_;
  base::WeakPtrFactory<ExternalSemaphorePool> weak_ptr_factory_{this};
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_SEMAPHORE_POOL_H_
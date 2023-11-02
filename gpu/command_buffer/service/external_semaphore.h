// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_SEMAPHORE_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_SEMAPHORE_H_

#include <vulkan/vulkan_core.h>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/vulkan/semaphore_handle.h"

namespace viz {
class VulkanContextProvider;
}

namespace gpu {

class GPU_GLES2_EXPORT ExternalSemaphore {
 public:
  static ExternalSemaphore Create(viz::VulkanContextProvider* context_provider);

  static ExternalSemaphore CreateFromHandle(
      viz::VulkanContextProvider* context_provider,
      SemaphoreHandle handle);

  ExternalSemaphore();
  ExternalSemaphore(ExternalSemaphore&& other);
  ExternalSemaphore(base::PassKey<ExternalSemaphore>,
                    viz::VulkanContextProvider* context_provider,
                    VkSemaphore semaphore,
                    SemaphoreHandle handle);
  ~ExternalSemaphore();

  ExternalSemaphore& operator=(ExternalSemaphore&& other);
  ExternalSemaphore(const ExternalSemaphore&) = delete;
  ExternalSemaphore& operator=(const ExternalSemaphore&) = delete;
  explicit operator bool() const { return is_valid(); }

  void Reset();

  // Get the GL semaphore. The ownership is not transferred to caller.
  unsigned int GetGLSemaphore();

  // Get a VkSemaphore. The ownership is not transferred to caller.
  VkSemaphore GetVkSemaphore();

  bool is_valid() const { return context_provider_ && handle_.is_valid(); }
  SemaphoreHandle handle() { return handle_.Duplicate(); }

  // Take ownership of semaphore handle and then calls Reset().
  SemaphoreHandle TakeSemaphoreHandle();

 private:
  raw_ptr<viz::VulkanContextProvider> context_provider_ = nullptr;
  VkSemaphore semaphore_ = VK_NULL_HANDLE;
  SemaphoreHandle handle_;
  unsigned int gl_semaphore_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_SEMAPHORE_H_

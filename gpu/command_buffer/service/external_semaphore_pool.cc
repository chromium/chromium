// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_semaphore_pool.h"

#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gl/gl_context.h"

namespace gpu {
namespace {

#if BUILDFLAG(IS_FUCHSIA)
// On Fuchsia semaphores are passed to scenic as zx::event. Scenic doesn't reset
// them after waiting, so they would have to be reset explicitly to be reused.
// OTOH new semaphores are cheap, so reuse doesn't provide significant benefits.
constexpr size_t kMaxSemaphoresInPool = 0;
#else
constexpr size_t kMaxSemaphoresInPool = 16;
#endif

}  // namespace

ExternalSemaphorePool::ExternalSemaphorePool(
    SharedContextState* shared_context_state)
    : shared_context_state_(shared_context_state) {}

ExternalSemaphorePool::~ExternalSemaphorePool() = default;

ExternalSemaphore ExternalSemaphorePool::GetOrCreateSemaphore() {
  if (!semaphores_.empty()) {
    auto semaphore = std::move(semaphores_.front());
    semaphores_.pop_front();
    return semaphore;
  }
  return ExternalSemaphore::Create(
      shared_context_state_->vk_context_provider());
}

void ExternalSemaphorePool::ReturnSemaphore(ExternalSemaphore semaphore) {
  DCHECK(semaphore);
  if (semaphores_.size() < kMaxSemaphoresInPool)
    semaphores_.push_back(std::move(semaphore));
}

void ExternalSemaphorePool::ReturnSemaphores(
    std::vector<ExternalSemaphore> semaphores) {
  DCHECK_LE(semaphores_.size(), kMaxSemaphoresInPool);

  while (!semaphores.empty() && semaphores_.size() < kMaxSemaphoresInPool) {
    auto& semaphore = semaphores.back();
    DCHECK(semaphore);
    semaphores_.emplace_back(std::move(semaphore));
    semaphores.pop_back();
  }

  if (semaphores.empty())
    return;

  // Need a GL context current for releasing semaphores.
  if (!gl::GLContext::GetCurrent())
    shared_context_state_->MakeCurrent(/*surface=*/nullptr, /*needs_gl=*/true);
}

void ExternalSemaphorePool::ReturnSemaphoresWithFenceHelper(
    std::vector<ExternalSemaphore> semaphores) {
#if DCHECK_IS_ON()
  for (auto& semaphore : semaphores)
    DCHECK(semaphore);
#endif

  if (semaphores.empty())
    return;

  auto* fence_helper = shared_context_state_->vk_context_provider()
                           ->GetDeviceQueue()
                           ->GetFenceHelper();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](base::WeakPtr<ExternalSemaphorePool> pool,
         std::vector<ExternalSemaphore> semaphores,
         VulkanDeviceQueue* device_queue, bool device_lost) {
        if (pool)
          pool->ReturnSemaphores(std::move(semaphores));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(semaphores)));
}

}  // namespace gpu

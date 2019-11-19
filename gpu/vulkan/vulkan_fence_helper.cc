// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_fence_helper.h"

#include "base/bind.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VulkanFenceHelper::FenceHandle::FenceHandle() = default;
VulkanFenceHelper::FenceHandle::FenceHandle(VkFence fence,
                                            uint64_t generation_id)
    : fence_(fence), generation_id_(generation_id) {}
VulkanFenceHelper::FenceHandle::FenceHandle(const FenceHandle& other) = default;
VulkanFenceHelper::FenceHandle& VulkanFenceHelper::FenceHandle::operator=(
    const FenceHandle& other) = default;

VulkanFenceHelper::VulkanFenceHelper(VulkanDeviceQueue* device_queue)
    : device_queue_(device_queue) {}

VulkanFenceHelper::~VulkanFenceHelper() {
  DCHECK(tasks_pending_fence_.empty());
  DCHECK(cleanup_tasks_.empty());
}

void VulkanFenceHelper::Destroy() {
  PerformImmediateCleanup();
}

// TODO(ericrk): Handle recycling fences.
VkResult VulkanFenceHelper::GetFence(VkFence* fence) {
  VkFenceCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  return vkCreateFence(device_queue_->GetVulkanDevice(), &create_info,
                       nullptr /* pAllocator */, fence);
}

VulkanFenceHelper::FenceHandle VulkanFenceHelper::EnqueueFence(VkFence fence) {
  FenceHandle handle(fence, next_generation_++);
  cleanup_tasks_.emplace_back(handle, std::move(tasks_pending_fence_));
  tasks_pending_fence_ = std::vector<CleanupTask>();

  return handle;
}

bool VulkanFenceHelper::Wait(FenceHandle handle,
                             uint64_t timeout_in_nanoseconds) {
  if (HasPassed(handle))
    return true;

  VkResult result =
      vkWaitForFences(device_queue_->GetVulkanDevice(), 1, &handle.fence_, true,
                      timeout_in_nanoseconds);

  // After waiting, we can process cleanup tasks.
  ProcessCleanupTasks();

  return result == VK_SUCCESS;
}

bool VulkanFenceHelper::HasPassed(FenceHandle handle) {
  // Process cleanup tasks which advances our |current_generation_|.
  ProcessCleanupTasks();

  return current_generation_ >= handle.generation_id_;
}

void VulkanFenceHelper::EnqueueCleanupTaskForSubmittedWork(CleanupTask task) {
  tasks_pending_fence_.emplace_back(std::move(task));
}

void VulkanFenceHelper::ProcessCleanupTasks() {
  VkDevice device = device_queue_->GetVulkanDevice();

  // Iterate over our pending cleanup fences / tasks, advancing
  // |current_generation_| as far as possible.
  for (const auto& tasks_for_fence : cleanup_tasks_) {
    // If we're already ahead of this task (callback modified |generation_id_|),
    // continue.
    if (tasks_for_fence.generation_id <= current_generation_)
      continue;

    // Callback based tasks have no actual fence to wait on, keep checking
    // future fences, as a callback may be delayed.
    if (tasks_for_fence.UsingCallback())
      continue;

    VkResult result = vkGetFenceStatus(device, tasks_for_fence.fence);
    if (result == VK_NOT_READY)
      break;
    if (result != VK_SUCCESS) {
      PerformImmediateCleanup();
      return;
    }
    current_generation_ = tasks_for_fence.generation_id;
  }

  // Runs any cleanup tasks for generations that have passed. Create a temporary
  // vector of tasks to run to avoid reentrancy issues.
  std::vector<CleanupTask> tasks_to_run;
  while (!cleanup_tasks_.empty()) {
    TasksForFence& tasks_for_fence = cleanup_tasks_.front();
    if (tasks_for_fence.generation_id > current_generation_)
      break;
    if (tasks_for_fence.fence != VK_NULL_HANDLE) {
      DCHECK_EQ(vkGetFenceStatus(device, tasks_for_fence.fence), VK_SUCCESS);
      vkDestroyFence(device, tasks_for_fence.fence, nullptr);
    }
    tasks_to_run.insert(tasks_to_run.end(),
                        std::make_move_iterator(tasks_for_fence.tasks.begin()),
                        std::make_move_iterator(tasks_for_fence.tasks.end()));
    cleanup_tasks_.pop_front();
  }

  for (auto& task : tasks_to_run)
    std::move(task).Run(device_queue_, false /* device_lost */);
}

VulkanFenceHelper::FenceHandle VulkanFenceHelper::GenerateCleanupFence() {
  if (tasks_pending_fence_.empty())
    return FenceHandle();

  VkFence fence = VK_NULL_HANDLE;
  VkResult result = GetFence(&fence);
  if (result != VK_SUCCESS) {
    PerformImmediateCleanup();
    return FenceHandle();
  }
  result = vkQueueSubmit(device_queue_->GetVulkanQueue(), 0, nullptr, fence);
  if (result != VK_SUCCESS) {
    vkDestroyFence(device_queue_->GetVulkanDevice(), fence, nullptr);
    PerformImmediateCleanup();
    return FenceHandle();
  }

  return EnqueueFence(fence);
}

base::OnceClosure VulkanFenceHelper::CreateExternalCallback() {
  // No need to do callback tracking if there are no cleanup tasks to run.
  if (tasks_pending_fence_.empty())
    return base::OnceClosure();

  // Get a generation ID for this callback and associate existing cleanup
  // tasks.
  uint64_t generation_id = next_generation_++;
  cleanup_tasks_.emplace_back(generation_id, std::move(tasks_pending_fence_));
  tasks_pending_fence_ = std::vector<CleanupTask>();

  return base::BindOnce(
      [](base::WeakPtr<VulkanFenceHelper> fence_helper,
         uint64_t generation_id) {
        if (!fence_helper)
          return;
        // If |current_generation_| is ahead of the callback's
        // |generation_id|, the callback came late. Ignore it.
        if (generation_id > fence_helper->current_generation_) {
          fence_helper->current_generation_ = generation_id;
          fence_helper->ProcessCleanupTasks();
        }
      },
      weak_factory_.GetWeakPtr(), generation_id);
}

void VulkanFenceHelper::EnqueueSemaphoreCleanupForSubmittedWork(
    VkSemaphore semaphore) {
  if (semaphore == VK_NULL_HANDLE)
    return;

  EnqueueSemaphoresCleanupForSubmittedWork({semaphore});
}

void VulkanFenceHelper::EnqueueSemaphoresCleanupForSubmittedWork(
    std::vector<VkSemaphore> semaphores) {
  if (semaphores.empty())
    return;

  EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](std::vector<VkSemaphore> semaphores, VulkanDeviceQueue* device_queue,
         bool /* is_lost */) {
        for (VkSemaphore semaphore : semaphores) {
          vkDestroySemaphore(device_queue->GetVulkanDevice(), semaphore,
                             nullptr);
        }
      },
      std::move(semaphores)));
}

void VulkanFenceHelper::EnqueueImageCleanupForSubmittedWork(
    VkImage image,
    VkDeviceMemory memory) {
  if (image == VK_NULL_HANDLE && memory == VK_NULL_HANDLE)
    return;

  EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](VkImage image, VkDeviceMemory memory, VulkanDeviceQueue* device_queue,
         bool /* is_lost */) {
        if (image != VK_NULL_HANDLE)
          vkDestroyImage(device_queue->GetVulkanDevice(), image, nullptr);
        if (memory != VK_NULL_HANDLE)
          vkFreeMemory(device_queue->GetVulkanDevice(), memory, nullptr);
      },
      image, memory));
}

void VulkanFenceHelper::EnqueueBufferCleanupForSubmittedWork(
    VkBuffer buffer,
    VkDeviceMemory memory) {
  if (buffer == VK_NULL_HANDLE && memory == VK_NULL_HANDLE)
    return;

  EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](VkBuffer buffer, VkDeviceMemory memory,
         VulkanDeviceQueue* device_queue, bool /* is_lost */) {
        if (buffer != VK_NULL_HANDLE)
          vkDestroyBuffer(device_queue->GetVulkanDevice(), buffer, nullptr);
        if (memory != VK_NULL_HANDLE)
          vkFreeMemory(device_queue->GetVulkanDevice(), memory, nullptr);
      },
      buffer, memory));
}

void VulkanFenceHelper::PerformImmediateCleanup() {
  if (cleanup_tasks_.empty() && tasks_pending_fence_.empty())
    return;

  // We want to run all tasks immediately, so just use vkQueueWaitIdle which
  // ensures that all fences have passed.
  // Even if exclusively using callbacks, the callbacks use WeakPtr and will
  // not keep this class alive, so it's important to wait / run all cleanup
  // immediately.
  VkResult result = vkQueueWaitIdle(device_queue_->GetVulkanQueue());
  // Wait can only fail for three reasons - device loss, host OOM, device OOM.
  // If we hit an OOM, treat this as a crash. There isn't a great way to
  // recover from this.
  CHECK(result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST);
  bool device_lost = result == VK_ERROR_DEVICE_LOST;
  if (!device_lost)
    current_generation_ = next_generation_ - 1;

  // Run all cleanup tasks. Create a temporary vector of tasks to run to avoid
  // reentrancy issues.
  std::vector<CleanupTask> tasks_to_run;
  while (!cleanup_tasks_.empty()) {
    auto& tasks_for_fence = cleanup_tasks_.front();
    vkDestroyFence(device_queue_->GetVulkanDevice(), tasks_for_fence.fence,
                   nullptr);
    tasks_to_run.insert(tasks_to_run.end(),
                        std::make_move_iterator(tasks_for_fence.tasks.begin()),
                        std::make_move_iterator(tasks_for_fence.tasks.end()));
    cleanup_tasks_.pop_front();
  }
  tasks_to_run.insert(tasks_to_run.end(),
                      std::make_move_iterator(tasks_pending_fence_.begin()),
                      std::make_move_iterator(tasks_pending_fence_.end()));
  tasks_pending_fence_.clear();
  for (auto& task : tasks_to_run)
    std::move(task).Run(device_queue_, device_lost);
}

VulkanFenceHelper::TasksForFence::TasksForFence(FenceHandle handle,
                                                std::vector<CleanupTask> tasks)
    : fence(handle.fence_),
      generation_id(handle.generation_id_),
      tasks(std::move(tasks)) {}
VulkanFenceHelper::TasksForFence::TasksForFence(uint64_t generation_id,
                                                std::vector<CleanupTask> tasks)
    : generation_id(generation_id), tasks(std::move(tasks)) {}
VulkanFenceHelper::TasksForFence::~TasksForFence() = default;
VulkanFenceHelper::TasksForFence::TasksForFence(TasksForFence&& other) =
    default;

}  // namespace gpu

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_SWAP_CHAIN_H_
#define GPU_VULKAN_VULKAN_SWAP_CHAIN_H_

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/swap_result.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {

class VulkanDeviceQueue;

class COMPONENT_EXPORT(VULKAN) VulkanSwapChain {
 public:
  class COMPONENT_EXPORT(VULKAN) ScopedWrite {
   public:
    explicit ScopedWrite(VulkanSwapChain* swap_chain);
    ~ScopedWrite();

    bool success() const { return success_; }
    VkImage image() const { return image_; }
    uint32_t image_index() const { return image_index_; }
    VkImageLayout image_layout() const { return image_layout_; }
    VkSemaphore begin_semaphore() const { return begin_semaphore_; }
    VkSemaphore end_semaphore() const { return end_semaphore_; }

   private:
    VulkanSwapChain* const swap_chain_;
    bool success_ = false;
    VkImage image_ = VK_NULL_HANDLE;
    uint32_t image_index_ = 0;
    VkImageLayout image_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkSemaphore begin_semaphore_ = VK_NULL_HANDLE;
    VkSemaphore end_semaphore_ = VK_NULL_HANDLE;

    DISALLOW_COPY_AND_ASSIGN(ScopedWrite);
  };

  VulkanSwapChain();
  ~VulkanSwapChain();

  // min_image_count is the minimum number of presentable images.
  bool Initialize(VulkanDeviceQueue* device_queue,
                  VkSurfaceKHR surface,
                  const VkSurfaceFormatKHR& surface_format,
                  const gfx::Size& image_size,
                  uint32_t min_image_count,
                  VkImageUsageFlags image_usage_flags,
                  VkSurfaceTransformFlagBitsKHR pre_transform,
                  bool use_protected_memory,
                  std::unique_ptr<VulkanSwapChain> old_swap_chain);

  // Destroy() should be called when all related GPU tasks have been finished.
  void Destroy();

  // Present the current buffer.
  gfx::SwapResult PostSubBuffer(const gfx::Rect& rect);
  using PostSubBufferCompletionCallback =
      base::OnceCallback<void(gfx::SwapResult)>;
  void PostSubBufferAsync(const gfx::Rect& rect,
                          PostSubBufferCompletionCallback callback);

  uint32_t num_images() const {
    // size of |images_| will not be changed after initializing, so it is safe
    // to read it here.
    return static_cast<uint32_t>(TS_UNCHECKED_READ(images_).size());
  }
  const gfx::Size& size() const {
    // |size_| is never changed after initialization.
    return size_;
  }
  bool use_protected_memory() const {
    // |use_protected_memory_| is never changed after initialization.
    return use_protected_memory_;
  }

  uint32_t current_image_index() const {
    base::AutoLock auto_lock(lock_);
    DCHECK(acquired_image_);
    return *acquired_image_;
  }

  VkResult state() const {
    base::AutoLock auto_lock(lock_);
    return state_;
  }

 private:
  struct ImageData {
    VkImage image = VK_NULL_HANDLE;
    VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Semaphore used for vkAcquireNextImageKHR()
    VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
    // Semaphore used for vkQueuePresentKHR()
    VkSemaphore present_semaphore = VK_NULL_HANDLE;
  };

  bool InitializeSwapChain(VkSurfaceKHR surface,
                           const VkSurfaceFormatKHR& surface_format,
                           const gfx::Size& image_size,
                           uint32_t min_image_count,
                           VkImageUsageFlags image_usage_flags,
                           VkSurfaceTransformFlagBitsKHR pre_transform,
                           bool use_protected_memory,
                           std::unique_ptr<VulkanSwapChain> old_swap_chain)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DestroySwapChain() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool InitializeSwapImages(const VkSurfaceFormatKHR& surface_format)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DestroySwapImages() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool BeginWriteCurrentImage(VkImage* image,
                              uint32_t* image_index,
                              VkImageLayout* layout,
                              VkSemaphore* begin_semaphore,
                              VkSemaphore* end_semaphore);
  void EndWriteCurrentImage();

  bool PresentBuffer(const gfx::Rect& rect) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool AcquireNextImage() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Wait until PostSubBufferAsync() is finished on ThreadPool.
  void WaitUntilPostSubBufferAsyncFinished() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  struct FenceAndSemaphores {
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore semaphores[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
  };
  FenceAndSemaphores GetOrCreateFenceAndSemaphores()
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void ReturnFenceAndSemaphores(const FenceAndSemaphores& fence_and_semaphores)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  mutable base::Lock lock_;

  bool use_protected_memory_ = false;
  VulkanDeviceQueue* device_queue_ = nullptr;
  bool is_incremental_present_supported_ = false;
  VkSwapchainKHR swap_chain_ GUARDED_BY(lock_) = VK_NULL_HANDLE;
  gfx::Size size_;

  // Images in the swap chain.
  std::vector<ImageData> images_ GUARDED_BY(lock_);

  // True if BeginWriteCurrentImage() is called, but EndWriteCurrentImage() is
  // not.
  bool is_writing_ GUARDED_BY(lock_) = false;

  // True if the current image is new required, and BeginWriteCurrentImage() and
  // EndWriteCurrentImage() pair are not called.
  bool new_acquired_ GUARDED_BY(lock_) = true;

  // Condition variable is signalled when a PostSubBufferAsync() is finished.
  base::ConditionVariable condition_variable_{&lock_};

  // True if there is pending post sub buffer in the fly.
  bool has_pending_post_sub_buffer_ GUARDED_BY(lock_) = false;

  // The current swapchain state_.
  VkResult state_ GUARDED_BY(lock_) = VK_SUCCESS;

  // Acquired images queue.
  base::Optional<uint32_t> acquired_image_ GUARDED_BY(lock_);

  // For executing task on GPU main thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // For executing PosSubBufferAsync tasks off the GPU main thread.
  scoped_refptr<base::SequencedTaskRunner> post_sub_buffer_task_runner_;

#if !defined(OS_FUCHSIA)
  // Available fence and semaphores can be reused when fence is passed.
  // Not used on Fuchsia because Fuchsia's swapchain implementation doesn't
  // support fences.
  base::circular_deque<FenceAndSemaphores> fence_and_semaphores_queue_
      GUARDED_BY(lock_);
#endif

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(VulkanSwapChain);
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_SWAP_CHAIN_H_
// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_swap_chain.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

namespace {

VkSemaphore CreateSemaphore(VkDevice vk_device) {
  // Generic semaphore creation structure.
  constexpr VkSemaphoreCreateInfo semaphore_create_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

  VkSemaphore vk_semaphore = VK_NULL_HANDLE;
  auto result = vkCreateSemaphore(vk_device, &semaphore_create_info, nullptr,
                                  &vk_semaphore);
  LOG_IF(FATAL, VK_SUCCESS != result)
      << "vkCreateSemaphore() failed: " << result;
  return vk_semaphore;
}

}  // namespace

VulkanSwapChain::VulkanSwapChain(uint64_t acquire_next_image_timeout_ns)
    : acquire_next_image_timeout_ns_(acquire_next_image_timeout_ns) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

VulkanSwapChain::~VulkanSwapChain() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(images_.empty());
  DCHECK_EQ(static_cast<VkSwapchainKHR>(VK_NULL_HANDLE), swap_chain_);
#endif
}

bool VulkanSwapChain::Initialize(
    VulkanDeviceQueue* device_queue,
    VkSurfaceKHR surface,
    const VkSurfaceFormatKHR& surface_format,
    const gfx::Size& image_size,
    uint32_t min_image_count,
    VkImageUsageFlags image_usage_flags,
    VkSurfaceTransformFlagBitsKHR pre_transform,
    bool use_protected_memory,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(device_queue);
  DCHECK(!use_protected_memory || device_queue->allow_protected_memory());

  use_protected_memory_ = use_protected_memory;
  device_queue_ = device_queue;
  is_incremental_present_supported_ =
      gfx::HasExtension(device_queue_->enabled_extensions(),
                        VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
  device_queue_->GetFenceHelper()->ProcessCleanupTasks();
  return InitializeSwapChain(surface, surface_format, image_size,
                             min_image_count, image_usage_flags, pre_transform,
                             use_protected_memory, std::move(old_swap_chain)) &&
         InitializeSwapImages(surface_format) && AcquireNextImage();
}

void VulkanSwapChain::Destroy() {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  WaitUntilPostSubBufferAsyncFinished();

#if !defined(OS_FUCHSIA)
  if (UNLIKELY(!fence_and_semaphores_queue_.empty())) {
    VkDevice device = device_queue_->GetVulkanDevice();
    {
      // Make sure the last enqueued fence is passed, so we can release all
      // other fences and semaphores safely.
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      // Use 1 second timeout for vkWaitForFences(), it should be long enough.
      constexpr auto kTimeout = base::TimeTicks::kNanosecondsPerSecond;
      auto result =
          vkWaitForFences(device, 1, &fence_and_semaphores_queue_.back().fence,
                          VK_TRUE, kTimeout);
      if (result != VK_SUCCESS)
        LOG(ERROR) << "vkWaitForFences() failed: " << result;
    }
    for (auto& fence_and_semaphores : fence_and_semaphores_queue_) {
      vkDestroyFence(device, fence_and_semaphores.fence,
                     nullptr /* pAllocator */);
      vkDestroySemaphore(device, fence_and_semaphores.semaphores[0],
                         nullptr /* pAllocator */);
      vkDestroySemaphore(device, fence_and_semaphores.semaphores[1],
                         nullptr /* pAllocator */);
    }
    fence_and_semaphores_queue_.clear();
  }
#endif  // !defined(OS_FUCHSIA)

  DCHECK(!is_writing_);
  DestroySwapImages();
  DestroySwapChain();
}

gfx::SwapResult VulkanSwapChain::PostSubBuffer(const gfx::Rect& rect) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  WaitUntilPostSubBufferAsyncFinished();
  DCHECK(!has_pending_post_sub_buffer_);

  if (UNLIKELY(!PresentBuffer(rect)))
    return gfx::SwapResult::SWAP_FAILED;

  if (UNLIKELY(!AcquireNextImage()))
    return gfx::SwapResult::SWAP_FAILED;

  return gfx::SwapResult::SWAP_ACK;
}

void VulkanSwapChain::PostSubBufferAsync(
    const gfx::Rect& rect,
    PostSubBufferCompletionCallback callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  WaitUntilPostSubBufferAsyncFinished();
  DCHECK(!has_pending_post_sub_buffer_);

  if (UNLIKELY(!PresentBuffer(rect))) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), gfx::SwapResult::SWAP_FAILED));
    return;
  }

  DCHECK_EQ(state_, VK_SUCCESS);

  has_pending_post_sub_buffer_ = true;
  post_sub_buffer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](VulkanSwapChain* self,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             PostSubBufferCompletionCallback callback) {
            base::AutoLock auto_lock(self->lock_);
            DCHECK(self->has_pending_post_sub_buffer_);
            auto swap_result = self->AcquireNextImage()
                                   ? gfx::SwapResult::SWAP_ACK
                                   : gfx::SwapResult::SWAP_FAILED;
            task_runner->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), swap_result));
            self->has_pending_post_sub_buffer_ = false;
            self->condition_variable_.Signal();
          },
          base::Unretained(this), base::ThreadTaskRunnerHandle::Get(),
          std::move(callback)));
}

bool VulkanSwapChain::InitializeSwapChain(
    VkSurfaceKHR surface,
    const VkSurfaceFormatKHR& surface_format,
    const gfx::Size& image_size,
    uint32_t min_image_count,
    VkImageUsageFlags image_usage_flags,
    VkSurfaceTransformFlagBitsKHR pre_transform,
    bool use_protected_memory,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  VkSwapchainCreateInfoKHR swap_chain_create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .flags = use_protected_memory ? VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR : 0,
      .surface = surface,
      .minImageCount = min_image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = {image_size.width(), image_size.height()},
      .imageArrayLayers = 1,
      .imageUsage = image_usage_flags,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = pre_transform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  if (LIKELY(old_swap_chain)) {
    base::AutoLock auto_lock(old_swap_chain->lock_);
    old_swap_chain->WaitUntilPostSubBufferAsyncFinished();
    swap_chain_create_info.oldSwapchain = old_swap_chain->swap_chain_;
    // Reuse |post_sub_buffer_task_runner_| and |fence_and_semaphores_queue_|
    // from the |old_swap_chain|.
    post_sub_buffer_task_runner_ = old_swap_chain->post_sub_buffer_task_runner_;
#if !defined(OS_FUCHSIA)
    fence_and_semaphores_queue_ =
        std::move(old_swap_chain->fence_and_semaphores_queue_);
    old_swap_chain->fence_and_semaphores_queue_.clear();
#endif  // !defined(OS_FUCHSIA)
  }

  VkSwapchainKHR new_swap_chain = VK_NULL_HANDLE;
  result = vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr,
                                &new_swap_chain);

  if (LIKELY(old_swap_chain)) {
    auto* fence_helper = device_queue_->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(old_swap_chain));
  }

  if (UNLIKELY(VK_SUCCESS != result)) {
    LOG(DFATAL) << "vkCreateSwapchainKHR() failed: " << result;
    return false;
  }

  swap_chain_ = new_swap_chain;
  size_ = gfx::Size(swap_chain_create_info.imageExtent.width,
                    swap_chain_create_info.imageExtent.height);

  if (UNLIKELY(!post_sub_buffer_task_runner_)) {
    post_sub_buffer_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  }

  image_usage_ = image_usage_flags;

  return true;
}

void VulkanSwapChain::DestroySwapChain() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VkDevice device = device_queue_->GetVulkanDevice();
  // vkDestroySwapchainKHR() will hang on X11, after resuming from hibernate.
  // It is because a Xserver issue. To workaround it, we will not call
  // vkDestroySwapchainKHR(), if the problem is detected. When the problem is
  // detected, we will consider it as context lost, so the GPU process will
  // tear down all resources, and a new GPU process will be created. So it is OK
  // to leak this swapchain.
  // TODO(penghuang): remove this workaround when Xserver issue is fixed
  // upstream. https://crbug.com/1130495
  if (!destroy_swapchain_will_hang_)
    vkDestroySwapchainKHR(device, swap_chain_, nullptr /* pAllocator */);
  swap_chain_ = VK_NULL_HANDLE;
}

bool VulkanSwapChain::InitializeSwapImages(
    const VkSurfaceFormatKHR& surface_format) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  uint32_t image_count = 0;
  result = vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, nullptr);
  if (UNLIKELY(VK_SUCCESS != result)) {
    LOG(FATAL) << "vkGetSwapchainImagesKHR(nullptr) failed: " << result;
    return false;
  }

  std::vector<VkImage> images(image_count);
  result =
      vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, images.data());
  if (UNLIKELY(VK_SUCCESS != result)) {
    LOG(FATAL) << "vkGetSwapchainImagesKHR(images) failed: " << result;
    return false;
  }

  images_.resize(image_count);
  for (uint32_t i = 0; i < image_count; ++i) {
    auto& image_data = images_[i];
    image_data.image = images[i];
  }
  return true;
}

void VulkanSwapChain::DestroySwapImages() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VkDevice device = device_queue_->GetVulkanDevice();
  for (auto& image : images_) {
    vkDestroySemaphore(device, image.acquire_semaphore,
                       nullptr /* pAllocator */);
    vkDestroySemaphore(device, image.present_semaphore,
                       nullptr /* pAllocator */);
  }
  images_.clear();
}

bool VulkanSwapChain::BeginWriteCurrentImage(VkImage* image,
                                             uint32_t* image_index,
                                             VkImageLayout* image_layout,
                                             VkImageUsageFlags* image_usage,
                                             VkSemaphore* begin_semaphore,
                                             VkSemaphore* end_semaphore) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(image);
  DCHECK(image_index);
  DCHECK(image_layout);
  DCHECK(image_usage);
  DCHECK(begin_semaphore);
  DCHECK(end_semaphore);
  DCHECK(!is_writing_);

  if (UNLIKELY(state_ != VK_SUCCESS))
    return false;

  if (UNLIKELY(!acquired_image_))
    return false;

  auto& current_image_data = images_[*acquired_image_];

  if (UNLIKELY(!new_acquired_)) {
    // In this case, {Begin,End}WriteCurrentImage has been called, but
    // PostSubBuffer() is not call, so |acquire_semaphore| has been wait on for
    // the previous write request, release it with FenceHelper.
    device_queue_->GetFenceHelper()->EnqueueSemaphoreCleanupForSubmittedWork(
        current_image_data.acquire_semaphore);
    // Use |end_semaphore| from previous write as |begin_semaphore| for the new
    // write request, and create a new semaphore for |end_semaphore|.
    current_image_data.acquire_semaphore = current_image_data.present_semaphore;
    current_image_data.present_semaphore =
        CreateSemaphore(device_queue_->GetVulkanDevice());
    if (UNLIKELY(current_image_data.present_semaphore == VK_NULL_HANDLE))
      return false;
  }

  *image = current_image_data.image;
  *image_index = *acquired_image_;
  *image_layout = current_image_data.image_layout;
  *image_usage = image_usage_;
  *begin_semaphore = current_image_data.acquire_semaphore;
  *end_semaphore = current_image_data.present_semaphore;
  is_writing_ = true;

  return true;
}

void VulkanSwapChain::EndWriteCurrentImage() {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_writing_);
  DCHECK(acquired_image_);

  auto& current_image_data = images_[*acquired_image_];
  current_image_data.image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  is_writing_ = false;
  new_acquired_ = false;
}

bool VulkanSwapChain::PresentBuffer(const gfx::Rect& rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, VK_SUCCESS);
  DCHECK(acquired_image_);

  auto& current_image_data = images_[*acquired_image_];
  DCHECK(current_image_data.present_semaphore != VK_NULL_HANDLE);

  VkRectLayerKHR rect_layer = {
      .offset = {rect.x(), rect.y()},
      .extent = {rect.width(), rect.height()},
      .layer = 0,
  };

  VkPresentRegionKHR present_region = {
      .rectangleCount = 1,
      .pRectangles = &rect_layer,
  };

  VkPresentRegionsKHR present_regions = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR,
      .swapchainCount = 1,
      .pRegions = &present_region,
  };

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = is_incremental_present_supported_ ? &present_regions : nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &current_image_data.present_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &swap_chain_,
      .pImageIndices = &acquired_image_.value(),
  };

  VkQueue queue = device_queue_->GetVulkanQueue();
  auto result = vkQueuePresentKHR(queue, &present_info);
  if (UNLIKELY(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)) {
    LOG(DFATAL) << "vkQueuePresentKHR() failed: " << result;
    state_ = result;
    return false;
  }

  LOG_IF(ERROR, result == VK_SUBOPTIMAL_KHR) << "Swapchain is suboptimal.";
  acquired_image_.reset();

  return true;
}

bool VulkanSwapChain::AcquireNextImage() {
  DCHECK_EQ(state_, VK_SUCCESS);
  DCHECK(!acquired_image_);

  // VulkanDeviceQueue is not threadsafe for now, but |device_queue_| will not
  // be released, and device_queue_->device will never be changed after
  // initialization, so it is safe for now.
  // TODO(penghuang): make VulkanDeviceQueue threadsafe.
  VkDevice device = device_queue_->GetVulkanDevice();
  auto fence_and_semaphores = GetOrCreateFenceAndSemaphores();
  if (UNLIKELY(fence_and_semaphores.semaphores[0] == VK_NULL_HANDLE)) {
#if !defined(OS_FUCHSIA)
    DCHECK(fence_and_semaphores.fence == VK_NULL_HANDLE);
#endif  // !defined(OS_FUCHSIA)
    DCHECK(fence_and_semaphores.semaphores[1] == VK_NULL_HANDLE);
    return false;
  }

  DCHECK(fence_and_semaphores.semaphores[0] != VK_NULL_HANDLE);
  DCHECK(fence_and_semaphores.semaphores[1] != VK_NULL_HANDLE);

  VkFence acquire_fence = fence_and_semaphores.fence;
  VkSemaphore acquire_semaphore = fence_and_semaphores.semaphores[0];
  VkSemaphore present_semaphore = fence_and_semaphores.semaphores[1];
  uint32_t next_image;
  auto result = ({
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    vkAcquireNextImageKHR(device, swap_chain_, acquire_next_image_timeout_ns_,
                          acquire_semaphore, acquire_fence, &next_image);
  });

  if (UNLIKELY(result == VK_TIMEOUT)) {
    LOG(ERROR) << "vkAcquireNextImageKHR() hangs.";
    vkDestroySemaphore(device, acquire_semaphore, nullptr);
    vkDestroySemaphore(device, present_semaphore, nullptr);
    vkDestroyFence(device, acquire_fence, nullptr);
    state_ = VK_ERROR_SURFACE_LOST_KHR;
    destroy_swapchain_will_hang_ = true;
    return false;
  }

  if (UNLIKELY(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)) {
    LOG(DFATAL) << "vkAcquireNextImageKHR() failed: " << result;
    vkDestroySemaphore(device, acquire_semaphore, nullptr);
    vkDestroySemaphore(device, present_semaphore, nullptr);
    vkDestroyFence(device, acquire_fence, nullptr);
    state_ = result;
    return false;
  }

  acquired_image_.emplace(next_image);
  new_acquired_ = true;

  // For the previous use of the image, |current_image_data.acquire_semaphore|
  // has been wait on for the compositing work last time,
  // and |current_image_data.present_semaphore| has been wait on by present
  // engine for presenting the image last time, so those two semaphores should
  // be free for reusing, when the |acquire_fence| is passed.
  auto& current_image_data = images_[next_image];
  ReturnFenceAndSemaphores({acquire_fence,
                            {current_image_data.acquire_semaphore,
                             current_image_data.present_semaphore}});
  current_image_data.acquire_semaphore = acquire_semaphore;
  current_image_data.present_semaphore = present_semaphore;

  return true;
}

void VulkanSwapChain::WaitUntilPostSubBufferAsyncFinished() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  while (has_pending_post_sub_buffer_) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    condition_variable_.Wait();
  }
  DCHECK(acquired_image_ || state_ != VK_SUCCESS);
}

VulkanSwapChain::FenceAndSemaphores
VulkanSwapChain::GetOrCreateFenceAndSemaphores() {
  VkDevice device = device_queue_->GetVulkanDevice();
  FenceAndSemaphores fence_and_semaphores;
  do {
#if !defined(OS_FUCHSIA)
    if (LIKELY(!fence_and_semaphores_queue_.empty())) {
      fence_and_semaphores = fence_and_semaphores_queue_.front();
      auto result = vkGetFenceStatus(device, fence_and_semaphores.fence);
      if (LIKELY(result == VK_SUCCESS)) {
        fence_and_semaphores_queue_.pop_front();
        vkResetFences(device, 1, &fence_and_semaphores.fence);
      } else if (LIKELY(result == VK_NOT_READY)) {
        // If fence is not passed, new fence and semaphores will be created.
        fence_and_semaphores = {};
      } else {
        DLOG(ERROR) << "vkGetFenceStatus() failed: " << result;
        fence_and_semaphores = {};
        break;
      }
    }

    if (UNLIKELY(fence_and_semaphores.fence == VK_NULL_HANDLE)) {
      constexpr VkFenceCreateInfo fence_create_info = {
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      };
      auto result =
          vkCreateFence(device, &fence_create_info,
                        /*pAllocator=*/nullptr, &fence_and_semaphores.fence);
      if (result != VK_SUCCESS) {
        DLOG(ERROR) << "vkCreateFence() failed: " << result;
        break;
      }
    }
#endif  // !defined(OS_FUCHSIA)

    if (UNLIKELY(fence_and_semaphores.semaphores[0] == VK_NULL_HANDLE))
      fence_and_semaphores.semaphores[0] = CreateSemaphore(device);
    if (UNLIKELY(fence_and_semaphores.semaphores[1] == VK_NULL_HANDLE))
      fence_and_semaphores.semaphores[1] = CreateSemaphore(device);

    if (UNLIKELY(fence_and_semaphores.semaphores[0] == VK_NULL_HANDLE ||
                 fence_and_semaphores.semaphores[1] == VK_NULL_HANDLE)) {
      break;
    }

    return fence_and_semaphores;
  } while (false);

  // Failed to get or create fence and semaphores, release resources.
  vkDestroyFence(device, fence_and_semaphores.fence, nullptr /* pAllocator */);
  vkDestroySemaphore(device, fence_and_semaphores.semaphores[0],
                     nullptr /* pAllocator */);
  vkDestroySemaphore(device, fence_and_semaphores.semaphores[1],
                     nullptr /* pAllocator */);
  return {};
}

void VulkanSwapChain::ReturnFenceAndSemaphores(
    const FenceAndSemaphores& fence_and_semaphores) {
#if defined(OS_FUCHSIA)
  VkDevice device = device_queue_->GetVulkanDevice();
  DCHECK(fence_and_semaphores.fence == VK_NULL_HANDLE);
  vkDestroySemaphore(device, fence_and_semaphores.semaphores[0],
                     nullptr /* pAllocator */);
  vkDestroySemaphore(device, fence_and_semaphores.semaphores[1],
                     nullptr /* pAllocator */);
#else
  DCHECK(fence_and_semaphores.fence != VK_NULL_HANDLE);
  fence_and_semaphores_queue_.push_back(fence_and_semaphores);
#endif
}

VulkanSwapChain::ScopedWrite::ScopedWrite(VulkanSwapChain* swap_chain)
    : swap_chain_(swap_chain) {
  success_ = swap_chain_->BeginWriteCurrentImage(
      &image_, &image_index_, &image_layout_, &image_usage_, &begin_semaphore_,
      &end_semaphore_);
  if (LIKELY(success_)) {
    DCHECK(begin_semaphore_ != VK_NULL_HANDLE);
    DCHECK(end_semaphore_ != VK_NULL_HANDLE);
  } else {
    DCHECK(begin_semaphore_ == VK_NULL_HANDLE);
    DCHECK(end_semaphore_ == VK_NULL_HANDLE);
  }
}

VulkanSwapChain::ScopedWrite::~ScopedWrite() {
  if (LIKELY(success_)) {
    DCHECK(begin_semaphore_ != VK_NULL_HANDLE);
    DCHECK(end_semaphore_ != VK_NULL_HANDLE);
    swap_chain_->EndWriteCurrentImage();
  } else {
    DCHECK(begin_semaphore_ == VK_NULL_HANDLE);
    DCHECK(end_semaphore_ == VK_NULL_HANDLE);
  }
}

}  // namespace gpu

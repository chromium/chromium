// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_swap_chain.h"

#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
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
  auto result = vkCreateSemaphore(vk_device, &semaphore_create_info,
                                  /*pAllocator=*/nullptr, &vk_semaphore);
  LOG_IF(FATAL, VK_SUCCESS != result)
      << "vkCreateSemaphore() failed: " << result;
  return vk_semaphore;
}

}  // namespace

VulkanSwapChain::VulkanSwapChain(uint64_t acquire_next_image_timeout_ns)
    : acquire_next_image_timeout_ns_(acquire_next_image_timeout_ns) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(acquire_next_image_timeout_ns, 0u);
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
    VkCompositeAlphaFlagBitsKHR composite_alpha,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(device_queue);

  device_queue_ = device_queue;
  is_incremental_present_supported_ =
      gfx::HasExtension(device_queue_->enabled_extensions(),
                        VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
  device_queue_->GetFenceHelper()->ProcessCleanupTasks();
  return InitializeSwapChain(surface, surface_format, image_size,
                             min_image_count, image_usage_flags, pre_transform,
                             composite_alpha, std::move(old_swap_chain)) &&
         InitializeSwapImages(surface_format) && AcquireNextImage();
}

void VulkanSwapChain::Destroy() {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  WaitUntilPostSubBufferAsyncFinished();

  if (!pending_semaphores_queue_.empty()) [[unlikely]] {
    auto* fence_helper = device_queue_->GetFenceHelper();
    fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
        [](base::circular_deque<PendingSemaphores> pending_semaphores_queue,
           VulkanDeviceQueue* device_queue, bool device_lost) {
          VkDevice device = device_queue->GetVulkanDevice();
          for (auto& pending_semaphores : pending_semaphores_queue) {
            vkDestroySemaphore(device, pending_semaphores.acquire_semaphore,
                               /*pAllocator=*/nullptr);
            vkDestroySemaphore(device, pending_semaphores.present_semaphore,
                               /*pAllocator=*/nullptr);
          }
        },
        std::move(pending_semaphores_queue_)));
    pending_semaphores_queue_.clear();
  }

  DCHECK(!is_writing_);
  DestroySwapImages();
  DestroySwapChain();
}

gfx::SwapResult VulkanSwapChain::PostSubBuffer(const gfx::Rect& rect) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  WaitUntilPostSubBufferAsyncFinished();
  DCHECK(!has_pending_post_sub_buffer_);

  if (!PresentBuffer(rect)) [[unlikely]] {
    return gfx::SwapResult::SWAP_FAILED;
  }

  if (!AcquireNextImage()) [[unlikely]] {
    return gfx::SwapResult::SWAP_FAILED;
  }

  return gfx::SwapResult::SWAP_ACK;
}

void VulkanSwapChain::PostSubBufferAsync(
    const gfx::Rect& rect,
    PostSubBufferCompletionCallback callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  WaitUntilPostSubBufferAsyncFinished();
  DCHECK(!has_pending_post_sub_buffer_);

  if (!PresentBuffer(rect)) [[unlikely]] {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
          base::Unretained(this),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          std::move(callback)));
}

bool VulkanSwapChain::InitializeSwapChain(
    VkSurfaceKHR surface,
    const VkSurfaceFormatKHR& surface_format,
    const gfx::Size& image_size,
    uint32_t min_image_count,
    VkImageUsageFlags image_usage_flags,
    VkSurfaceTransformFlagBitsKHR pre_transform,
    VkCompositeAlphaFlagBitsKHR composite_alpha,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  VkSwapchainCreateInfoKHR swap_chain_create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .flags = 0,
      .surface = surface,
      .minImageCount = min_image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = {static_cast<uint32_t>(image_size.width()),
                      static_cast<uint32_t>(image_size.height())},
      .imageArrayLayers = 1,
      .imageUsage = image_usage_flags,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = pre_transform,
      .compositeAlpha = composite_alpha,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  if (old_swap_chain) [[likely]] {
    base::AutoLock auto_lock(old_swap_chain->lock_);
    old_swap_chain->WaitUntilPostSubBufferAsyncFinished();
    swap_chain_create_info.oldSwapchain = old_swap_chain->swap_chain_;
    // Reuse |post_sub_buffer_task_runner_| and |pending_semaphores_queue_|
    // from the |old_swap_chain|.
    post_sub_buffer_task_runner_ = old_swap_chain->post_sub_buffer_task_runner_;
    pending_semaphores_queue_ =
        std::move(old_swap_chain->pending_semaphores_queue_);
    old_swap_chain->pending_semaphores_queue_.clear();
  }

  VkSwapchainKHR new_swap_chain = VK_NULL_HANDLE;
  result = vkCreateSwapchainKHR(device, &swap_chain_create_info,
                                /*pAllocator=*/nullptr, &new_swap_chain);

  if (old_swap_chain) [[likely]] {
    auto* fence_helper = device_queue_->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(old_swap_chain));
  }

  if (VK_SUCCESS != result) [[unlikely]] {
    LOG(DFATAL) << "vkCreateSwapchainKHR() failed: " << result;
    return false;
  }

  swap_chain_ = new_swap_chain;
  size_ = gfx::Size(swap_chain_create_info.imageExtent.width,
                    swap_chain_create_info.imageExtent.height);

  if (!post_sub_buffer_task_runner_) [[unlikely]] {
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
    vkDestroySwapchainKHR(device, swap_chain_, /*pAllocator=*/nullptr);
  swap_chain_ = VK_NULL_HANDLE;
}

bool VulkanSwapChain::InitializeSwapImages(
    const VkSurfaceFormatKHR& surface_format) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  uint32_t image_count = 0;
  result = vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, nullptr);
  if (VK_SUCCESS != result) [[unlikely]] {
    LOG(FATAL) << "vkGetSwapchainImagesKHR(nullptr) failed: " << result;
  }

  std::vector<VkImage> images(image_count);
  result =
      vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, images.data());
  if (VK_SUCCESS != result) [[unlikely]] {
    LOG(FATAL) << "vkGetSwapchainImagesKHR(images) failed: " << result;
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
                       /*pAllocator=*/nullptr);
    vkDestroySemaphore(device, image.present_semaphore,
                       /*pAllocator=*/nullptr);
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

  if (state_ != VK_SUCCESS) [[unlikely]] {
    return false;
  }

  if (!acquired_image_) [[unlikely]] {
    return false;
  }

  auto& current_image_data = images_[*acquired_image_];

  if (!new_acquired_) [[unlikely]] {
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
    if (current_image_data.present_semaphore == VK_NULL_HANDLE) [[unlikely]] {
      return false;
    }
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
      .extent = {static_cast<uint32_t>(rect.width()),
                 static_cast<uint32_t>(rect.height())},
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
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) [[unlikely]] {
    LOG(DFATAL) << "vkQueuePresentKHR() failed: " << result;
    state_ = result;
    return false;
  }

  VLOG_IF(2, result == VK_SUBOPTIMAL_KHR) << "Swapchain is suboptimal.";
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

  VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
  VkSemaphore present_semaphore = VK_NULL_HANDLE;
  if (!GetOrCreateSemaphores(&acquire_semaphore, &present_semaphore))
    return false;

  uint32_t next_image;
  auto result = ({
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    vkAcquireNextImageKHR(device, swap_chain_, acquire_next_image_timeout_ns_,
                          acquire_semaphore, /*fence=*/VK_NULL_HANDLE,
                          &next_image);
  });

  if (result == VK_TIMEOUT) [[unlikely]] {
    LOG(ERROR) << "vkAcquireNextImageKHR() hangs.";
    vkDestroySemaphore(device, acquire_semaphore, /*pAllocator=*/nullptr);
    vkDestroySemaphore(device, present_semaphore, /*pAllocator=*/nullptr);
    state_ = VK_ERROR_SURFACE_LOST_KHR;
    destroy_swapchain_will_hang_ = true;
    return false;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) [[unlikely]] {
    LOG(DFATAL) << "vkAcquireNextImageKHR() failed: " << result;
    vkDestroySemaphore(device, acquire_semaphore, /*pAllocator=*/nullptr);
    vkDestroySemaphore(device, present_semaphore, /*pAllocator=*/nullptr);
    state_ = result;
    return false;
  }

  acquired_image_.emplace(next_image);
  new_acquired_ = true;

  // For the previous use of the image, |current_image_data.acquire_semaphore|
  // has been wait on for the compositing work last time,
  // and |current_image_data.present_semaphore| has been wait on by present
  // engine for presenting the image last time, so those two semaphores should
  // be free for reusing when |num_images() * 2| frames are passed, because it
  // is impossible there are more than |num_images() * 2| frames are in flight.
  auto& current_image_data = images_[next_image];
  ReturnSemaphores(current_image_data.acquire_semaphore,
                   current_image_data.present_semaphore);
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

bool VulkanSwapChain::GetOrCreateSemaphores(VkSemaphore* acquire_semaphore,
                                            VkSemaphore* present_semaphore) {
  // When pending semaphores are more than |num_images() * 2|, we will
  // assume the semaphores at the front of the queue has been signaled
  // and can be reused (because it is impossible there are more than
  // |num_images() * 2| frames are in flight). Otherwise, new semaphores
  // will be created.
  if (pending_semaphores_queue_.size() >= num_images() * 2) [[likely]] {
    const auto& semaphores = pending_semaphores_queue_.front();
    DCHECK(semaphores.acquire_semaphore != VK_NULL_HANDLE);
    DCHECK(semaphores.present_semaphore != VK_NULL_HANDLE);
    pending_semaphores_queue_.pop_front();
    *acquire_semaphore = semaphores.acquire_semaphore;
    *present_semaphore = semaphores.present_semaphore;
    return true;
  }

  VkDevice device = device_queue_->GetVulkanDevice();
  *acquire_semaphore = CreateSemaphore(device);
  if (*acquire_semaphore == VK_NULL_HANDLE)
    return false;

  *present_semaphore = CreateSemaphore(device);
  if (*present_semaphore == VK_NULL_HANDLE) {
    // Failed to get or create semaphores, release resources.
    vkDestroySemaphore(device, *acquire_semaphore, /*pAllocator=*/nullptr);
    return false;
  }

  return true;
}

void VulkanSwapChain::ReturnSemaphores(VkSemaphore acquire_semaphore,
                                       VkSemaphore present_semaphore) {
  DCHECK_EQ(acquire_semaphore != VK_NULL_HANDLE,
            present_semaphore != VK_NULL_HANDLE);

  if (acquire_semaphore == VK_NULL_HANDLE)
    return;

  pending_semaphores_queue_.push_back({acquire_semaphore, present_semaphore});
}

VulkanSwapChain::ScopedWrite::ScopedWrite(VulkanSwapChain* swap_chain)
    : swap_chain_(swap_chain) {
  success_ = swap_chain_->BeginWriteCurrentImage(
      &image_, &image_index_, &image_layout_, &image_usage_, &begin_semaphore_,
      &end_semaphore_);
  if (success_) [[likely]] {
    DCHECK(begin_semaphore_ != VK_NULL_HANDLE);
    DCHECK(end_semaphore_ != VK_NULL_HANDLE);
  } else {
    DCHECK(begin_semaphore_ == VK_NULL_HANDLE);
    DCHECK(end_semaphore_ == VK_NULL_HANDLE);
  }
}

VulkanSwapChain::ScopedWrite::ScopedWrite(ScopedWrite&& other) {
  *this = std::move(other);
}

VulkanSwapChain::ScopedWrite::~ScopedWrite() {
  Reset();
}

const VulkanSwapChain::ScopedWrite& VulkanSwapChain::ScopedWrite::operator=(
    ScopedWrite&& other) {
  Reset();
  std::swap(swap_chain_, other.swap_chain_);
  std::swap(success_, other.success_);
  std::swap(image_, other.image_);
  std::swap(image_index_, other.image_index_);
  std::swap(image_layout_, other.image_layout_);
  std::swap(image_usage_, other.image_usage_);
  std::swap(begin_semaphore_, other.begin_semaphore_);
  std::swap(end_semaphore_, other.end_semaphore_);
  return *this;
}

void VulkanSwapChain::ScopedWrite::Reset() {
  if (success_) [[likely]] {
    DCHECK(begin_semaphore_ != VK_NULL_HANDLE);
    DCHECK(end_semaphore_ != VK_NULL_HANDLE);
    swap_chain_->EndWriteCurrentImage();
  } else {
    DCHECK(begin_semaphore_ == VK_NULL_HANDLE);
    DCHECK(end_semaphore_ == VK_NULL_HANDLE);
  }
  swap_chain_ = nullptr;
  success_ = false;
  image_ = VK_NULL_HANDLE;
  image_index_ = 0;
  image_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  image_usage_ = 0;
  begin_semaphore_ = VK_NULL_HANDLE;
  end_semaphore_ = VK_NULL_HANDLE;
}

}  // namespace gpu

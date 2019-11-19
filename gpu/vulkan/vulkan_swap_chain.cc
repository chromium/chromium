// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_swap_chain.h"

#include "base/bind.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

namespace {

VkSemaphore CreateSemaphore(VkDevice vk_device) {
  // Generic semaphore creation structure.
  VkSemaphoreCreateInfo semaphore_create_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

  VkSemaphore vk_semaphore;
  auto result = vkCreateSemaphore(vk_device, &semaphore_create_info, nullptr,
                                  &vk_semaphore);
  DLOG_IF(FATAL, VK_SUCCESS != result)
      << "vkCreateSemaphore() failed: " << result;
  return vk_semaphore;
}

}  // namespace

VulkanSwapChain::VulkanSwapChain() {}

VulkanSwapChain::~VulkanSwapChain() {
  DCHECK(images_.empty());
  DCHECK_EQ(static_cast<VkSwapchainKHR>(VK_NULL_HANDLE), swap_chain_);
}

bool VulkanSwapChain::Initialize(
    VulkanDeviceQueue* device_queue,
    VkSurfaceKHR surface,
    const VkSurfaceFormatKHR& surface_format,
    const gfx::Size& image_size,
    uint32_t min_image_count,
    VkSurfaceTransformFlagBitsKHR pre_transform,
    bool use_protected_memory,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  DCHECK(device_queue);
  DCHECK(!use_protected_memory || device_queue->allow_protected_memory());
  use_protected_memory_ = use_protected_memory;
  device_queue_ = device_queue;
  device_queue_->GetFenceHelper()->ProcessCleanupTasks();
  return InitializeSwapChain(surface, surface_format, image_size,
                             min_image_count, pre_transform,
                             use_protected_memory, std::move(old_swap_chain)) &&
         InitializeSwapImages(surface_format);
}

void VulkanSwapChain::Destroy() {
  DCHECK(!is_writing_);
  DestroySwapImages();
  DestroySwapChain();
}

gfx::SwapResult VulkanSwapChain::PresentBuffer() {
  DCHECK(acquired_image_);
  DCHECK(end_write_semaphore_ != VK_NULL_HANDLE);

  VkResult result = VK_SUCCESS;
  VkDevice device = device_queue_->GetVulkanDevice();
  VkQueue queue = device_queue_->GetVulkanQueue();
  auto* fence_helper = device_queue_->GetFenceHelper();

  auto& current_image_data = images_[*acquired_image_];
  if (current_image_data.layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    {
      current_image_data.command_buffer->Clear();
      ScopedSingleUseCommandBufferRecorder recorder(
          *current_image_data.command_buffer);
      current_image_data.command_buffer->TransitionImageLayout(
          current_image_data.image, current_image_data.layout,
          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    current_image_data.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkSemaphore vk_semaphore = CreateSemaphore(device);
    // Submit our command_buffer for the current buffer. It sets the image
    // layout for presenting.
    if (!current_image_data.command_buffer->Submit(1, &end_write_semaphore_, 1,
                                                   &vk_semaphore)) {
      vkDestroySemaphore(device, vk_semaphore, nullptr /* pAllocator */);
      return gfx::SwapResult::SWAP_FAILED;
    }
    current_image_data.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    fence_helper->EnqueueSemaphoreCleanupForSubmittedWork(end_write_semaphore_);
    end_write_semaphore_ = vk_semaphore;
  }

  // Queue the present.
  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &end_write_semaphore_;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swap_chain_;
  present_info.pImageIndices = &acquired_image_.value();

  result = vkQueuePresentKHR(queue, &present_info);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    DLOG(ERROR) << "vkQueuePresentKHR() failed: " << result;
    return gfx::SwapResult::SWAP_FAILED;
  }
  DLOG_IF(ERROR, result == VK_SUBOPTIMAL_KHR) << "Swapchian is suboptimal.";

  acquired_image_.reset();
  fence_helper->EnqueueSemaphoreCleanupForSubmittedWork(end_write_semaphore_);
  end_write_semaphore_ = VK_NULL_HANDLE;

  return gfx::SwapResult::SWAP_ACK;
}

bool VulkanSwapChain::InitializeSwapChain(
    VkSurfaceKHR surface,
    const VkSurfaceFormatKHR& surface_format,
    const gfx::Size& image_size,
    uint32_t min_image_count,
    VkSurfaceTransformFlagBitsKHR pre_transform,
    bool use_protected_memory,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  VkSwapchainCreateInfoKHR swap_chain_create_info = {};
  swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swap_chain_create_info.flags =
      use_protected_memory ? VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR : 0;
  swap_chain_create_info.surface = surface;
  swap_chain_create_info.minImageCount = min_image_count,
  swap_chain_create_info.imageFormat = surface_format.format;
  swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
  swap_chain_create_info.imageExtent.width = image_size.width();
  swap_chain_create_info.imageExtent.height = image_size.height();
  swap_chain_create_info.imageArrayLayers = 1;
  swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swap_chain_create_info.preTransform = pre_transform;
  swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swap_chain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  swap_chain_create_info.clipped = true;
  swap_chain_create_info.oldSwapchain =
      old_swap_chain ? old_swap_chain->swap_chain_ : VK_NULL_HANDLE;

  VkSwapchainKHR new_swap_chain = VK_NULL_HANDLE;
  result = vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr,
                                &new_swap_chain);

  if (old_swap_chain) {
    auto* fence_helper = device_queue_->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(old_swap_chain));
  }

  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateSwapchainKHR() failed: " << result;
    return false;
  }

  swap_chain_ = new_swap_chain;
  size_ = gfx::Size(swap_chain_create_info.imageExtent.width,
                    swap_chain_create_info.imageExtent.height);

  return true;
}

void VulkanSwapChain::DestroySwapChain() {
  if (swap_chain_ == VK_NULL_HANDLE)
    return;
  vkDestroySwapchainKHR(device_queue_->GetVulkanDevice(), swap_chain_,
                        nullptr /* pAllocator */);
  swap_chain_ = VK_NULL_HANDLE;
}

bool VulkanSwapChain::InitializeSwapImages(
    const VkSurfaceFormatKHR& surface_format) {
  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  uint32_t image_count = 0;
  result = vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetSwapchainImagesKHR(NULL) failed: " << result;
    return false;
  }

  std::vector<VkImage> images(image_count);
  result =
      vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, images.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetSwapchainImagesKHR(images) failed: " << result;
    return false;
  }

  command_pool_ = device_queue_->CreateCommandPool();
  if (!command_pool_)
    return false;

  images_.resize(image_count);
  for (uint32_t i = 0; i < image_count; ++i) {
    auto& image_data = images_[i];
    image_data.image = images[i];
    // Initialize the command buffer for this buffer data.
    image_data.command_buffer = command_pool_->CreatePrimaryCommandBuffer();
  }
  return true;
}

void VulkanSwapChain::DestroySwapImages() {
  if (end_write_semaphore_)
    vkDestroySemaphore(device_queue_->GetVulkanDevice(), end_write_semaphore_,
                       nullptr /* pAllocator */);
  end_write_semaphore_ = VK_NULL_HANDLE;

  for (auto& image_data : images_) {
    if (!image_data.command_buffer)
      continue;
    image_data.command_buffer->Destroy();
    image_data.command_buffer = nullptr;
  }
  images_.clear();

  command_pool_->Destroy();
  command_pool_ = nullptr;
}

bool VulkanSwapChain::BeginWriteCurrentImage(VkImage* image,
                                             uint32_t* image_index,
                                             VkImageLayout* image_layout,
                                             VkSemaphore* semaphore) {
  DCHECK(image);
  DCHECK(image_index);
  DCHECK(image_layout);
  DCHECK(semaphore);
  DCHECK(!is_writing_);

  VkSemaphore vk_semaphore = VK_NULL_HANDLE;

  if (!acquired_image_) {
    DCHECK(end_write_semaphore_ == VK_NULL_HANDLE);

    VkDevice device = device_queue_->GetVulkanDevice();
    vk_semaphore = CreateSemaphore(device);
    DCHECK(vk_semaphore != VK_NULL_HANDLE);

    uint32_t next_image = 0;
    // Acquire then next image.
    auto result =
        vkAcquireNextImageKHR(device, swap_chain_, UINT64_MAX, vk_semaphore,
                              VK_NULL_HANDLE, &next_image);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      vkDestroySemaphore(device, vk_semaphore, nullptr /* pAllocator */);
      DLOG(ERROR) << "vkAcquireNextImageKHR() failed: " << result;
      return false;
    }
    acquired_image_.emplace(next_image);
  } else {
    // In this case, PresentBuffer() is not called after
    // {Begin,End}WriteCurrentImage pairs, |end_write_semaphore_| should be
    // waited on before writing the image again.
    vk_semaphore = end_write_semaphore_;
    end_write_semaphore_ = VK_NULL_HANDLE;
  }

  auto& current_image_data = images_[*acquired_image_];
  *image = current_image_data.image;
  *image_index = *acquired_image_;
  *image_layout = current_image_data.layout;
  *semaphore = vk_semaphore;
  is_writing_ = true;

  return true;
}

void VulkanSwapChain::EndWriteCurrentImage(VkImageLayout image_layout,
                                           VkSemaphore semaphore) {
  DCHECK(is_writing_);
  DCHECK(acquired_image_);
  DCHECK(end_write_semaphore_ == VK_NULL_HANDLE);

  auto& current_image_data = images_[*acquired_image_];
  current_image_data.layout = image_layout;
  end_write_semaphore_ = semaphore;
  is_writing_ = false;
}

VulkanSwapChain::ScopedWrite::ScopedWrite(VulkanSwapChain* swap_chain)
    : swap_chain_(swap_chain) {
  success_ = swap_chain_->BeginWriteCurrentImage(
      &image_, &image_index_, &image_layout_, &begin_semaphore_);
}

VulkanSwapChain::ScopedWrite::~ScopedWrite() {
  DCHECK(begin_semaphore_ == VK_NULL_HANDLE);
  if (success_)
    swap_chain_->EndWriteCurrentImage(image_layout_, end_semaphore_);
}

VkSemaphore VulkanSwapChain::ScopedWrite::TakeBeginSemaphore() {
  VkSemaphore semaphore = begin_semaphore_;
  begin_semaphore_ = VK_NULL_HANDLE;
  return semaphore;
}

void VulkanSwapChain::ScopedWrite::SetEndSemaphore(VkSemaphore semaphore) {
  DCHECK(end_semaphore_ == VK_NULL_HANDLE);
  DCHECK(semaphore != VK_NULL_HANDLE);
  end_semaphore_ = semaphore;
}

VulkanSwapChain::ImageData::ImageData() = default;
VulkanSwapChain::ImageData::ImageData(ImageData&& other) = default;
VulkanSwapChain::ImageData::~ImageData() = default;
VulkanSwapChain::ImageData& VulkanSwapChain::ImageData::operator=(
    ImageData&& other) = default;

}  // namespace gpu

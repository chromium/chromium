// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_command_buffer.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"

namespace gpu {

namespace {

VkPipelineStageFlags GetPipelineStageFlags(
    const VulkanDeviceQueue* device_queue,
    const VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return VK_PIPELINE_STAGE_HOST_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
      VkPipelineStageFlags flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
      if (device_queue->enabled_device_features().tessellationShader) {
        flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                 VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
      }
      if (device_queue->enabled_device_features().geometryShader) {
        flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
      }
      return flags;
    }
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    default:
      NOTREACHED_IN_MIGRATION() << "layout=" << layout;
  }
  return 0;
}

VkAccessFlags GetAccessMask(const VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return 0;
    case VK_IMAGE_LAYOUT_GENERAL:
      LOG(WARNING) << "VK_IMAGE_LAYOUT_GENERAL is used.";
      return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
             VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT |
             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
             VK_ACCESS_HOST_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
             VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
             VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return VK_ACCESS_HOST_WRITE_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return 0;
    default:
      NOTREACHED_IN_MIGRATION() << "layout=" << layout;
  }
  return 0;
}

}  // namespace

VulkanCommandBuffer::VulkanCommandBuffer(VulkanDeviceQueue* device_queue,
                                         VulkanCommandPool* command_pool,
                                         bool primary)
    : primary_(primary),
      device_queue_(device_queue),
      command_pool_(command_pool) {
  command_pool_->IncrementCommandBufferCount();
}

VulkanCommandBuffer::~VulkanCommandBuffer() {
  DCHECK(!submission_fence_.is_valid());
  DCHECK_EQ(static_cast<VkCommandBuffer>(VK_NULL_HANDLE), command_buffer_);
  DCHECK(!recording_);
  command_pool_->DecrementCommandBufferCount();
}

bool VulkanCommandBuffer::Initialize() {
  VkResult result = VK_SUCCESS;
  VkDevice device = device_queue_->GetVulkanDevice();

  VkCommandBufferAllocateInfo command_buffer_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = command_pool_->handle(),
      .level = primary_ ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
                        : VK_COMMAND_BUFFER_LEVEL_SECONDARY,
      .commandBufferCount = 1,
  };

  DCHECK_EQ(static_cast<VkCommandBuffer>(VK_NULL_HANDLE), command_buffer_);
  result =
      vkAllocateCommandBuffers(device, &command_buffer_info, &command_buffer_);
  if (VK_SUCCESS != result) {
    LOG(ERROR) << "vkAllocateCommandBuffers() failed: " << result;
    return false;
  }

  record_type_ = RECORD_TYPE_EMPTY;
  return true;
}

void VulkanCommandBuffer::Destroy() {
  VkDevice device = device_queue_->GetVulkanDevice();
  if (submission_fence_.is_valid()) {
    DCHECK(device_queue_->GetFenceHelper()->HasPassed(submission_fence_));
    submission_fence_ = VulkanFenceHelper::FenceHandle();
  }

  if (VK_NULL_HANDLE != command_buffer_) {
    vkFreeCommandBuffers(device, command_pool_->handle(), 1, &command_buffer_);
    command_buffer_ = VK_NULL_HANDLE;
  }
}

bool VulkanCommandBuffer::Submit(uint32_t num_wait_semaphores,
                                 VkSemaphore* wait_semaphores,
                                 uint32_t num_signal_semaphores,
                                 VkSemaphore* signal_semaphores,
                                 bool allow_protected_memory) {
  DCHECK(primary_);

  std::vector<VkPipelineStageFlags> wait_dst_stage_mask(
      num_wait_semaphores, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

  VkProtectedSubmitInfo protected_submit_info = {};
  protected_submit_info.sType = VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO;
  protected_submit_info.protectedSubmit = allow_protected_memory;

  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = &protected_submit_info;
  submit_info.waitSemaphoreCount = num_wait_semaphores;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_dst_stage_mask.data();
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer_;
  submit_info.signalSemaphoreCount = num_signal_semaphores;
  submit_info.pSignalSemaphores = signal_semaphores;

  VkResult result = VK_SUCCESS;

  VkFence fence;
  result = device_queue_->GetFenceHelper()->GetFence(&fence);
  if (VK_SUCCESS != result) {
    LOG(ERROR) << "Failed to create fence: " << result;
    return false;
  }

  result =
      vkQueueSubmit(device_queue_->GetVulkanQueue(), 1, &submit_info, fence);

  if (VK_SUCCESS != result) {
    vkDestroyFence(device_queue_->GetVulkanDevice(), fence, nullptr);
    submission_fence_ = VulkanFenceHelper::FenceHandle();
  } else {
    submission_fence_ = device_queue_->GetFenceHelper()->EnqueueFence(fence);
  }

  PostExecution();
  if (VK_SUCCESS != result) {
    LOG(ERROR) << "vkQueueSubmit() failed: " << result;
    return false;
  }

  return true;
}

void VulkanCommandBuffer::Enqueue(VkCommandBuffer primary_command_buffer) {
  DCHECK(!primary_);

  vkCmdExecuteCommands(primary_command_buffer, 1, &command_buffer_);
  PostExecution();
}

void VulkanCommandBuffer::Clear() {
  // Mark to reset upon next use.
  if (record_type_ != RECORD_TYPE_EMPTY)
    record_type_ = RECORD_TYPE_DIRTY;
}

void VulkanCommandBuffer::Wait(uint64_t timeout) {
  if (!submission_fence_.is_valid())
    return;

  device_queue_->GetFenceHelper()->Wait(submission_fence_, timeout);
}

bool VulkanCommandBuffer::SubmissionFinished() {
  if (!submission_fence_.is_valid())
    return true;

  return device_queue_->GetFenceHelper()->HasPassed(submission_fence_);
}

void VulkanCommandBuffer::TransitionImageLayout(
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    uint32_t src_queue_family_index,
    uint32_t dst_queue_family_index) {
  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = GetAccessMask(old_layout);
  barrier.dstAccessMask = GetAccessMask(new_layout);
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = src_queue_family_index;
  barrier.dstQueueFamilyIndex = dst_queue_family_index;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(command_buffer_,
                       GetPipelineStageFlags(device_queue_, old_layout),
                       GetPipelineStageFlags(device_queue_, new_layout), 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

void VulkanCommandBuffer::CopyBufferToImage(VkBuffer buffer,
                                            VkImage image,
                                            uint32_t buffer_width,
                                            uint32_t buffer_height,
                                            uint32_t width,
                                            uint32_t height,
                                            uint64_t buffer_offset) {
  VkBufferImageCopy region = {};
  region.bufferOffset = buffer_offset;
  region.bufferRowLength = buffer_width;
  region.bufferImageHeight = buffer_height;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};
  vkCmdCopyBufferToImage(command_buffer_, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void VulkanCommandBuffer::CopyImageToBuffer(VkBuffer buffer,
                                            VkImage image,
                                            uint32_t buffer_width,
                                            uint32_t buffer_height,
                                            uint32_t width,
                                            uint32_t height,
                                            uint64_t buffer_offset) {
  VkBufferImageCopy region = {};
  region.bufferOffset = buffer_offset;
  region.bufferRowLength = buffer_width;
  region.bufferImageHeight = buffer_height;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};
  vkCmdCopyImageToBuffer(command_buffer_, image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1,
                         &region);
}
void VulkanCommandBuffer::PostExecution() {
  if (record_type_ == RECORD_TYPE_SINGLE_USE) {
    // Clear upon next use.
    record_type_ = RECORD_TYPE_DIRTY;
  } else if (record_type_ == RECORD_TYPE_MULTI_USE) {
    // Can no longer record new items unless marked as clear.
    record_type_ = RECORD_TYPE_RECORDED;
  }
}

void VulkanCommandBuffer::ResetIfDirty() {
  DCHECK(!recording_);
  if (record_type_ == RECORD_TYPE_DIRTY) {
    // Block if command buffer is still in use. This can be externally avoided
    // using the asynchronous SubmissionFinished() function.
    Wait(UINT64_MAX);
    VkResult result = vkResetCommandBuffer(command_buffer_, 0);
    if (VK_SUCCESS != result) {
      LOG(ERROR) << "vkResetCommandBuffer() failed: " << result;
    } else {
      record_type_ = RECORD_TYPE_EMPTY;
    }
  }
}

CommandBufferRecorderBase::~CommandBufferRecorderBase() {
  VkResult result = vkEndCommandBuffer(handle_);
  if (VK_SUCCESS != result) {
    LOG(ERROR) << "vkEndCommandBuffer() failed: " << result;
  }
}

ScopedMultiUseCommandBufferRecorder::ScopedMultiUseCommandBufferRecorder(
    VulkanCommandBuffer& command_buffer)
    : CommandBufferRecorderBase(command_buffer) {
  ValidateMultiUse(command_buffer);
  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  VkResult result = vkBeginCommandBuffer(handle_, &begin_info);

  if (VK_SUCCESS != result) {
    LOG(ERROR) << "vkBeginCommandBuffer() failed: " << result;
  }
}

ScopedSingleUseCommandBufferRecorder::ScopedSingleUseCommandBufferRecorder(
    VulkanCommandBuffer& command_buffer)
    : CommandBufferRecorderBase(command_buffer) {
  ValidateSingleUse(command_buffer);
  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VkResult result = vkBeginCommandBuffer(handle_, &begin_info);

  if (VK_SUCCESS != result) {
    LOG(ERROR) << "vkBeginCommandBuffer() failed: " << result;
  }
}

}  // namespace gpu

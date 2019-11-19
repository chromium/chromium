// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_COMMAND_BUFFER_H_
#define GPU_VULKAN_VULKAN_COMMAND_BUFFER_H_

#include <vulkan/vulkan.h>

#include "base/logging.h"
#include "base/macros.h"
#include "gpu/vulkan/vulkan_export.h"
#include "gpu/vulkan/vulkan_fence_helper.h"

namespace gpu {

class VulkanCommandPool;
class VulkanDeviceQueue;

class VULKAN_EXPORT VulkanCommandBuffer {
 public:
  VulkanCommandBuffer(VulkanDeviceQueue* device_queue,
                      VulkanCommandPool* command_pool,
                      bool primary,
                      bool use_protected_memory);
  ~VulkanCommandBuffer();

  bool Initialize();
  // Destroy() should be called when all related GPU tasks have been finished.
  void Destroy();

  // Submit primary command buffer to the queue.
  bool Submit(uint32_t num_wait_semaphores,
              VkSemaphore* wait_semaphores,
              uint32_t num_signal_semaphores,
              VkSemaphore* signal_semaphores);

  // Enqueue secondary command buffer within a primary command buffer.
  void Enqueue(VkCommandBuffer primary_command_buffer);

  void Clear();

  // This blocks until the commands from the previous submit are done.
  void Wait(uint64_t timeout);

  // This simply tests asynchronously if the commands from the previous submit
  // is finished.
  bool SubmissionFinished();

  void TransitionImageLayout(VkImage image,
                             VkImageLayout old_layout,
                             VkImageLayout new_layout);
  void CopyBufferToImage(VkBuffer buffer,
                         VkImage image,
                         uint32_t buffer_width,
                         uint32_t buffer_height,
                         uint32_t width,
                         uint32_t height);

 private:
  friend class CommandBufferRecorderBase;

  enum RecordType {
    // Nothing has been recorded yet.
    RECORD_TYPE_EMPTY,

    // Recorded for single use, will be reset upon submission.
    RECORD_TYPE_SINGLE_USE,

    // Recording for multi use, once submitted it can't be modified until reset.
    RECORD_TYPE_MULTI_USE,

    // Recorded for multi-use, can no longer be modified unless reset.
    RECORD_TYPE_RECORDED,

    // Dirty, should be cleared before use. This assumes its externally
    // synchronized and the command buffer is no longer in use.
    RECORD_TYPE_DIRTY,
  };

  void PostExecution();
  void ResetIfDirty();

  const bool primary_;
  const bool use_protected_memory_;
  bool recording_ = false;
  RecordType record_type_ = RECORD_TYPE_EMPTY;
  VulkanDeviceQueue* device_queue_;
  VulkanCommandPool* command_pool_;
  VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
  VulkanFenceHelper::FenceHandle submission_fence_;

  DISALLOW_COPY_AND_ASSIGN(VulkanCommandBuffer);
};

class VULKAN_EXPORT CommandBufferRecorderBase {
 public:
  VkCommandBuffer handle() const { return handle_; }

 protected:
  CommandBufferRecorderBase(VulkanCommandBuffer& command_buffer)
      : handle_(command_buffer.command_buffer_) {
    command_buffer.ResetIfDirty();
  }

  virtual ~CommandBufferRecorderBase();

  void ValidateSingleUse(VulkanCommandBuffer& command_buffer) {
    DCHECK((VulkanCommandBuffer::RECORD_TYPE_SINGLE_USE ==
            command_buffer.record_type_) ||
           (VulkanCommandBuffer::RECORD_TYPE_EMPTY ==
            command_buffer.record_type_));
    command_buffer.record_type_ = VulkanCommandBuffer::RECORD_TYPE_SINGLE_USE;
  }

  void ValidateMultiUse(VulkanCommandBuffer& command_buffer) {
    DCHECK((VulkanCommandBuffer::RECORD_TYPE_MULTI_USE ==
            command_buffer.record_type_) ||
           (VulkanCommandBuffer::RECORD_TYPE_EMPTY ==
            command_buffer.record_type_));
    command_buffer.record_type_ = VulkanCommandBuffer::RECORD_TYPE_MULTI_USE;
  }

  VkCommandBuffer handle_;
};

class VULKAN_EXPORT ScopedMultiUseCommandBufferRecorder
    : public CommandBufferRecorderBase {
 public:
  ScopedMultiUseCommandBufferRecorder(VulkanCommandBuffer& command_buffer);
  ~ScopedMultiUseCommandBufferRecorder() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedMultiUseCommandBufferRecorder);
};

class VULKAN_EXPORT ScopedSingleUseCommandBufferRecorder
    : public CommandBufferRecorderBase {
 public:
  ScopedSingleUseCommandBufferRecorder(VulkanCommandBuffer& command_buffer);
  ~ScopedSingleUseCommandBufferRecorder() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSingleUseCommandBufferRecorder);
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_COMMAND_BUFFER_H_

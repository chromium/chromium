// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_ON_GPU_H_
#define GPU_IPC_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_ON_GPU_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/gl_in_process_context_export.h"

namespace gpu {
class CommandBufferTaskExecutor;

// This class holds ownership of data structure that is only used on the gpu
// thread. This class is expected to be 1:1 relationship with the display
// compositor.
class GL_IN_PROCESS_CONTEXT_EXPORT
    DisplayCompositorMemoryAndTaskControllerOnGpu {
 public:
  // Used for SkiaRenderer.
  explicit DisplayCompositorMemoryAndTaskControllerOnGpu(
      scoped_refptr<SharedContextState> shared_context_state);
  // Used for InProcessCommandBuffer.
  explicit DisplayCompositorMemoryAndTaskControllerOnGpu(
      CommandBufferTaskExecutor* task_executor);
  DisplayCompositorMemoryAndTaskControllerOnGpu(
      const DisplayCompositorMemoryAndTaskControllerOnGpu&) = delete;
  DisplayCompositorMemoryAndTaskControllerOnGpu& operator=(
      const DisplayCompositorMemoryAndTaskControllerOnGpu&) = delete;
  ~DisplayCompositorMemoryAndTaskControllerOnGpu();

  SharedContextState* shared_context_state() const {
    return shared_context_state_.get();
  }
  MemoryTracker* memory_tracker() const;
  CommandBufferId command_buffer_id() const { return command_buffer_id_; }
  // Used to create SharedImageInterface. Only used for in process command
  // buffer and shared image channels created for the display compositor in the
  // GPU process. Not Used for cross process shared image stub.
  static gpu::CommandBufferId NextCommandBufferId();

 private:
  scoped_refptr<SharedContextState> shared_context_state_;

  const CommandBufferId command_buffer_id_;

  // Only needed for InProcessCommandBuffer.
  bool should_have_memory_tracker_ = false;
  std::unique_ptr<MemoryTracker> memory_tracker_;

  SEQUENCE_CHECKER(gpu_sequence_checker_);
};

}  // namespace gpu

#endif  // GPU_IPC_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_ON_GPU_H_

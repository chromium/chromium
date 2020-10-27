// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/display_compositor_memory_and_task_controller_on_gpu.h"

#include "base/atomic_sequence_num.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/service/gpu_command_buffer_memory_tracker.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/service/gpu_channel_manager.h"

namespace gpu {

namespace {
// For generating command buffer id.
base::AtomicSequenceNumber g_next_shared_route_id;

CommandBufferId GenNextCommandBufferId() {
  return CommandBufferIdFromChannelAndRoute(
      kDisplayCompositorClientId, g_next_shared_route_id.GetNext() + 1);
}
}  // namespace

// Used for SkiaRenderer.
DisplayCompositorMemoryAndTaskControllerOnGpu::
    DisplayCompositorMemoryAndTaskControllerOnGpu(
        scoped_refptr<SharedContextState> shared_context_state)
    : shared_context_state_(std::move(shared_context_state)),
      command_buffer_id_(g_next_shared_route_id.GetNext() + 1),
      should_have_memory_tracker_(true) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
}

// Used for InProcessCommandBuffer.
DisplayCompositorMemoryAndTaskControllerOnGpu::
    DisplayCompositorMemoryAndTaskControllerOnGpu(
        CommandBufferTaskExecutor* task_executor)
    : shared_context_state_(task_executor->GetSharedContextState()),
      command_buffer_id_(GenNextCommandBufferId()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  // Android WebView won't have a memory tracker.
  if (task_executor->ShouldCreateMemoryTracker()) {
    should_have_memory_tracker_ = true;
    memory_tracker_ = std::make_unique<GpuCommandBufferMemoryTracker>(
        command_buffer_id_,
        base::trace_event::MemoryDumpManager::GetInstance()
            ->GetTracingProcessId(),
        base::ThreadTaskRunnerHandle::Get(),
        /* obserer=*/nullptr);
  } else {
    should_have_memory_tracker_ = false;
  }
}

DisplayCompositorMemoryAndTaskControllerOnGpu::
    ~DisplayCompositorMemoryAndTaskControllerOnGpu() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
}

MemoryTracker* DisplayCompositorMemoryAndTaskControllerOnGpu::memory_tracker()
    const {
  if (!should_have_memory_tracker_)
    return nullptr;

  if (memory_tracker_)
    return memory_tracker_.get();
  else
    return shared_context_state_->memory_tracker();
}

// Static
CommandBufferId
DisplayCompositorMemoryAndTaskControllerOnGpu::NextCommandBufferId() {
  return GenNextCommandBufferId();
}
}  // namespace gpu

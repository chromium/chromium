// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"

#include "base/atomic_sequence_num.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/command_buffer/service/gpu_command_buffer_memory_tracker.h"
#include "gpu/ipc/common/gpu_client_ids.h"

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
        scoped_refptr<SharedContextState> shared_context_state,
        SharedImageManager* shared_image_manager,
        SyncPointManager* sync_point_manager,
        const GpuPreferences& gpu_preferences,
        const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds,
        const GpuFeatureInfo& gpu_feature_info)
    : shared_context_state_(std::move(shared_context_state)),
      command_buffer_id_(g_next_shared_route_id.GetNext() + 1),
      shared_image_manager_(shared_image_manager),
      sync_point_manager_(sync_point_manager),
      gpu_preferences_(gpu_preferences),
      gpu_driver_bug_workarounds_(gpu_driver_bug_workarounds),
      gpu_feature_info_(gpu_feature_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
}

DisplayCompositorMemoryAndTaskControllerOnGpu::
    ~DisplayCompositorMemoryAndTaskControllerOnGpu() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
}

MemoryTracker* DisplayCompositorMemoryAndTaskControllerOnGpu::memory_tracker()
    const {
  DCHECK(shared_context_state_);
  return shared_context_state_->memory_tracker();
}

// Static
CommandBufferId
DisplayCompositorMemoryAndTaskControllerOnGpu::NextCommandBufferId() {
  return GenNextCommandBufferId();
}
}  // namespace gpu

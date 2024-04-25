// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_ON_GPU_H_
#define GPU_COMMAND_BUFFER_SERVICE_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_ON_GPU_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/command_buffer_id.h"

namespace gpu {
class SyncPointManager;
class SharedImageManager;
struct GpuFeatureInfo;
struct GpuPreferences;

// This class holds ownership of data structure that is only used on the gpu
// thread. This class is expected to be 1:1 relationship with the display
// compositor.
class GPU_GLES2_EXPORT DisplayCompositorMemoryAndTaskControllerOnGpu {
 public:
  DisplayCompositorMemoryAndTaskControllerOnGpu(
      scoped_refptr<SharedContextState> shared_context_state,
      SharedImageManager* shared_image_manager,
      SyncPointManager* sync_point_manager,
      const GpuPreferences& gpu_preferences,
      const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds,
      const GpuFeatureInfo& gpu_feature_info);
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

  SharedImageManager* shared_image_manager() const {
    return shared_image_manager_;
  }
  SyncPointManager* sync_point_manager() const { return sync_point_manager_; }
  const GpuPreferences& gpu_preferences() const { return *gpu_preferences_; }
  const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds() const {
    return gpu_driver_bug_workarounds_;
  }
  const GpuFeatureInfo& gpu_feature_info() const { return *gpu_feature_info_; }

 private:
  scoped_refptr<SharedContextState> shared_context_state_;

  const CommandBufferId command_buffer_id_;

  // Used for creating SharedImageFactory.
  raw_ptr<SharedImageManager> shared_image_manager_;
  raw_ptr<SyncPointManager> sync_point_manager_;
  const raw_ref<const GpuPreferences> gpu_preferences_;
  GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;
  const raw_ref<const GpuFeatureInfo> gpu_feature_info_;

  SEQUENCE_CHECKER(gpu_sequence_checker_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_ON_GPU_H_

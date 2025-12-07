// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process_base.h"
#include "gpu/command_buffer/service/task_graph.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class SharedContextState;
class SharedImageFactory;
class SharedImageManager;
class SingleTaskSequence;
struct GpuPreferences;
class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;
struct SyncToken;

// This is an implementation of the SharedImageInterface to be used on the viz
// compositor thread. This class also implements the corresponding parts
// happening on gpu thread.
class GPU_GLES2_EXPORT SharedImageInterfaceInProcess
    : public SharedImageInterfaceInProcessBase {
 public:
  // Callers must guarantee that the instances passed via pointers are kept
  // alive for as long as the instance of this class is alive. This can be
  // achieved by ensuring that the ownership of the created
  // SharedImageInterfaceInProcess is the same as the ownership of the passed in
  // pointers. The `gpu_task_runner` is the task runner that `ScheduleGpuTask()`
  // schedules on; it is used to ensure that some parts of initialization and
  // destruction happen on the GPU thread.
  static scoped_refptr<SharedImageInterfaceInProcess> Create(
      SingleTaskSequence* task_sequence,
      const GpuPreferences& gpu_preferences,
      const GpuDriverBugWorkarounds& gpu_workarounds,
      const GpuFeatureInfo& gpu_feature_info,
      gpu::SharedContextState* context_state,
      SharedImageManager* shared_image_manager,
      bool is_for_display_compositor,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      bool always_create_native_gmb_handles = false);

  SharedImageInterfaceInProcess(const SharedImageInterfaceInProcess&) = delete;
  SharedImageInterfaceInProcess& operator=(
      const SharedImageInterfaceInProcess&) = delete;

  // SharedImageInterface:
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      std::optional<SharedImagePoolId> pool_id) override;

 protected:
  ~SharedImageInterfaceInProcess() override;

  // SharedImageInterfaceBase:
  SharedImageFactory* GetSharedImageFactoryOnGpuThread() override;
  bool MakeContextCurrentOnGpuThread(bool needs_gl) override;
  using SharedImageInterfaceInProcessBase::MakeContextCurrentOnGpuThread;
  void MarkContextLostOnGpuThread() override;
  void ScheduleGpuTask(base::OnceClosure task,
                       std::vector<SyncToken> sync_token_fences,
                       const SyncToken& release) override;

 private:
  // Private to ensure `Initialize()` is always called after construction.
  SharedImageInterfaceInProcess(
      SingleTaskSequence* task_sequence,
      SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner);

  // Parameters needed to be passed in to set up the class on the GPU.
  // Needed since we cannot pass refcounted instances (e.g.
  // gpu::SharedContextState) to base::BindOnce as raw pointers.
  struct SetUpOnGpuParams;

  // Set up GPU state.
  void Initialize(std::unique_ptr<SetUpOnGpuParams> params);

  void SetUpOnGpu(std::unique_ptr<SetUpOnGpuParams> params);
  void DestroyOnGpu(base::WaitableEvent* completion);

  // Used to schedule work on the gpu thread. This is a raw pointer for now
  // since the ownership of SingleTaskSequence would be the same as the
  // SharedImageInterfaceInProcess.
  raw_ptr<SingleTaskSequence> task_sequence_;

  base::OnceCallback<std::unique_ptr<SharedImageFactory>()> create_factory_;

  // Accessed on any thread.
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // Accessed on compositor thread.
  // This is used to get NativePixmap, and is only used when SharedImageManager
  // is thread safe.
  raw_ptr<SharedImageManager> shared_image_manager_;

  // Accessed on GPU thread.
  scoped_refptr<SharedContextState> context_state_;
  ScopedSyncPointClientState sync_point_client_state_;
  std::unique_ptr<SharedImageFactory> shared_image_factory_;
  bool always_create_native_gmb_handles_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_

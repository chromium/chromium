// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_interface_in_process.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process_base.h"
#include "gpu/command_buffer/service/single_task_sequence.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {

struct SharedImageInterfaceInProcess::SetUpOnGpuParams {
  const GpuPreferences gpu_preferences;
  const GpuDriverBugWorkarounds gpu_workarounds;
  const GpuFeatureInfo gpu_feature_info;
  const raw_ptr<gpu::SharedContextState> context_state;
  const raw_ptr<SharedImageManager> shared_image_manager;
  const bool is_for_display_compositor;

  SetUpOnGpuParams(const GpuPreferences& gpu_preferences,
                   const GpuDriverBugWorkarounds& gpu_workarounds,
                   const GpuFeatureInfo& gpu_feature_info,
                   gpu::SharedContextState* context_state,
                   SharedImageManager* shared_image_manager,
                   bool is_for_display_compositor)
      : gpu_preferences(gpu_preferences),
        gpu_workarounds(gpu_workarounds),
        gpu_feature_info(gpu_feature_info),
        context_state(context_state),
        shared_image_manager(shared_image_manager),
        is_for_display_compositor(is_for_display_compositor) {}

  ~SetUpOnGpuParams() = default;

  SetUpOnGpuParams(const SetUpOnGpuParams& other) = delete;
  SetUpOnGpuParams& operator=(const SetUpOnGpuParams& other) = delete;
};

scoped_refptr<SharedImageInterfaceInProcess>
SharedImageInterfaceInProcess::Create(
    SingleTaskSequence* task_sequence,
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& gpu_workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    gpu::SharedContextState* context_state,
    SharedImageManager* shared_image_manager,
    bool is_for_display_compositor,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    bool always_create_native_gmb_handles /*=false*/) {
  // ensure Initialize() is called before pointer returned to caller
  auto sii = base::WrapRefCounted(new SharedImageInterfaceInProcess{
      task_sequence, shared_image_manager, std::move(gpu_task_runner)});
  sii->Initialize(std::make_unique<SetUpOnGpuParams>(
      gpu_preferences, gpu_workarounds, gpu_feature_info, context_state,
      shared_image_manager, is_for_display_compositor));

  if (always_create_native_gmb_handles) {
    // Creation with this option can be done only on the GPU thread, since we
    // must construct the SharedImageFactory eagerly to ensure that it is
    // immediately available to create GMB handles on the IO thread.
    CHECK(sii->gpu_task_runner_->BelongsToCurrentThread());
    sii->GetSharedImageFactoryOnGpuThread();
    sii->always_create_native_gmb_handles_ = true;
  }
  return sii;
}

SharedImageInterfaceInProcess::SharedImageInterfaceInProcess(
    SingleTaskSequence* task_sequence,
    SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner)
    : SharedImageInterfaceInProcessBase(
          CommandBufferNamespace::IN_PROCESS,
          DisplayCompositorMemoryAndTaskControllerOnGpu::NextCommandBufferId(),
          /*verify_creation_sync_token=*/false),
      task_sequence_(task_sequence),
      gpu_task_runner_(std::move(gpu_task_runner)),
      shared_image_manager_(shared_image_manager) {}

void SharedImageInterfaceInProcess::Initialize(
    std::unique_ptr<SetUpOnGpuParams> params) {
  if (gpu_task_runner_->BelongsToCurrentThread()) {
    SetUpOnGpu(std::move(params));
  } else {
    // Can't safely be called in constructor, because receiver must be
    // retained, but constructor has zero ref-count
    task_sequence_->ScheduleTask(
        base::BindOnce(&SharedImageInterfaceInProcess::SetUpOnGpu, this,
                       std::move(params)),
        /*sync_token_fences=*/{}, SyncToken());
  }
}

SharedImageInterfaceInProcess::~SharedImageInterfaceInProcess() {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  if (gpu_task_runner_->BelongsToCurrentThread()) {
    DestroyOnGpu(&completion);
  } else {
    // Unretained because called in destructor, where ref-count is always zero;
    // safe because destructor is blocked on `completion` until async task runs
    task_sequence_->ScheduleTask(
        base::BindOnce(&SharedImageInterfaceInProcess::DestroyOnGpu,
                       base::Unretained(this), &completion),
        /*sync_token_fences=*/{}, SyncToken());
  }

  completion.Wait();
}

void SharedImageInterfaceInProcess::SetUpOnGpu(
    std::unique_ptr<SetUpOnGpuParams> params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  context_state_ = params->context_state.get();

  create_factory_ = base::BindOnce(
      [](std::unique_ptr<SetUpOnGpuParams> params) {
        auto* memory_tracker = params->context_state
                                   ? params->context_state->memory_tracker()
                                   : nullptr;
        auto shared_image_factory = std::make_unique<SharedImageFactory>(
            params->gpu_preferences, params->gpu_workarounds,
            params->gpu_feature_info, params->context_state,
            params->shared_image_manager, memory_tracker,
            params->is_for_display_compositor);
        return shared_image_factory;
      },
      std::move(params));

  // Make the SharedImageInterface use the same sequence as the command buffer,
  // it's necessary for WebView because of the blocking behavior.
  // TODO(piman): see if it's worth using a different sequence for non-WebView.
  sync_point_client_state_ = task_sequence_->CreateSyncPointClientState(
      CommandBufferNamespace::IN_PROCESS, command_buffer_id());
}

void SharedImageInterfaceInProcess::DestroyOnGpu(
    base::WaitableEvent* completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  bool have_context = MakeContextCurrentOnGpuThread();
  if (shared_image_factory_) {
    shared_image_factory_->DestroyAllSharedImages(have_context);
    shared_image_factory_ = nullptr;
  }

  sync_point_client_state_.Reset();

  context_state_ = nullptr;
  completion->Signal();
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    std::optional<SharedImagePoolId> pool_id) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));

  if (always_create_native_gmb_handles_) {
#if BUILDFLAG(IS_ANDROID)
    // Creation of native buffer handles is not supported on Android (the
    // only way that a non-null GpuMemoryBufferHandle can be created on
    // Android is by importing an external AHB).
    return nullptr;
#else
    // The below method doesn't (yet?) take in pool IDs.
    CHECK(!pool_id);

    // The SIFactory may be null if it was not possible for it to be created in
    // the constructor (because the context was lost). In this case, there is
    // nothing possible to do (context loss is sticky and will eventually be
    // resolved by recreating the SII in one way or another).
    if (!shared_image_factory_) {
      return nullptr;
    }
    auto gmb_handle = shared_image_factory_->CreateNativeGpuMemoryBufferHandle(
        si_info.meta.size, si_info.meta.format, buffer_usage);
    if (gmb_handle.is_null()) {
      return nullptr;
    }
    return SharedImageInterfaceInProcessBase::CreateSharedImage(
        si_info, surface_handle, buffer_usage, std::move(gmb_handle));
#endif
  }

  return SharedImageInterfaceInProcessBase::CreateSharedImage(
      si_info, surface_handle, buffer_usage, pool_id);
}

SharedImageFactory*
SharedImageInterfaceInProcess::GetSharedImageFactoryOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (shared_image_factory_) {
    return shared_image_factory_.get();
  }

  // Some shared image backing factories will use GL in ctor, so we need GL even
  // if chrome is using non-GL backing.
  if (!MakeContextCurrentOnGpuThread(/*needs_gl=*/true)) {
    return nullptr;
  }

  shared_image_factory_ = std::move(create_factory_).Run();
  return shared_image_factory_.get();
}

bool SharedImageInterfaceInProcess::MakeContextCurrentOnGpuThread(
    bool needs_gl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (gl::GetGLImplementation() == gl::kGLImplementationDisabled) {
    return true;
  }

  if (!context_state_)
    return false;

  if (context_state_->context_lost())
    return false;

  // |shared_image_factory_| never writes to the surface, so skip unnecessary
  // MakeCurrent to improve performance. https://crbug.com/457431
  auto* context = context_state_->real_context();
  if (context->IsCurrent(nullptr))
    return !context_state_->CheckResetStatus(needs_gl);
  return context_state_->MakeCurrent(/*surface=*/nullptr, needs_gl);
}

void SharedImageInterfaceInProcess::MarkContextLostOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  context_state_->MarkContextLost();
}

void SharedImageInterfaceInProcess::ScheduleGpuTask(
    base::OnceClosure task,
    std::vector<SyncToken> sync_token_fences,
    const SyncToken& release) {
  task_sequence_->ScheduleTask(std::move(task), std::move(sync_token_fences),
                               release);
}

}  // namespace gpu

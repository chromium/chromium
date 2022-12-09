// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_interface_in_process.h"

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/command_buffer/service/gpu_command_buffer_memory_tracker.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/single_task_sequence.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "ui/gl/gl_context.h"

namespace gpu {

struct SharedImageInterfaceInProcess::SetUpOnGpuParams {
  const GpuPreferences gpu_preferences;
  const GpuDriverBugWorkarounds gpu_workarounds;
  const GpuFeatureInfo gpu_feature_info;
  const raw_ptr<gpu::SharedContextState> context_state;
  const raw_ptr<SharedImageManager> shared_image_manager;
  const raw_ptr<ImageFactory> image_factory;
  const bool is_for_display_compositor;

  SetUpOnGpuParams(const GpuPreferences& gpu_preferences,
                   const GpuDriverBugWorkarounds& gpu_workarounds,
                   const GpuFeatureInfo& gpu_feature_info,
                   gpu::SharedContextState* context_state,
                   SharedImageManager* shared_image_manager,
                   ImageFactory* image_factory,
                   bool is_for_display_compositor)
      : gpu_preferences(gpu_preferences),
        gpu_workarounds(gpu_workarounds),
        gpu_feature_info(gpu_feature_info),
        context_state(context_state),
        shared_image_manager(shared_image_manager),
        image_factory(image_factory),
        is_for_display_compositor(is_for_display_compositor) {}

  ~SetUpOnGpuParams() = default;

  SetUpOnGpuParams(const SetUpOnGpuParams& other) = delete;
  SetUpOnGpuParams& operator=(const SetUpOnGpuParams& other) = delete;
};

SharedImageInterfaceInProcess::SharedImageInterfaceInProcess(
    SingleTaskSequence* task_sequence,
    DisplayCompositorMemoryAndTaskControllerOnGpu* display_controller)
    : SharedImageInterfaceInProcess(
          task_sequence,
          display_controller->sync_point_manager(),
          display_controller->gpu_preferences(),
          display_controller->gpu_driver_bug_workarounds(),
          display_controller->gpu_feature_info(),
          display_controller->shared_context_state(),
          display_controller->shared_image_manager(),
          display_controller->image_factory(),
          /*is_for_display_compositor=*/true) {}

SharedImageInterfaceInProcess::SharedImageInterfaceInProcess(
    SingleTaskSequence* task_sequence,
    SyncPointManager* sync_point_manager,
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& gpu_workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    gpu::SharedContextState* context_state,
    SharedImageManager* shared_image_manager,
    ImageFactory* image_factory,
    bool is_for_display_compositor)
    : task_sequence_(task_sequence),
      command_buffer_id_(
          DisplayCompositorMemoryAndTaskControllerOnGpu::NextCommandBufferId()),
      shared_image_manager_(shared_image_manager),
      sync_point_manager_(sync_point_manager) {
  DETACH_FROM_SEQUENCE(gpu_sequence_checker_);
  task_sequence_->ScheduleTask(
      base::BindOnce(
          &SharedImageInterfaceInProcess::SetUpOnGpu, base::Unretained(this),
          std::make_unique<SetUpOnGpuParams>(
              gpu_preferences, gpu_workarounds, gpu_feature_info, context_state,
              shared_image_manager, image_factory, is_for_display_compositor)),
      {});
}

SharedImageInterfaceInProcess::~SharedImageInterfaceInProcess() {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  task_sequence_->ScheduleTask(
      base::BindOnce(&SharedImageInterfaceInProcess::DestroyOnGpu,
                     base::Unretained(this), &completion),
      {});
  completion.Wait();
}
void SharedImageInterfaceInProcess::SetUpOnGpu(
    std::unique_ptr<SetUpOnGpuParams> params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  context_state_ = params->context_state.get();

  create_factory_ = base::BindOnce(
      [](std::unique_ptr<SetUpOnGpuParams> params) {
        auto shared_image_factory = std::make_unique<SharedImageFactory>(
            params->gpu_preferences, params->gpu_workarounds,
            params->gpu_feature_info, params->context_state,
            params->shared_image_manager, params->image_factory,
            params->context_state->memory_tracker(),
            params->is_for_display_compositor);
        return shared_image_factory;
      },
      std::move(params));

  // Make the SharedImageInterface use the same sequence as the command buffer,
  // it's necessary for WebView because of the blocking behavior.
  // TODO(piman): see if it's worth using a different sequence for non-WebView.
  sync_point_client_state_ = sync_point_manager_->CreateSyncPointClientState(
      CommandBufferNamespace::IN_PROCESS, command_buffer_id_,
      task_sequence_->GetSequenceId());
}

void SharedImageInterfaceInProcess::DestroyOnGpu(
    base::WaitableEvent* completion) {
  bool have_context = MakeContextCurrent();
  if (shared_image_factory_) {
    shared_image_factory_->DestroyAllSharedImages(have_context);
    shared_image_factory_ = nullptr;
  }

  if (sync_point_client_state_) {
    sync_point_client_state_->Destroy();
    sync_point_client_state_ = nullptr;
  }

  context_state_ = nullptr;
  completion->Signal();
}

bool SharedImageInterfaceInProcess::MakeContextCurrent(bool needs_gl) {
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

bool SharedImageInterfaceInProcess::LazyCreateSharedImageFactory() {
  if (shared_image_factory_)
    return true;

  // Some shared image backing factories will use GL in ctor, so we need GL even
  // if chrome is using non-GL backing.
  if (!MakeContextCurrent(/*needs_gl=*/true))
    return false;

  shared_image_factory_ = std::move(create_factory_).Run();
  return true;
}

Mailbox SharedImageInterfaceInProcess::CreateSharedImage(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gpu::SurfaceHandle surface_handle) {
  DCHECK(gpu::IsValidClientUsage(usage));
  auto mailbox = Mailbox::GenerateForSharedImage();
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // SharedImageInterfaceInProcess synchronizes with the GPU thread at
    // destruction time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(
        base::BindOnce(
            &SharedImageInterfaceInProcess::CreateSharedImageOnGpuThread,
            base::Unretained(this), mailbox, format, surface_handle, size,
            color_space, surface_origin, alpha_type, usage,
            MakeSyncToken(next_fence_sync_release_++)),
        {});
  }
  return mailbox;
}

void SharedImageInterfaceInProcess::CreateSharedImageOnGpuThread(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    gpu::SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory())
    return;

  if (!MakeContextCurrent())
    return;

  DCHECK(shared_image_factory_);
  auto si_format = viz::SharedImageFormat::SinglePlane(format);
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, si_format, size, color_space, surface_origin, alpha_type,
          surface_handle, usage)) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

Mailbox SharedImageInterfaceInProcess::CreateSharedImage(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  DCHECK(gpu::IsValidClientUsage(usage));
  auto mailbox = Mailbox::GenerateForSharedImage();
  std::vector<uint8_t> pixel_data_copy(pixel_data.begin(), pixel_data.end());
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(
        base::BindOnce(&SharedImageInterfaceInProcess::
                           CreateSharedImageWithDataOnGpuThread,
                       base::Unretained(this), mailbox, format, size,
                       color_space, surface_origin, alpha_type, usage,
                       MakeSyncToken(next_fence_sync_release_++),
                       std::move(pixel_data_copy)),
        {});
  }
  return mailbox;
}

void SharedImageInterfaceInProcess::CreateSharedImageWithDataOnGpuThread(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    const SyncToken& sync_token,
    std::vector<uint8_t> pixel_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory())
    return;

  if (!MakeContextCurrent())
    return;

  DCHECK(shared_image_factory_);
  auto si_format = viz::SharedImageFormat::SinglePlane(format);
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, si_format, size, color_space, surface_origin, alpha_type,
          usage, pixel_data)) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

Mailbox SharedImageInterfaceInProcess::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  NOTREACHED();
  return Mailbox();
}

Mailbox SharedImageInterfaceInProcess::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferPlane plane,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  DCHECK(gpu::IsValidClientUsage(usage));
  DCHECK(gpu_memory_buffer->GetType() == gfx::NATIVE_PIXMAP ||
         gpu_memory_buffer->GetType() == gfx::ANDROID_HARDWARE_BUFFER ||
         gpu_memory_buffer_manager);

  // TODO(piman): DCHECK GMB format support.
  DCHECK(IsImageSizeValidForGpuMemoryBufferFormat(
      gpu_memory_buffer->GetSize(), gpu_memory_buffer->GetFormat()));
  DCHECK(IsPlaneValidForGpuMemoryBufferFormat(plane,
                                              gpu_memory_buffer->GetFormat()));

  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->CloneHandle();
  bool requires_sync_token = handle.type == gfx::IO_SURFACE_BUFFER;
  SyncToken sync_token;
  {
    base::AutoLock lock(lock_);
    sync_token = MakeSyncToken(next_fence_sync_release_++);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(
        base::BindOnce(
            &SharedImageInterfaceInProcess::CreateGMBSharedImageOnGpuThread,
            base::Unretained(this), mailbox, std::move(handle),
            gpu_memory_buffer->GetFormat(), plane, gpu_memory_buffer->GetSize(),
            color_space, surface_origin, alpha_type, usage, sync_token),
        {});
  }
  if (requires_sync_token) {
    sync_token.SetVerifyFlush();
    gpu_memory_buffer_manager->SetDestructionSyncToken(gpu_memory_buffer,
                                                       sync_token);
  }
  return mailbox;
}

void SharedImageInterfaceInProcess::CreateGMBSharedImageOnGpuThread(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory())
    return;

  if (!MakeContextCurrent())
    return;

  DCHECK(shared_image_factory_);
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, kDisplayCompositorClientId, std::move(handle), format, plane,
          size, color_space, surface_origin, alpha_type, usage)) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

SharedImageInterface::SwapChainMailboxes
SharedImageInterfaceInProcess::CreateSwapChain(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  NOTREACHED();
  return {};
}

void SharedImageInterfaceInProcess::PresentSwapChain(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  NOTREACHED();
}

#if BUILDFLAG(IS_FUCHSIA)
void SharedImageInterfaceInProcess::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_FUCHSIA)

void SharedImageInterfaceInProcess::UpdateSharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  UpdateSharedImage(sync_token, nullptr, mailbox);
}

void SharedImageInterfaceInProcess::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  DCHECK(!acquire_fence);
  base::AutoLock lock(lock_);
  // Note: we enqueue the task under the lock to guarantee monotonicity of
  // the release ids as seen by the service. Unretained is safe because
  // InProcessCommandBuffer synchronizes with the GPU thread at destruction
  // time, cancelling tasks, before |this| is destroyed.
  ScheduleGpuTask(
      base::BindOnce(
          &SharedImageInterfaceInProcess::UpdateSharedImageOnGpuThread,
          base::Unretained(this), mailbox,
          MakeSyncToken(next_fence_sync_release_++)),
      {sync_token});
}

void SharedImageInterfaceInProcess::UpdateSharedImageOnGpuThread(
    const Mailbox& mailbox,
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeContextCurrent())
    return;

  if (!shared_image_factory_ ||
      !shared_image_factory_->UpdateSharedImage(mailbox)) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

void SharedImageInterfaceInProcess::DestroySharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  // Use sync token dependency to ensure that the destroy task does not run
  // before sync token is released.
  ScheduleGpuTask(
      base::BindOnce(
          &SharedImageInterfaceInProcess::DestroySharedImageOnGpuThread,
          base::Unretained(this), mailbox),
      {sync_token});
}

void SharedImageInterfaceInProcess::DestroySharedImageOnGpuThread(
    const Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeContextCurrent())
    return;

  if (!shared_image_factory_ ||
      !shared_image_factory_->DestroySharedImage(mailbox)) {
    context_state_->MarkContextLost();
  }
}

void SharedImageInterfaceInProcess::WaitSyncTokenOnGpuThread(
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeContextCurrent())
    return;

  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

SyncToken SharedImageInterfaceInProcess::GenUnverifiedSyncToken() {
  base::AutoLock lock(lock_);
  return MakeSyncToken(next_fence_sync_release_ - 1);
}

SyncToken SharedImageInterfaceInProcess::GenVerifiedSyncToken() {
  base::AutoLock lock(lock_);
  SyncToken sync_token = MakeSyncToken(next_fence_sync_release_ - 1);
  sync_token.SetVerifyFlush();
  return sync_token;
}

void SharedImageInterfaceInProcess::WaitSyncToken(const SyncToken& sync_token) {
  base::AutoLock lock(lock_);

  ScheduleGpuTask(
      base::BindOnce(&SharedImageInterfaceInProcess::WaitSyncTokenOnGpuThread,
                     base::Unretained(this),
                     MakeSyncToken(next_fence_sync_release_++)),
      {sync_token});
}

void SharedImageInterfaceInProcess::Flush() {
  // No need to flush in this implementation.
}

scoped_refptr<gfx::NativePixmap> SharedImageInterfaceInProcess::GetNativePixmap(
    const gpu::Mailbox& mailbox) {
  DCHECK(shared_image_manager_->is_thread_safe());
  return shared_image_manager_->GetNativePixmap(mailbox);
}

void SharedImageInterfaceInProcess::ScheduleGpuTask(
    base::OnceClosure task,
    std::vector<SyncToken> sync_token_fences) {
  task_sequence_->ScheduleTask(std::move(task), std::move(sync_token_fences));
}

}  // namespace gpu

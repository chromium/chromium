// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_interface_in_process.h"

#include <optional>
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/command_buffer/service/gpu_command_buffer_memory_tracker.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
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
          /*is_for_display_compositor=*/true) {}

SharedImageInterfaceInProcess::SharedImageInterfaceInProcess(
    SingleTaskSequence* task_sequence,
    SyncPointManager* sync_point_manager,
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& gpu_workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    gpu::SharedContextState* context_state,
    SharedImageManager* shared_image_manager,
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
              shared_image_manager, is_for_display_compositor)),
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

const SharedImageCapabilities&
SharedImageInterfaceInProcess::GetCapabilities() {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  if (!shared_image_capabilities_) {
    shared_image_capabilities_ = std::make_unique<SharedImageCapabilities>();
    task_sequence_->ScheduleTask(
        base::BindOnce(&SharedImageInterfaceInProcess::GetCapabilitiesOnGpu,
                       base::Unretained(this), &completion,
                       shared_image_capabilities_.get()),
        {});
    completion.Wait();
  }
  return *shared_image_capabilities_;
}

void SharedImageInterfaceInProcess::GetCapabilitiesOnGpu(
    base::WaitableEvent* completion,
    SharedImageCapabilities* out_capabilities) {
  if (!LazyCreateSharedImageFactory()) {
    return;
  }

  DCHECK(shared_image_factory_);
  *out_capabilities = shared_image_factory_->MakeCapabilities();
  completion->Signal();
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
            params->shared_image_manager,
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

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
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
            std::string(debug_label),
            MakeSyncToken(next_fence_sync_release_++)),
        {});
  }
  return base::MakeRefCounted<ClientSharedImage>(mailbox);
}

void SharedImageInterfaceInProcess::CreateSharedImageOnGpuThread(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    gpu::SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory())
    return;

  if (!MakeContextCurrent())
    return;

  DCHECK(shared_image_factory_);
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, format, size, color_space, surface_origin, alpha_type,
          surface_handle, usage, std::string(debug_label))) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
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
    ScheduleGpuTask(base::BindOnce(&SharedImageInterfaceInProcess::
                                       CreateSharedImageWithDataOnGpuThread,
                                   base::Unretained(this), mailbox, format,
                                   size, color_space, surface_origin,
                                   alpha_type, usage, std::string(debug_label),
                                   MakeSyncToken(next_fence_sync_release_++),
                                   std::move(pixel_data_copy)),
                    {});
  }
  return base::MakeRefCounted<ClientSharedImage>(mailbox);
}

void SharedImageInterfaceInProcess::CreateSharedImageWithDataOnGpuThread(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    const SyncToken& sync_token,
    std::vector<uint8_t> pixel_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory())
    return;

  if (!MakeContextCurrent())
    return;

  DCHECK(shared_image_factory_);
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, format, size, color_space, surface_origin, alpha_type, usage,
          std::move(debug_label), pixel_data)) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  DCHECK(gpu::IsValidClientUsage(usage));
  auto mailbox = Mailbox::GenerateForSharedImage();
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(
        base::BindOnce(&SharedImageInterfaceInProcess::
                           CreateSharedImageWithBufferUsageOnGpuThread,
                       base::Unretained(this), mailbox, format, size,
                       color_space, surface_origin, alpha_type, usage,
                       std::string(debug_label), surface_handle, buffer_usage,
                       MakeSyncToken(next_fence_sync_release_++)),
        {});
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, GetGpuMemoryBufferHandleInfo(mailbox));
}

void SharedImageInterfaceInProcess::CreateSharedImageWithBufferUsageOnGpuThread(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory()) {
    return;
  }

  if (!MakeContextCurrent()) {
    return;
  }

  DCHECK(shared_image_factory_);

  // Note that SharedImageInterfaceInProcess implementation here uses
  // SharedImageFactory::CreateSharedImage() to create a shared image backed by
  // native buffer/shared memory in GPU process. This is different
  // implementation and code path compared to ClientSharedImage implementation
  // which creates native buffer/shared memory on IO thread and then creates a
  // mailbox from it on GPU thread.
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, format, size, color_space, surface_origin, alpha_type,
          surface_handle, usage, std::move(debug_label), buffer_usage)) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

GpuMemoryBufferHandleInfo
SharedImageInterfaceInProcess::GetGpuMemoryBufferHandleInfo(
    const Mailbox& mailbox) {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  gfx::GpuMemoryBufferHandle handle;
  viz::SharedImageFormat format;
  gfx::Size size;
  gfx::BufferUsage buffer_usage;

  task_sequence_->ScheduleTask(
      base::BindOnce(&SharedImageInterfaceInProcess::
                         GetGpuMemoryBufferHandleInfoOnGpuThread,
                     base::Unretained(this), mailbox, &handle, &format, &size,
                     &buffer_usage, &completion),
      {});
  completion.Wait();
  return GpuMemoryBufferHandleInfo(std::move(handle), format, size,
                                   buffer_usage);
}

void SharedImageInterfaceInProcess::GetGpuMemoryBufferHandleInfoOnGpuThread(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle* handle,
    viz::SharedImageFormat* format,
    gfx::Size* size,
    gfx::BufferUsage* buffer_usage,
    base::WaitableEvent* completion) {
  base::ScopedClosureRunner completion_runner(base::BindOnce(
      [](base::WaitableEvent* completion) { completion->Signal(); },
      completion));

  DCHECK(shared_image_factory_);

  if (!mailbox.IsSharedImage()) {
    LOG(ERROR) << "SharedImageInterfaceInProcess: Trying to access a "
                  "SharedImage with a "
                  "non-SharedImage mailbox.";
    return;
  }

  // Note that we are not making |context_state_| current here as of now since
  // it is not needed to get the handle from the backings. Make context current
  // if we find that it is required.
  if (!shared_image_factory_->GetGpuMemoryBufferHandleInfo(
          mailbox, *handle, *format, *size, *buffer_usage)) {
    LOG(ERROR)
        << "SharedImageInterfaceInProcess: Unable to get GpuMemoryBufferHandle";
  }
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  auto client_buffer_handle = buffer_handle.Clone();
  auto mailbox =
      CreateSharedImage(format, size, color_space, surface_origin, alpha_type,
                        usage, debug_label, std::move(buffer_handle))
          ->mailbox();

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, GpuMemoryBufferHandleInfo(std::move(client_buffer_handle),
                                         format, size, buffer_usage));
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK(gpu::IsValidClientUsage(usage));

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  CHECK(!format.PrefersExternalSampler());
#endif

  auto mailbox = Mailbox::GenerateForSharedImage();
  {
    base::AutoLock lock(lock_);
    SyncToken sync_token = MakeSyncToken(next_fence_sync_release_++);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(base::BindOnce(&SharedImageInterfaceInProcess::
                                       CreateSharedImageWithBufferOnGpuThread,
                                   base::Unretained(this), mailbox, format,
                                   size, color_space, surface_origin,
                                   alpha_type, usage, std::move(buffer_handle),
                                   std::string(debug_label), sync_token),
                    {});
  }

  return base::MakeRefCounted<ClientSharedImage>(mailbox);
}

void SharedImageInterfaceInProcess::CreateSharedImageWithBufferOnGpuThread(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gfx::GpuMemoryBufferHandle buffer_handle,
    std::string debug_label,
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory()) {
    return;
  }

  if (!MakeContextCurrent()) {
    return;
  }

  DCHECK(shared_image_factory_);
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, format, size, color_space, surface_origin, alpha_type, usage,
          std::move(debug_label), std::move(buffer_handle))) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferPlane plane,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label) {
  DCHECK(gpu::IsValidClientUsage(usage));
  // TODO(piman): DCHECK GMB format support.
  DCHECK(IsImageSizeValidForGpuMemoryBufferFormat(
      gpu_memory_buffer->GetSize(), gpu_memory_buffer->GetFormat()));
  DCHECK(IsPlaneValidForGpuMemoryBufferFormat(plane,
                                              gpu_memory_buffer->GetFormat()));

  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->CloneHandle();
  {
    base::AutoLock lock(lock_);
    SyncToken sync_token = MakeSyncToken(next_fence_sync_release_++);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(
        base::BindOnce(
            &SharedImageInterfaceInProcess::CreateGMBSharedImageOnGpuThread,
            base::Unretained(this), mailbox, std::move(handle),
            gpu_memory_buffer->GetFormat(), plane, gpu_memory_buffer->GetSize(),
            color_space, surface_origin, alpha_type, usage,
            std::string(debug_label), sync_token),
        {});
  }

  return base::MakeRefCounted<ClientSharedImage>(mailbox);
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
    std::string debug_label,
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory())
    return;

  if (!MakeContextCurrent())
    return;

  DCHECK(shared_image_factory_);
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, std::move(handle), format, plane, size, color_space,
          surface_origin, alpha_type, usage, std::move(debug_label))) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

SharedImageInterface::SwapChainMailboxes
SharedImageInterfaceInProcess::CreateSwapChain(
    viz::SharedImageFormat format,
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

void SharedImageInterfaceInProcess::DestroySharedImage(
    const SyncToken& sync_token,
    scoped_refptr<ClientSharedImage> client_shared_image) {
  CHECK(client_shared_image->HasOneRef());
  // Use sync token dependency to ensure that the destroy task does not run
  // before sync token is released.
  ScheduleGpuTask(
      base::BindOnce(
          &SharedImageInterfaceInProcess::DestroyClientSharedImageOnGpuThread,
          base::Unretained(this), std::move(client_shared_image)),
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

void SharedImageInterfaceInProcess::DestroyClientSharedImageOnGpuThread(
    scoped_refptr<ClientSharedImage> client_shared_image) {
  DestroySharedImageOnGpuThread(client_shared_image->mailbox());
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

void SharedImageInterfaceInProcess::AddReferenceToSharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox,
    uint32_t usage) {
  // Secondary references are required only by client processes, so it shouldn't
  // be reachable here.
  NOTREACHED();
}

}  // namespace gpu

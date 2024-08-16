// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_interface_in_process.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/process/memory.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
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
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gfx/buffer_format_util.h"
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
    bool is_for_display_compositor,
    OwnerThread owner_thread)
    : task_sequence_(task_sequence),
      command_buffer_id_(
          DisplayCompositorMemoryAndTaskControllerOnGpu::NextCommandBufferId()),
      shared_image_manager_(shared_image_manager),
      sync_point_manager_(sync_point_manager),
      owner_thread_(owner_thread) {
  DETACH_FROM_SEQUENCE(gpu_sequence_checker_);

  auto params = std::make_unique<SetUpOnGpuParams>(
      gpu_preferences, gpu_workarounds, gpu_feature_info, context_state,
      shared_image_manager, is_for_display_compositor);
  if (owner_thread_ == OwnerThread::kCompositor) {
    task_sequence_->ScheduleTask(
        base::BindOnce(&SharedImageInterfaceInProcess::SetUpOnGpu,
                       base::Unretained(this), std::move(params)),

        {});
  } else {
    CHECK_EQ(owner_thread_, OwnerThread::kGpu);
    SetUpOnGpu(std::move(params));
  }
}

SharedImageInterfaceInProcess::~SharedImageInterfaceInProcess() {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  if (owner_thread_ == OwnerThread::kCompositor) {
    task_sequence_->ScheduleTask(
        base::BindOnce(&SharedImageInterfaceInProcess::DestroyOnGpu,
                       base::Unretained(this), &completion),
        {});
  } else {
    CHECK_EQ(owner_thread_, OwnerThread::kGpu);
    DestroyOnGpu(&completion);
  }

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
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  auto mailbox = Mailbox::Generate();
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // SharedImageInterfaceInProcess synchronizes with the GPU thread at
    // destruction time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(
        base::BindOnce(
            &SharedImageInterfaceInProcess::CreateSharedImageOnGpuThread,
            base::Unretained(this), mailbox, si_info, surface_handle,
            MakeSyncToken(next_fence_sync_release_++)),
        {});
  }
  return base::MakeRefCounted<ClientSharedImage>(mailbox, si_info.meta,
                                                 GenUnverifiedSyncToken(),
                                                 holder_, gfx::EMPTY_BUFFER);
}

void SharedImageInterfaceInProcess::CreateSharedImageOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
    gpu::SurfaceHandle surface_handle,
    const SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory())
    return;

  if (!MakeContextCurrent())
    return;

  DCHECK(shared_image_factory_);
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, surface_handle, si_info.meta.usage,
          std::string(si_info.debug_label))) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  auto mailbox = Mailbox::Generate();
  std::vector<uint8_t> pixel_data_copy(pixel_data.begin(), pixel_data.end());
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(base::BindOnce(&SharedImageInterfaceInProcess::
                                       CreateSharedImageWithDataOnGpuThread,
                                   base::Unretained(this), mailbox, si_info,
                                   MakeSyncToken(next_fence_sync_release_++),
                                   std::move(pixel_data_copy)),
                    {});
  }
  return base::MakeRefCounted<ClientSharedImage>(mailbox, si_info.meta,
                                                 GenUnverifiedSyncToken(),
                                                 holder_, gfx::EMPTY_BUFFER);
}

void SharedImageInterfaceInProcess::CreateSharedImageWithDataOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
    const SyncToken& sync_token,
    std::vector<uint8_t> pixel_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!LazyCreateSharedImageFactory())
    return;

  if (!MakeContextCurrent())
    return;

  DCHECK(shared_image_factory_);
  if (!shared_image_factory_->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, si_info.meta.usage,
          std::move(si_info.debug_label), pixel_data)) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  auto mailbox = Mailbox::Generate();
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(
        base::BindOnce(&SharedImageInterfaceInProcess::
                           CreateSharedImageWithBufferUsageOnGpuThread,
                       base::Unretained(this), mailbox, si_info, surface_handle,
                       buffer_usage, MakeSyncToken(next_fence_sync_release_++)),
        {});
  }

  auto handle_info = GetGpuMemoryBufferHandleInfo(mailbox);
  SharedImageInfo si_info_copy = si_info;

  // Clear the external sampler prefs for shared memory case if it is set.
  // https://issues.chromium.org/339546249.
  if (si_info_copy.meta.format.PrefersExternalSampler() &&
      (handle_info.handle.type ==
       gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER)) {
    si_info_copy.meta.format.ClearPrefersExternalSampler();
  }
  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info_copy.meta, GenUnverifiedSyncToken(),
      std::move(handle_info), holder_);
}

void SharedImageInterfaceInProcess::CreateSharedImageWithBufferUsageOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
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
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, surface_handle, si_info.meta.usage,
          std::move(si_info.debug_label), buffer_usage)) {
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
  absl::Cleanup completion_runner = [completion] { completion->Signal(); };

  DCHECK(shared_image_factory_);
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
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  CHECK(!si_info.meta.format.PrefersExternalSampler());
#endif

  auto client_buffer_handle = buffer_handle.Clone();
  auto mailbox = Mailbox::Generate();
  {
    base::AutoLock lock(lock_);
    SyncToken sync_token = MakeSyncToken(next_fence_sync_release_++);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(base::BindOnce(&SharedImageInterfaceInProcess::
                                       CreateSharedImageWithBufferOnGpuThread,
                                   base::Unretained(this), mailbox, si_info,
                                   std::move(buffer_handle), sync_token),
                    {});
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, GenUnverifiedSyncToken(),
      GpuMemoryBufferHandleInfo(std::move(client_buffer_handle),
                                si_info.meta.format, si_info.meta.size,
                                buffer_usage),
      holder_);
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::CreateSharedImage(
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  CHECK(!si_info.meta.format.PrefersExternalSampler());
#endif

  auto mailbox = Mailbox::Generate();
  auto gmb_type = buffer_handle.type;
  {
    base::AutoLock lock(lock_);
    SyncToken sync_token = MakeSyncToken(next_fence_sync_release_++);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(base::BindOnce(&SharedImageInterfaceInProcess::
                                       CreateSharedImageWithBufferOnGpuThread,
                                   base::Unretained(this), mailbox, si_info,
                                   std::move(buffer_handle), sync_token),
                    {});
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, GenUnverifiedSyncToken(), holder_, gmb_type);
}

SharedImageInterface::SharedImageMapping
SharedImageInterfaceInProcess::CreateSharedImage(
    const SharedImageInfo& si_info) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  DCHECK_EQ(si_info.meta.usage,
            gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_CPU_WRITE));

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  CHECK(!si_info.meta.format.PrefersExternalSampler());
#endif

  SharedImageInterface::SharedImageMapping shared_image_mapping;
  gfx::BufferFormat buffer_format =
      viz::SinglePlaneSharedImageFormatToBufferFormat(si_info.meta.format);
  const size_t buffer_size =
      gfx::BufferSizeForBufferFormat(si_info.meta.size, buffer_format);
  auto shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  if (!shared_memory_region.IsValid()) {
    DLOG(ERROR) << "base::UnsafeSharedMemoryRegion::Create() for SharedImage "
                   "with SHARED_IMAGE_USAGE_CPU_WRITE fails!";
    base::TerminateBecauseOutOfMemory(buffer_size);
  }

  shared_image_mapping.mapping = shared_memory_region.Map();
  if (!shared_image_mapping.mapping.IsValid()) {
    DLOG(ERROR)
        << "shared_memory_region.Map() for SHARED_IMAGE_USAGE_CPU_WRITE fails!";
    base::TerminateBecauseOutOfMemory(buffer_size);
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.offset = 0;
  handle.stride = static_cast<int32_t>(
      gfx::RowSizeForBufferFormat(si_info.meta.size.width(), buffer_format, 0));
  handle.region = std::move(shared_memory_region);

  auto mailbox = Mailbox::Generate();
  {
    base::AutoLock lock(lock_);
    SyncToken sync_token = MakeSyncToken(next_fence_sync_release_++);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service. Unretained is safe because
    // InProcessCommandBuffer synchronizes with the GPU thread at destruction
    // time, cancelling tasks, before |this| is destroyed.
    ScheduleGpuTask(base::BindOnce(&SharedImageInterfaceInProcess::
                                       CreateSharedImageWithBufferOnGpuThread,
                                   base::Unretained(this), mailbox, si_info,
                                   std::move(handle), sync_token),
                    {});
  }
  shared_image_mapping.shared_image = base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, GenUnverifiedSyncToken(), holder_,
      gfx::SHARED_MEMORY_BUFFER);

  return shared_image_mapping;
}

void SharedImageInterfaceInProcess::CreateSharedImageWithBufferOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
    gfx::GpuMemoryBufferHandle buffer_handle,
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
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, si_info.meta.usage,
          std::move(si_info.debug_label), std::move(buffer_handle))) {
    context_state_->MarkContextLost();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(sync_token.release_count());
}

SharedImageInterface::SwapChainSharedImages
SharedImageInterfaceInProcess::CreateSwapChain(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage) {
  NOTREACHED_IN_MIGRATION();
  return SharedImageInterface::SwapChainSharedImages(nullptr, nullptr);
}

void SharedImageInterfaceInProcess::PresentSwapChain(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  NOTREACHED_IN_MIGRATION();
}

#if BUILDFLAG(IS_FUCHSIA)
void SharedImageInterfaceInProcess::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    const viz::SharedImageFormat& format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  NOTREACHED_IN_MIGRATION();
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
  client_shared_image->UpdateDestructionSyncToken(sync_token);
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
  VerifySyncToken(sync_token);
  return sync_token;
}

void SharedImageInterfaceInProcess::VerifySyncToken(SyncToken& sync_token) {
  sync_token.SetVerifyFlush();
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

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcess::ImportSharedImage(
    const ExportedSharedImage& exported_shared_image) {
  // Secondary references are required only by client processes, so it shouldn't
  // be reachable here.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace gpu

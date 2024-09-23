// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"

#include "base/process/memory.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ui/gfx/buffer_format_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#endif

namespace gpu {

namespace {

// Generates unique IDs for GpuChannelSharedImageInterface.
base::AtomicSequenceNumber g_next_id;

}  // namespace

GpuChannelSharedImageInterface::GpuChannelSharedImageInterface(
    base::WeakPtr<SharedImageStub> shared_image_stub)
    : shared_image_stub_(shared_image_stub),
      command_buffer_id_(CommandBufferIdFromChannelAndRoute(
          shared_image_stub->channel()->client_id(),
          g_next_id.GetNext() + 1)),
      scheduler_(shared_image_stub->channel()->scheduler()),
      sequence_(scheduler_->CreateSequence(
          SchedulingPriority::kLow,
          shared_image_stub->channel()->task_runner(),
          CommandBufferNamespace::GPU_CHANNEL_SHARED_IMAGE_INTERFACE,
          command_buffer_id_)),
      shared_image_capabilities_(
          shared_image_stub->factory()->MakeCapabilities()) {
  DETACH_FROM_SEQUENCE(gpu_sequence_checker_);
}

GpuChannelSharedImageInterface::~GpuChannelSharedImageInterface() {
  scheduler_->DestroySequence(sequence_);
}

const SharedImageCapabilities&
GpuChannelSharedImageInterface::GetCapabilities() {
  return shared_image_capabilities_;
}

// Public functions specific to GpuChannelSharedImageInterface:
#if BUILDFLAG(IS_ANDROID)
scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::CreateSharedImageForAndroidVideo(
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    scoped_refptr<StreamTextureSharedImageInterface> image,
    scoped_refptr<RefCountedLock> drdc_lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  if (!shared_image_stub_) {
    return nullptr;
  }

  auto mailbox = Mailbox::Generate();

  scoped_refptr<SharedContextState> shared_context =
      shared_image_stub_->shared_context_state();

  if (shared_context->context_lost()) {
    return nullptr;
  }

  auto shared_image_backing = AndroidVideoImageBacking::Create(
      mailbox, size, color_space, kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      /*debug_label=*/"SIForAndroidVideo", std::move(image),
      std::move(shared_context), std::move(drdc_lock));
  SharedImageMetadata metadata{shared_image_backing->format(),
                               shared_image_backing->size(),
                               shared_image_backing->color_space(),
                               shared_image_backing->surface_origin(),
                               shared_image_backing->alpha_type(),
                               shared_image_backing->usage()};

  // Register it with shared image mailbox. This keeps |shared_image_backing|
  // around until its destruction cb is called.
  DCHECK(shared_image_stub_->channel()
             ->gpu_channel_manager()
             ->shared_image_manager());
  shared_image_stub_->factory()->RegisterBacking(
      std::move(shared_image_backing));

  return base::WrapRefCounted<ClientSharedImage>(
      new ClientSharedImage(mailbox, metadata, GenVerifiedSyncToken(), holder_,
                            GL_TEXTURE_EXTERNAL_OES));
}
#endif

#if BUILDFLAG(IS_WIN)
scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::CreateSharedImageForD3D11Video(
    const SharedImageInfo& si_info,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
    scoped_refptr<gpu::DXGISharedHandleState> dxgi_shared_handle_state,
    size_t array_slice,
    const bool is_thread_safe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  if (!shared_image_stub_) {
    return nullptr;
  }

  auto mailbox = Mailbox::Generate();
  auto metadata = si_info.meta;
  auto caps = shared_image_stub_->shared_context_state()->GetGLFormatCaps();
  // The target must be GL_TEXTURE_EXTERNAL_OES as the texture is not created
  // with D3D11_BIND_RENDER_TARGET bind flag and so it cannot be bound to the
  // framebuffer. To prevent Skia trying to bind it for read pixels, we need
  // it to be GL_TEXTURE_EXTERNAL_OES.
  std::unique_ptr<gpu::SharedImageBacking> backing =
      gpu::D3DImageBacking::Create(
          mailbox, metadata.format, metadata.size, metadata.color_space,
          metadata.surface_origin, metadata.alpha_type, metadata.usage,
          si_info.debug_label, texture, /*dcomp_texture=*/nullptr,
          std::move(dxgi_shared_handle_state), caps, GL_TEXTURE_EXTERNAL_OES,
          array_slice, /*use_update_subresource1=*/false, is_thread_safe);

  if (!backing) {
    return nullptr;
  }

  // Need to clear the backing since the D3D11 Video Decoder will initialize
  // the textures.
  backing->SetCleared();

  DCHECK(shared_image_stub_->channel()
             ->gpu_channel_manager()
             ->shared_image_manager());
  shared_image_stub_->factory()->RegisterBacking(std::move(backing));

  return base::WrapRefCounted<ClientSharedImage>(
      new ClientSharedImage(mailbox, metadata, GenVerifiedSyncToken(), holder_,
                            GL_TEXTURE_EXTERNAL_OES));
}
#endif

bool GpuChannelSharedImageInterface::MakeContextCurrent(bool needs_gl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!shared_image_stub_) {
    return false;
  }

  return shared_image_stub_->MakeContextCurrent(needs_gl);
}

scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  auto mailbox = Mailbox::Generate();
  {
    base::AutoLock lock(lock_);
    ScheduleGpuTask(
        base::BindOnce(
            &GpuChannelSharedImageInterface::CreateSharedImageOnGpuThread, this,
            mailbox, si_info, surface_handle),
        /*sync_token_fences=*/{}, MakeSyncToken(next_fence_sync_release_++));
  }
  return base::MakeRefCounted<ClientSharedImage>(mailbox, si_info.meta,
                                                 GenVerifiedSyncToken(),
                                                 holder_, gfx::EMPTY_BUFFER);
}

void GpuChannelSharedImageInterface::CreateSharedImageOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeContextCurrent()) {
    return;
  }

  DCHECK(shared_image_stub_->factory());
  if (!shared_image_stub_->factory()->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, surface_handle, si_info.meta.usage,
          std::string(si_info.debug_label))) {
    shared_image_stub_->shared_context_state()->MarkContextLost();
    return;
  }
}

scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  auto mailbox = Mailbox::Generate();
  std::vector<uint8_t> pixel_data_copy(pixel_data.begin(), pixel_data.end());
  {
    base::AutoLock lock(lock_);
    ScheduleGpuTask(
        base::BindOnce(&GpuChannelSharedImageInterface::
                           CreateSharedImageWithDataOnGpuThread,
                       this, mailbox, si_info, std::move(pixel_data_copy)),
        /*sync_token_fences=*/{}, MakeSyncToken(next_fence_sync_release_++));
  }
  return base::MakeRefCounted<ClientSharedImage>(mailbox, si_info.meta,
                                                 GenVerifiedSyncToken(),
                                                 holder_, gfx::EMPTY_BUFFER);
}

void GpuChannelSharedImageInterface::CreateSharedImageWithDataOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
    std::vector<uint8_t> pixel_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeContextCurrent()) {
    return;
  }

  DCHECK(shared_image_stub_->factory());
  if (!shared_image_stub_->factory()->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, si_info.meta.usage,
          std::move(si_info.debug_label), pixel_data)) {
    shared_image_stub_->shared_context_state()->MarkContextLost();
    return;
  }
}

scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  auto mailbox = Mailbox::Generate();
  {
    base::AutoLock lock(lock_);
    ScheduleGpuTask(
        base::BindOnce(&GpuChannelSharedImageInterface::
                           CreateSharedImageWithBufferUsageOnGpuThread,
                       this, mailbox, si_info, surface_handle, buffer_usage),
        /*sync_token_fences=*/{}, MakeSyncToken(next_fence_sync_release_++));
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, GenVerifiedSyncToken(),
      GetGpuMemoryBufferHandleInfo(mailbox), holder_);
}

void GpuChannelSharedImageInterface::
    CreateSharedImageWithBufferUsageOnGpuThread(const Mailbox& mailbox,
                                                SharedImageInfo si_info,
                                                SurfaceHandle surface_handle,
                                                gfx::BufferUsage buffer_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeContextCurrent()) {
    return;
  }

  DCHECK(shared_image_stub_->factory());
  if (!shared_image_stub_->factory()->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, surface_handle, si_info.meta.usage,
          std::move(si_info.debug_label), buffer_usage)) {
    shared_image_stub_->shared_context_state()->MarkContextLost();
    return;
  }
}

GpuMemoryBufferHandleInfo
GpuChannelSharedImageInterface::GetGpuMemoryBufferHandleInfo(
    const Mailbox& mailbox) {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  gfx::GpuMemoryBufferHandle handle;
  viz::SharedImageFormat format;
  gfx::Size size;
  gfx::BufferUsage buffer_usage;

  ScheduleGpuTask(base::BindOnce(&GpuChannelSharedImageInterface::
                                     GetGpuMemoryBufferHandleInfoOnGpuThread,
                                 this, mailbox, &handle, &format, &size,
                                 &buffer_usage, &completion),
                  /*sync_token_fences=*/{}, SyncToken());
  completion.Wait();
  return GpuMemoryBufferHandleInfo(std::move(handle), format, size,
                                   buffer_usage);
}

void GpuChannelSharedImageInterface::GetGpuMemoryBufferHandleInfoOnGpuThread(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle* handle,
    viz::SharedImageFormat* format,
    gfx::Size* size,
    gfx::BufferUsage* buffer_usage,
    base::WaitableEvent* completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  base::ScopedClosureRunner completion_runner(base::BindOnce(
      [](base::WaitableEvent* completion) { completion->Signal(); },
      completion));

  DCHECK(shared_image_stub_->factory());

  if (!shared_image_stub_->factory()->GetGpuMemoryBufferHandleInfo(
          mailbox, *handle, *format, *size, *buffer_usage)) {
    LOG(ERROR) << "GpuChannelSharedImageInterface: Unable to get "
                  "GpuMemoryBufferHandle";
  }
}

scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::CreateSharedImage(
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
    ScheduleGpuTask(
        base::BindOnce(&GpuChannelSharedImageInterface::
                           CreateSharedImageWithBufferOnGpuThread,
                       this, mailbox, si_info, std::move(buffer_handle)),
        /*sync_token_fences=*/{}, MakeSyncToken(next_fence_sync_release_++));
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, GenVerifiedSyncToken(),
      GpuMemoryBufferHandleInfo(std::move(client_buffer_handle),
                                si_info.meta.format, si_info.meta.size,
                                buffer_usage),
      holder_);
}

scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::CreateSharedImage(
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
    ScheduleGpuTask(
        base::BindOnce(&GpuChannelSharedImageInterface::
                           CreateSharedImageWithBufferOnGpuThread,
                       this, mailbox, si_info, std::move(buffer_handle)),
        /*sync_token_fences=*/{}, MakeSyncToken(next_fence_sync_release_++));
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, GenVerifiedSyncToken(), holder_, gmb_type);
}
SharedImageInterface::SharedImageMapping
GpuChannelSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  DCHECK(si_info.meta.usage ==
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
    ScheduleGpuTask(base::BindOnce(&GpuChannelSharedImageInterface::
                                       CreateSharedImageWithBufferOnGpuThread,
                                   this, mailbox, si_info, std::move(handle)),
                    /*sync_token_fences=*/{},
                    MakeSyncToken(next_fence_sync_release_++));
  }
  shared_image_mapping.shared_image = base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, GenVerifiedSyncToken(), holder_,
      gfx::SHARED_MEMORY_BUFFER);

  return shared_image_mapping;
}

void GpuChannelSharedImageInterface::CreateSharedImageWithBufferOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeContextCurrent()) {
    return;
  }

  DCHECK(shared_image_stub_->factory());
  if (!shared_image_stub_->factory()->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, si_info.meta.usage,
          std::move(si_info.debug_label), std::move(buffer_handle))) {
    shared_image_stub_->shared_context_state()->MarkContextLost();
    return;
  }
}

SharedImageInterface::SwapChainSharedImages
GpuChannelSharedImageInterface::CreateSwapChain(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage) {
  NOTREACHED_IN_MIGRATION();
  return GpuChannelSharedImageInterface::SwapChainSharedImages(nullptr,
                                                               nullptr);
}

void GpuChannelSharedImageInterface::PresentSwapChain(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  NOTREACHED_IN_MIGRATION();
}

#if BUILDFLAG(IS_FUCHSIA)
void GpuChannelSharedImageInterface::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    const viz::SharedImageFormat& format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  NOTREACHED_IN_MIGRATION();
}
#endif  // BUILDFLAG(IS_FUCHSIA)

void GpuChannelSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  UpdateSharedImage(sync_token, nullptr, mailbox);
}

void GpuChannelSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  DCHECK(!acquire_fence);
  base::AutoLock lock(lock_);
  ScheduleGpuTask(
      base::BindOnce(
          &GpuChannelSharedImageInterface::UpdateSharedImageOnGpuThread, this,
          mailbox),
      {sync_token}, MakeSyncToken(next_fence_sync_release_++));
}

void GpuChannelSharedImageInterface::UpdateSharedImageOnGpuThread(
    const Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeContextCurrent()) {
    return;
  }

  if (!shared_image_stub_->factory() ||
      !shared_image_stub_->factory()->UpdateSharedImage(mailbox)) {
    shared_image_stub_->shared_context_state()->MarkContextLost();
    return;
  }
}

void GpuChannelSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  // Use sync token dependency to ensure that the destroy task does not run
  // before sync token is released.
  ScheduleGpuTask(
      base::BindOnce(
          &GpuChannelSharedImageInterface::DestroySharedImageOnGpuThread, this,
          mailbox),
      {sync_token}, SyncToken());
}

void GpuChannelSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    scoped_refptr<ClientSharedImage> client_shared_image) {
  CHECK(client_shared_image->HasOneRef());
  client_shared_image->UpdateDestructionSyncToken(sync_token);
}

void GpuChannelSharedImageInterface::DestroySharedImageOnGpuThread(
    const Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!MakeContextCurrent()) {
    return;
  }

  if (!shared_image_stub_->factory() ||
      !shared_image_stub_->factory()->DestroySharedImage(mailbox)) {
    shared_image_stub_->shared_context_state()->MarkContextLost();
  }
}

SyncToken GpuChannelSharedImageInterface::GenUnverifiedSyncToken() {
  base::AutoLock lock(lock_);
  return MakeSyncToken(next_fence_sync_release_ - 1);
}

SyncToken GpuChannelSharedImageInterface::GenVerifiedSyncToken() {
  base::AutoLock lock(lock_);
  SyncToken sync_token = MakeSyncToken(next_fence_sync_release_ - 1);
  VerifySyncToken(sync_token);
  return sync_token;
}

void GpuChannelSharedImageInterface::VerifySyncToken(SyncToken& sync_token) {
  sync_token.SetVerifyFlush();
}

void GpuChannelSharedImageInterface::WaitSyncToken(
    const SyncToken& sync_token) {
  base::AutoLock lock(lock_);

  ScheduleGpuTask(base::DoNothing(), {sync_token},
                  MakeSyncToken(next_fence_sync_release_++));
}

void GpuChannelSharedImageInterface::Flush() {
  // No need to flush in this implementation.
}

scoped_refptr<gfx::NativePixmap>
GpuChannelSharedImageInterface::GetNativePixmap(const gpu::Mailbox& mailbox) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void GpuChannelSharedImageInterface::ScheduleGpuTask(
    base::OnceClosure task,
    std::vector<SyncToken> sync_token_fences,
    const SyncToken& release) {
  scheduler_->ScheduleTask(gpu::Scheduler::Task(
      sequence_, std::move(task), std::move(sync_token_fences), release));
}

scoped_refptr<ClientSharedImage>
GpuChannelSharedImageInterface::ImportSharedImage(
    const ExportedSharedImage& exported_shared_image) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace gpu

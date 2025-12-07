// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_interface_in_process_base.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "build/buildflag.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_pool_id.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {

SharedImageInterfaceInProcessBase::SharedImageInterfaceInProcessBase(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id,
    bool verify_creation_sync_token,
    SharedImageCapabilities shared_image_capabilities)
    : namespace_id_(namespace_id),
      command_buffer_id_(command_buffer_id),
      verify_creation_sync_token_(verify_creation_sync_token),
      shared_image_capabilities_(std::move(shared_image_capabilities)),
      shared_image_capabilities_ready_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::SIGNALED) {
  DETACH_FROM_SEQUENCE(gpu_sequence_checker_);
}

SharedImageInterfaceInProcessBase::SharedImageInterfaceInProcessBase(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id,
    bool verify_creation_sync_token)
    : namespace_id_(namespace_id),
      command_buffer_id_(command_buffer_id),
      verify_creation_sync_token_(verify_creation_sync_token),
      shared_image_capabilities_ready_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DETACH_FROM_SEQUENCE(gpu_sequence_checker_);
}

SharedImageInterfaceInProcessBase::~SharedImageInterfaceInProcessBase() =
    default;

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcessBase::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    std::optional<SharedImagePoolId> pool_id) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  auto mailbox = Mailbox::Generate();
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service.
    ScheduleGpuTask(
        base::BindOnce(
            &SharedImageInterfaceInProcessBase::CreateSharedImageOnGpuThread,
            this, mailbox, si_info, surface_handle),
        /*sync_token_fences=*/{}, GenNextSyncTokenLocked());
  }
  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info, GenCreationSyncToken(), holder_, gfx::EMPTY_BUFFER);
}

void SharedImageInterfaceInProcessBase::CreateSharedImageOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  SharedImageFactory* shared_image_factory = GetSharedImageFactoryOnGpuThread();
  if (!shared_image_factory) {
    return;
  }

  if (!MakeContextCurrentOnGpuThread()) {
    return;
  }

  if (!shared_image_factory->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, surface_handle, si_info.meta.usage,
          std::string(si_info.debug_label))) {
    MarkContextLostOnGpuThread();
  }
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcessBase::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  auto mailbox = Mailbox::Generate();
  std::vector<uint8_t> pixel_data_copy(pixel_data.begin(), pixel_data.end());
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service.
    ScheduleGpuTask(
        base::BindOnce(&SharedImageInterfaceInProcessBase::
                           CreateSharedImageWithDataOnGpuThread,
                       this, mailbox, si_info, std::move(pixel_data_copy)),
        /*sync_token_fences=*/{}, GenNextSyncTokenLocked());
  }
  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info, GenCreationSyncToken(), holder_, gfx::EMPTY_BUFFER);
}

void SharedImageInterfaceInProcessBase::CreateSharedImageWithDataOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
    std::vector<uint8_t> pixel_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  SharedImageFactory* shared_image_factory = GetSharedImageFactoryOnGpuThread();
  if (!shared_image_factory) {
    return;
  }

  if (!MakeContextCurrentOnGpuThread()) {
    return;
  }

  if (!shared_image_factory->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, si_info.meta.usage,
          std::move(si_info.debug_label), pixel_data)) {
    MarkContextLostOnGpuThread();
  }
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcessBase::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    std::optional<SharedImagePoolId> pool_id) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage));
  auto mailbox = Mailbox::Generate();
  // Copy which can be modified.
  SharedImageInfo si_info_copy = si_info;
  // Set CPU read/write usage based on buffer usage.
  si_info_copy.meta.usage |= GetCpuSIUsage(buffer_usage);
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service.
    ScheduleGpuTask(
        base::BindOnce(&SharedImageInterfaceInProcessBase::
                           CreateSharedImageWithBufferUsageOnGpuThread,
                       this, mailbox, si_info_copy, surface_handle,
                       buffer_usage),
        /*sync_token_fences=*/{}, GenNextSyncTokenLocked());
  }

  auto handle_info = GetGpuMemoryBufferHandleInfo(mailbox);
  // Clear the external sampler prefs for shared memory case if it is set.
  // https://issues.chromium.org/339546249.
  if (si_info_copy.meta.format.PrefersExternalSampler() &&
      (handle_info.handle.type ==
       gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER)) {
    si_info_copy.meta.format.ClearPrefersExternalSampler();
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info_copy, GenCreationSyncToken(), std::move(handle_info),
      holder_);
}

void SharedImageInterfaceInProcessBase::
    CreateSharedImageWithBufferUsageOnGpuThread(const Mailbox& mailbox,
                                                SharedImageInfo si_info,
                                                SurfaceHandle surface_handle,
                                                gfx::BufferUsage buffer_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  SharedImageFactory* shared_image_factory = GetSharedImageFactoryOnGpuThread();
  if (!shared_image_factory) {
    return;
  }

  if (!MakeContextCurrentOnGpuThread()) {
    return;
  }

  // Note that SharedImageInterfaceInProcess implementation here uses
  // SharedImageFactory::CreateSharedImage() to create a shared image backed by
  // native buffer/shared memory in GPU process. This is different
  // implementation and code path compared to ClientSharedImage implementation
  // which creates native buffer/shared memory on IO thread and then creates a
  // mailbox from it on GPU thread.
  if (!shared_image_factory->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, surface_handle, si_info.meta.usage,
          std::move(si_info.debug_label), buffer_usage)) {
    MarkContextLostOnGpuThread();
  }
}

GpuMemoryBufferHandleInfo
SharedImageInterfaceInProcessBase::GetGpuMemoryBufferHandleInfo(
    const Mailbox& mailbox) {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  gfx::GpuMemoryBufferHandle handle;
  gfx::BufferUsage buffer_usage;

  ScheduleGpuTask(
      base::BindOnce(&SharedImageInterfaceInProcessBase::
                         GetGpuMemoryBufferHandleInfoOnGpuThread,
                     this, mailbox, &handle, &buffer_usage, &completion),
      /*sync_token_fences=*/{}, SyncToken());
  completion.Wait();
  return GpuMemoryBufferHandleInfo(std::move(handle), buffer_usage);
}

void SharedImageInterfaceInProcessBase::GetGpuMemoryBufferHandleInfoOnGpuThread(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle* handle,
    gfx::BufferUsage* buffer_usage,
    base::WaitableEvent* completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  absl::Cleanup completion_runner = [completion] { completion->Signal(); };

  SharedImageFactory* shared_image_factory = GetSharedImageFactoryOnGpuThread();
  DCHECK(shared_image_factory);

  // Note that we are not calling `MakeContextCurrent()` here as of now since
  // it is not needed to get the handle from the backings. Make context current
  // if we find that it is required.
  if (!shared_image_factory->GetGpuMemoryBufferHandleInfo(mailbox, *handle,
                                                          *buffer_usage)) {
    LOG(ERROR) << "SharedImageInterfaceInProcessBase: Unable to get "
                  "GpuMemoryBufferHandle";
  }
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcessBase::CreateSharedImage(
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
  // Copy which can be modified.
  SharedImageInfo si_info_copy = si_info;
  // Set CPU read/write usage based on buffer usage.
  si_info_copy.meta.usage |= GetCpuSIUsage(buffer_usage);
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service.
    ScheduleGpuTask(
        base::BindOnce(&SharedImageInterfaceInProcessBase::
                           CreateSharedImageWithBufferOnGpuThread,
                       this, mailbox, si_info_copy, std::move(buffer_handle)),
        /*sync_token_fences=*/{}, GenNextSyncTokenLocked());
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info_copy, GenCreationSyncToken(),
      GpuMemoryBufferHandleInfo(std::move(client_buffer_handle), buffer_usage),
      holder_);
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcessBase::CreateSharedImage(
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
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service.
    ScheduleGpuTask(
        base::BindOnce(&SharedImageInterfaceInProcessBase::
                           CreateSharedImageWithBufferOnGpuThread,
                       this, mailbox, si_info, std::move(buffer_handle)),
        /*sync_token_fences=*/{}, GenNextSyncTokenLocked());
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info, GenCreationSyncToken(), holder_, gmb_type);
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcessBase::CreateSharedImageForMLTensor(
    std::string debug_label,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    gpu::SharedImageUsageSet usage) {
  NOTREACHED();
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcessBase::CreateSharedImageForSoftwareCompositor(
    const SharedImageInfo& si_info) {
  base::WritableSharedMemoryMapping mapping;
  gfx::GpuMemoryBufferHandle handle;
  CreateSharedMemoryRegionFromSIInfo(si_info, mapping, handle);

  auto mailbox = Mailbox::Generate();
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the task under the lock to guarantee monotonicity of
    // the release ids as seen by the service.
    ScheduleGpuTask(base::BindOnce(&SharedImageInterfaceInProcessBase::
                                       CreateSharedImageWithBufferOnGpuThread,
                                   this, mailbox, si_info, std::move(handle)),
                    /*sync_token_fences=*/{}, GenNextSyncTokenLocked());
  }
  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info, GenCreationSyncToken(), holder_, std::move(mapping));
}

void SharedImageInterfaceInProcessBase::CreateSharedImageWithBufferOnGpuThread(
    const Mailbox& mailbox,
    SharedImageInfo si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  SharedImageFactory* shared_image_factory = GetSharedImageFactoryOnGpuThread();
  if (!shared_image_factory) {
    return;
  }

  if (!MakeContextCurrentOnGpuThread()) {
    return;
  }

  if (!shared_image_factory->CreateSharedImage(
          mailbox, si_info.meta.format, si_info.meta.size,
          si_info.meta.color_space, si_info.meta.surface_origin,
          si_info.meta.alpha_type, si_info.meta.usage,
          std::move(si_info.debug_label), std::move(buffer_handle))) {
    MarkContextLostOnGpuThread();
  }
}

#if BUILDFLAG(IS_FUCHSIA)
void SharedImageInterfaceInProcessBase::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    const viz::SharedImageFormat& format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_FUCHSIA)

void SharedImageInterfaceInProcessBase::UpdateSharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  UpdateSharedImage(sync_token, nullptr, mailbox);
}

void SharedImageInterfaceInProcessBase::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  DCHECK(!acquire_fence);
  base::AutoLock lock(lock_);
  // Note: we enqueue the task under the lock to guarantee monotonicity of the
  // release ids as seen by the service.
  ScheduleGpuTask(
      base::BindOnce(
          &SharedImageInterfaceInProcessBase::UpdateSharedImageOnGpuThread,
          this, mailbox),
      /*sync_token_fences=*/{sync_token}, GenNextSyncTokenLocked());
}

void SharedImageInterfaceInProcessBase::UpdateSharedImageOnGpuThread(
    const Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  if (!MakeContextCurrentOnGpuThread()) {
    return;
  }

  SharedImageFactory* shared_image_factory = GetSharedImageFactoryOnGpuThread();
  if (!shared_image_factory ||
      !shared_image_factory->UpdateSharedImage(mailbox)) {
    MarkContextLostOnGpuThread();
  }
}

void SharedImageInterfaceInProcessBase::DestroySharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  // Use sync token dependency to ensure that the destroy task does not run
  // before sync token is released.
  ScheduleGpuTask(
      base::BindOnce(
          &SharedImageInterfaceInProcessBase::DestroySharedImageOnGpuThread,
          this, mailbox),
      /*sync_token_fences=*/{sync_token}, SyncToken());
}

void SharedImageInterfaceInProcessBase::DestroySharedImage(
    const SyncToken& sync_token,
    scoped_refptr<ClientSharedImage> client_shared_image) {
  CHECK(client_shared_image->HasOneRef());
  client_shared_image->UpdateDestructionSyncToken(sync_token);
}

void SharedImageInterfaceInProcessBase::DestroySharedImageOnGpuThread(
    const Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  if (!MakeContextCurrentOnGpuThread()) {
    return;
  }

  SharedImageFactory* shared_image_factory = GetSharedImageFactoryOnGpuThread();
  if (!shared_image_factory ||
      !shared_image_factory->DestroySharedImage(mailbox)) {
    MarkContextLostOnGpuThread();
  }
}

SyncToken SharedImageInterfaceInProcessBase::GenNextSyncToken() {
  base::AutoLock lock(lock_);
  return GenNextSyncTokenLocked();
}

SyncToken SharedImageInterfaceInProcessBase::GenNextSyncTokenLocked() {
  return MakeSyncToken(next_fence_sync_release_++);
}

SyncToken SharedImageInterfaceInProcessBase::GenUnverifiedSyncToken() {
  base::AutoLock lock(lock_);
  return MakeSyncToken(next_fence_sync_release_ - 1);
}

SyncToken SharedImageInterfaceInProcessBase::GenVerifiedSyncToken() {
  base::AutoLock lock(lock_);
  SyncToken sync_token = MakeSyncToken(next_fence_sync_release_ - 1);
  VerifySyncToken(sync_token);
  return sync_token;
}

void SharedImageInterfaceInProcessBase::VerifySyncToken(SyncToken& sync_token) {
  sync_token.SetVerifyFlush();
}

bool SharedImageInterfaceInProcessBase::CanVerifySyncToken(
    const gpu::SyncToken& sync_token) {
  return sync_token.namespace_id() == namespace_id_;
}

void SharedImageInterfaceInProcessBase::VerifyFlush() {
  // No flush required as we are only within a single process
}

void SharedImageInterfaceInProcessBase::WaitSyncToken(
    const SyncToken& sync_token) {
  base::AutoLock lock(lock_);

  ScheduleGpuTask(base::DoNothing(),
                  /*sync_token_fences=*/{sync_token}, GenNextSyncTokenLocked());
}

scoped_refptr<ClientSharedImage>
SharedImageInterfaceInProcessBase::ImportSharedImage(
    ExportedSharedImage exported_shared_image) {
  // Secondary references are required only by client processes, so it shouldn't
  // be reachable here.
  NOTREACHED();
}

const SharedImageCapabilities&
SharedImageInterfaceInProcessBase::GetCapabilities() {
  // Return fast on already-initialized common case.
  if (shared_image_capabilities_ready_.IsSignaled()) {
    return shared_image_capabilities_;
  }

  ScheduleGpuTask(
      base::BindOnce(
          &SharedImageInterfaceInProcessBase::GetCapabilitiesOnGpuThread, this),
      /*sync_token_fences=*/{}, SyncToken());

  shared_image_capabilities_ready_.Wait();
  return shared_image_capabilities_;
}

void SharedImageInterfaceInProcessBase::GetCapabilitiesOnGpuThread() {
  // `GetCapabilitiesOnGpuThread()` may be scheduled by multiple threads,
  // so quick return if it's already been run.
  if (shared_image_capabilities_ready_.IsSignaled()) {
    return;
  }

  SharedImageFactory* shared_image_factory = GetSharedImageFactoryOnGpuThread();
  if (shared_image_factory) {
    shared_image_capabilities_ = shared_image_factory->MakeCapabilities();
  }
  // Fallback to default-initialized version if no factory.

  shared_image_capabilities_ready_.Signal();
}

}  // namespace gpu

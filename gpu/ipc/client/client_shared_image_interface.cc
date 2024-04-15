// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/client_shared_image_interface.h"

#include "base/process/memory.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/client/shared_image_interface_proxy.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

namespace gpu {

ClientSharedImageInterface::ClientSharedImageInterface(
    SharedImageInterfaceProxy* proxy,
    scoped_refptr<gpu::GpuChannelHost> channel)
    : gpu_channel_(std::move(channel)), proxy_(proxy) {}

ClientSharedImageInterface::~ClientSharedImageInterface() {
  gpu::SyncToken sync_token;
  auto mailboxes_to_delete = mailboxes_;
  for (const auto& mailbox : mailboxes_to_delete)
    DestroySharedImage(sync_token, mailbox);
}

void ClientSharedImageInterface::UpdateSharedImage(const SyncToken& sync_token,
                                                   const Mailbox& mailbox) {
  proxy_->UpdateSharedImage(sync_token, mailbox);
}

void ClientSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  proxy_->UpdateSharedImage(sync_token, std::move(acquire_fence), mailbox);
}

void ClientSharedImageInterface::PresentSwapChain(const SyncToken& sync_token,
                                                  const Mailbox& mailbox) {
  proxy_->PresentSwapChain(sync_token, mailbox);
}

#if BUILDFLAG(IS_FUCHSIA)
void ClientSharedImageInterface::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  proxy_->RegisterSysmemBufferCollection(std::move(service_handle),
                                         std::move(sysmem_token), format, usage,
                                         register_with_image_pipe);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

SyncToken ClientSharedImageInterface::GenUnverifiedSyncToken() {
  return proxy_->GenUnverifiedSyncToken();
}

SyncToken ClientSharedImageInterface::GenVerifiedSyncToken() {
  return proxy_->GenVerifiedSyncToken();
}

void ClientSharedImageInterface::VerifySyncToken(gpu::SyncToken& sync_token) {
  proxy_->VerifySyncToken(sync_token);
}

void ClientSharedImageInterface::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  proxy_->WaitSyncToken(sync_token);
}

void ClientSharedImageInterface::Flush() {
  proxy_->Flush();
}

scoped_refptr<gfx::NativePixmap> ClientSharedImageInterface::GetNativePixmap(
    const gpu::Mailbox& mailbox) {
  return proxy_->GetNativePixmap(mailbox);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_EQ(surface_handle, kNullSurfaceHandle);
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage)) << si_info.meta.usage;
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(proxy_->CreateSharedImage(si_info)), si_info.meta,
      GenUnverifiedSyncToken(), holder_, gfx::EMPTY_BUFFER);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  // Pixel upload path only supports single-planar formats.
  DCHECK(si_info.meta.format.is_single_plane())
      << si_info.meta.format.ToString();
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage)) << si_info.meta.usage;

  // EstimatedSizeInBytes() returns the minimum size in bytes needed to store
  // `format` at `size` so if span is smaller there is a problem.
  CHECK_GE(pixel_data.size(),
           si_info.meta.format.EstimatedSizeInBytes(si_info.meta.size));

  auto mailbox = proxy_->CreateSharedImage(si_info, pixel_data);
  if (mailbox.IsZero()) {
    return nullptr;
  }

  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(mailbox), si_info.meta, GenUnverifiedSyncToken(), holder_,
      gfx::EMPTY_BUFFER);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  DCHECK_EQ(surface_handle, kNullSurfaceHandle);
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage)) << si_info.meta.usage;
  gfx::GpuMemoryBufferHandle buffer_handle;
  auto mailbox =
      proxy_->CreateSharedImage(si_info, buffer_usage, &buffer_handle);
  if (mailbox.IsZero()) {
    return nullptr;
  }

  CHECK(!buffer_handle.is_null());
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(mailbox), si_info.meta, GenUnverifiedSyncToken(),
      GpuMemoryBufferHandleInfo(std::move(buffer_handle), si_info.meta.format,
                                si_info.meta.size, buffer_usage),
      holder_);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage)) << si_info.meta.usage;
  DCHECK(viz::HasEquivalentBufferFormat(si_info.meta.format))
      << si_info.meta.format.ToString();
  CHECK(!si_info.meta.format.IsLegacyMultiplanar())
      << si_info.meta.format.ToString();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  CHECK(!si_info.meta.format.PrefersExternalSampler())
      << si_info.meta.format.ToString();
#endif
  auto client_buffer_handle = buffer_handle.Clone();
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(proxy_->CreateSharedImage(si_info, std::move(buffer_handle))),
      si_info.meta, GenUnverifiedSyncToken(),
      GpuMemoryBufferHandleInfo(std::move(client_buffer_handle),
                                si_info.meta.format, si_info.meta.size,
                                buffer_usage),
      holder_);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage)) << si_info.meta.usage;
  DCHECK(viz::HasEquivalentBufferFormat(si_info.meta.format))
      << si_info.meta.format.ToString();
  CHECK(!si_info.meta.format.IsLegacyMultiplanar())
      << si_info.meta.format.ToString();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  CHECK(!si_info.meta.format.PrefersExternalSampler())
      << si_info.meta.format.ToString();
#endif
  auto buffer_handle_type = buffer_handle.type;
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(proxy_->CreateSharedImage(si_info, std::move(buffer_handle))),
      si_info.meta, GenUnverifiedSyncToken(), holder_, buffer_handle_type);
}

SharedImageInterface::SharedImageMapping
ClientSharedImageInterface::CreateSharedImage(const SharedImageInfo& si_info) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage)) << si_info.meta.usage;
  DCHECK_EQ(si_info.meta.usage, gpu::SHARED_IMAGE_USAGE_CPU_WRITE);
  DCHECK(viz::HasEquivalentBufferFormat(si_info.meta.format))
      << si_info.meta.format.ToString();
  CHECK(!si_info.meta.format.IsLegacyMultiplanar())
      << si_info.meta.format.ToString();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  CHECK(!si_info.meta.format.PrefersExternalSampler())
      << si_info.meta.format.ToString();
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

  shared_image_mapping.shared_image = base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(proxy_->CreateSharedImage(si_info, std::move(handle))),
      si_info.meta, GenUnverifiedSyncToken(), holder_,
      gfx::SHARED_MEMORY_BUFFER);
  return shared_image_mapping;
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferPlane plane,
    const SharedImageInfo& si_info) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage)) << si_info.meta.usage;
  auto buffer_format = gpu_memory_buffer->GetFormat();
  CHECK(gpu::IsPlaneValidForGpuMemoryBufferFormat(plane, buffer_format));
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(proxy_->CreateSharedImage(
          buffer_format, plane, gpu_memory_buffer->GetSize(), si_info,
          gpu_memory_buffer->CloneHandle())),
      SharedImageMetadata(viz::GetSinglePlaneSharedImageFormat(
                              GetPlaneBufferFormat(plane, buffer_format)),
                          gpu_memory_buffer->GetSize(),
                          si_info.meta.color_space, si_info.meta.surface_origin,
                          si_info.meta.alpha_type, si_info.meta.usage),
      GenUnverifiedSyncToken(), holder_, gpu_memory_buffer->GetType());
}

#if BUILDFLAG(IS_WIN)
void ClientSharedImageInterface::CopyToGpuMemoryBuffer(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  proxy_->CopyToGpuMemoryBuffer(sync_token, mailbox);
}

void ClientSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    scoped_refptr<gfx::D3DSharedFence> d3d_shared_fence,
    const Mailbox& mailbox) {
  proxy_->UpdateSharedImage(sync_token, std::move(d3d_shared_fence), mailbox);
}
#endif

ClientSharedImageInterface::SwapChainSharedImages
ClientSharedImageInterface::CreateSwapChain(viz::SharedImageFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            GrSurfaceOrigin surface_origin,
                                            SkAlphaType alpha_type,
                                            uint32_t usage) {
  DCHECK(gpu::IsValidClientUsage(usage));
  auto mailboxes = proxy_->CreateSwapChain(format, size, color_space,
                                           surface_origin, alpha_type, usage);
  AddMailbox(mailboxes.front_buffer);
  AddMailbox(mailboxes.back_buffer);
  SyncToken sync_token = GenUnverifiedSyncToken();
  return ClientSharedImageInterface::SwapChainSharedImages(
      base::MakeRefCounted<ClientSharedImage>(
          mailboxes.front_buffer,
          SharedImageMetadata(format, size, color_space, surface_origin,
                              alpha_type, usage),
          sync_token, holder_, gfx::EMPTY_BUFFER),
      base::MakeRefCounted<ClientSharedImage>(
          mailboxes.back_buffer,
          SharedImageMetadata(format, size, color_space, surface_origin,
                              alpha_type, usage),
          sync_token, holder_, gfx::EMPTY_BUFFER));
}

void ClientSharedImageInterface::DestroySharedImage(const SyncToken& sync_token,
                                                    const Mailbox& mailbox) {
  DCHECK(!mailbox.IsZero());

  {
    base::AutoLock lock(lock_);
    auto it = mailboxes_.find(mailbox);
    CHECK(it != mailboxes_.end());
    mailboxes_.erase(it);
  }
  proxy_->DestroySharedImage(sync_token, mailbox);
}

void ClientSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    scoped_refptr<ClientSharedImage> client_shared_image) {
  CHECK(client_shared_image->HasOneRef());
  CHECK(client_shared_image->HasHolder());
  client_shared_image->UpdateDestructionSyncToken(sync_token);
  client_shared_image->MarkForDestruction();
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::ImportSharedImage(
    const ExportedSharedImage& exported_shared_image) {
  const auto& mailbox = exported_shared_image.mailbox_;
  const auto& metadata = exported_shared_image.metadata_;
  const auto& sync_token = exported_shared_image.creation_sync_token_;
  uint32_t texture_target = exported_shared_image.texture_target_;

  DCHECK(!mailbox.IsZero());
  AddMailbox(mailbox);
  proxy_->AddReferenceToSharedImage(sync_token, mailbox, metadata.usage);

  return base::WrapRefCounted<ClientSharedImage>(new ClientSharedImage(
      mailbox, metadata, sync_token, holder_, texture_target));
}

uint32_t ClientSharedImageInterface::UsageForMailbox(const Mailbox& mailbox) {
  return proxy_->UsageForMailbox(mailbox);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::NotifyMailboxAdded(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  AddMailbox(mailbox);
  proxy_->NotifyMailboxAdded(mailbox, usage);

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox,
      SharedImageMetadata(format, size, color_space, surface_origin, alpha_type,
                          usage),
      GenUnverifiedSyncToken(), holder_, gfx::EMPTY_BUFFER);
}

Mailbox ClientSharedImageInterface::AddMailbox(const gpu::Mailbox& mailbox) {
  if (mailbox.IsZero())
    return mailbox;

  base::AutoLock lock(lock_);
  mailboxes_.insert(mailbox);
  return mailbox;
}

const SharedImageCapabilities& ClientSharedImageInterface::GetCapabilities() {
  return proxy_->GetCapabilities();
}

}  // namespace gpu

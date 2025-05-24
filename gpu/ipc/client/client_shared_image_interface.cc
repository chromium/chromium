// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/client_shared_image_interface.h"

#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/client/shared_image_interface_proxy.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

namespace gpu {

ClientSharedImageInterface::ClientSharedImageInterface(
    SharedImageInterfaceProxy* proxy,
    scoped_refptr<gpu::GpuChannelHost> channel)
    : gpu_channel_(std::move(channel)),
      proxy_(proxy),
      shared_memory_pool_(
#if BUILDFLAG(IS_WIN)
          base::MakeRefCounted<base::UnsafeSharedMemoryPool>()
#else
          nullptr
#endif
      ) {
}

ClientSharedImageInterface::~ClientSharedImageInterface() {
  gpu::SyncToken sync_token;
  for (const auto& [mailbox, ref_count] : mailboxes_) {
    CHECK_GT(ref_count, 0);
    for (int i = 0; i < ref_count; i++) {
      proxy_->DestroySharedImage(sync_token, mailbox);
    }
  }
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
    const viz::SharedImageFormat& format,
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

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    std::optional<SharedImagePoolId> pool_id) {
  DCHECK_EQ(surface_handle, kNullSurfaceHandle);
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage))
      << uint32_t(si_info.meta.usage);
  auto mailbox = proxy_->CreateSharedImage(si_info, std::move(pool_id));
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(mailbox), si_info.meta, GenUnverifiedSyncToken(), holder_,
      gfx::EMPTY_BUFFER);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  // Pixel upload path only supports single-planar formats.
  DCHECK(si_info.meta.format.is_single_plane())
      << si_info.meta.format.ToString();
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage))
      << uint32_t(si_info.meta.usage);

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
    gfx::BufferUsage buffer_usage,
    std::optional<SharedImagePoolId> pool_id) {
  DCHECK_EQ(surface_handle, kNullSurfaceHandle);
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage))
      << uint32_t(si_info.meta.usage);
  gfx::GpuMemoryBufferHandle buffer_handle;

  // Copy which can be modified.
  SharedImageInfo si_info_copy = si_info;
  auto mailbox = proxy_->CreateSharedImage(si_info_copy, buffer_usage,
                                           std::move(pool_id), &buffer_handle);
  if (mailbox.IsZero()) {
    return nullptr;
  }

  CHECK(!buffer_handle.is_null());
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(mailbox), si_info_copy.meta, GenUnverifiedSyncToken(),
      GpuMemoryBufferHandleInfo(std::move(buffer_handle),
                                si_info_copy.meta.format,
                                si_info_copy.meta.size, buffer_usage),
      holder_, shared_memory_pool_);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage))
      << uint32_t(si_info.meta.usage);
  DCHECK(viz::HasEquivalentBufferFormat(si_info.meta.format))
      << si_info.meta.format.ToString();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  CHECK(!si_info.meta.format.PrefersExternalSampler())
      << si_info.meta.format.ToString();
#endif
  auto client_buffer_handle = buffer_handle.Clone();
  auto mailbox = proxy_->CreateSharedImage(si_info, std::move(buffer_handle));
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(mailbox), si_info.meta, GenUnverifiedSyncToken(),
      GpuMemoryBufferHandleInfo(std::move(client_buffer_handle),
                                si_info.meta.format, si_info.meta.size,
                                buffer_usage),
      holder_, shared_memory_pool_);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage))
      << uint32_t(si_info.meta.usage);
  DCHECK(viz::HasEquivalentBufferFormat(si_info.meta.format))
      << si_info.meta.format.ToString();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  CHECK(!si_info.meta.format.PrefersExternalSampler())
      << si_info.meta.format.ToString();
#endif
  auto buffer_handle_type = buffer_handle.type;
  auto mailbox = proxy_->CreateSharedImage(si_info, std::move(buffer_handle));
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(mailbox), si_info.meta, GenUnverifiedSyncToken(), holder_,
      buffer_handle_type);
}

scoped_refptr<ClientSharedImage>
ClientSharedImageInterface::CreateSharedImageForMLTensor(
    std::string debug_label,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    gpu::SharedImageUsageSet usage) {
  CHECK(gpu::IsValidClientUsage(usage)) << uint32_t(usage);
  CHECK(usage.Has(SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR));

  const SharedImageInfo si_info = {format, std::move(size), gfx::ColorSpace(),
                                   usage, std::move(debug_label)};

  auto mailbox = proxy_->CreateSharedImage(si_info, std::nullopt);
  if (mailbox.IsZero()) {
    return nullptr;
  }

  return base::WrapRefCounted<ClientSharedImage>(new ClientSharedImage(
      AddMailbox(mailbox), si_info.meta, GenUnverifiedSyncToken(), holder_,
      gfx::EMPTY_BUFFER));
}

scoped_refptr<ClientSharedImage>
ClientSharedImageInterface::CreateSharedImageForSoftwareCompositor(
    const SharedImageInfo& si_info) {
  base::WritableSharedMemoryMapping mapping;
  gfx::GpuMemoryBufferHandle handle;
  CreateSharedMemoryRegionFromSIInfo(si_info, mapping, handle);

  auto mailbox = proxy_->CreateSharedImage(si_info, std::move(handle));

  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(mailbox), si_info.meta, GenUnverifiedSyncToken(), holder_,
      std::move(mapping));
}

void ClientSharedImageInterface::CopyToGpuMemoryBuffer(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  proxy_->CopyToGpuMemoryBuffer(sync_token, mailbox);
}

#if BUILDFLAG(IS_WIN)
void ClientSharedImageInterface::CopyToGpuMemoryBufferAsync(
    const SyncToken& sync_token,
    const Mailbox& mailbox,
    base::OnceCallback<void(bool)> callback) {
  proxy_->CopyToGpuMemoryBufferAsync(sync_token, mailbox, std::move(callback));
}

void ClientSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    scoped_refptr<gfx::D3DSharedFence> d3d_shared_fence,
    const Mailbox& mailbox) {
  proxy_->UpdateSharedImage(sync_token, std::move(d3d_shared_fence), mailbox);
}

bool ClientSharedImageInterface::CopyNativeGmbToSharedMemorySync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region) {
  CHECK_EQ(buffer_handle.type, gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE);
  CHECK(memory_region.IsValid());
  bool status = false;
  proxy_->CopyNativeGmbToSharedMemorySync(std::move(buffer_handle),
                                          std::move(memory_region), &status);
  return status;
}

void ClientSharedImageInterface::CopyNativeGmbToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  CHECK_EQ(buffer_handle.type, gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE);
  CHECK(memory_region.IsValid());
  proxy_->CopyNativeGmbToSharedMemoryAsync(
      std::move(buffer_handle), std::move(memory_region), std::move(callback));
}

bool ClientSharedImageInterface::IsConnected() {
  return proxy_->IsConnected();
}
#endif

ClientSharedImageInterface::SwapChainSharedImages
ClientSharedImageInterface::CreateSwapChain(viz::SharedImageFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            GrSurfaceOrigin surface_origin,
                                            SkAlphaType alpha_type,
                                            gpu::SharedImageUsageSet usage) {
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
    int& ref_count = it->second;
    CHECK_GT(ref_count, 0);
    if (--ref_count == 0) {
      mailboxes_.erase(it);
    }
  }
  proxy_->DestroySharedImage(sync_token, mailbox);
}

void ClientSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    scoped_refptr<ClientSharedImage> client_shared_image) {
  CHECK(client_shared_image->HasOneRef());
  CHECK(client_shared_image->HasHolder());
  client_shared_image->UpdateDestructionSyncToken(sync_token);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::ImportSharedImage(
    ExportedSharedImage exported_shared_image) {
  const auto& mailbox = exported_shared_image.mailbox_;
  const auto& sync_token = exported_shared_image.creation_sync_token_;

  DCHECK(!mailbox.IsZero());
  AddMailbox(mailbox);
  proxy_->AddReferenceToSharedImage(sync_token, mailbox);

  return base::WrapRefCounted<ClientSharedImage>(
      new ClientSharedImage(std::move(exported_shared_image), holder_));
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::NotifyMailboxAdded(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage) {
  AddMailbox(mailbox);
  proxy_->NotifyMailboxAdded(mailbox, usage);

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox,
      SharedImageMetadata(format, size, color_space, surface_origin, alpha_type,
                          usage),
      GenUnverifiedSyncToken(), holder_, gfx::EMPTY_BUFFER);
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::NotifyMailboxAdded(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    uint32_t texture_target) {
  AddMailbox(mailbox);
  proxy_->NotifyMailboxAdded(mailbox, usage);

  SharedImageMetadata metadata(format, size, color_space, surface_origin,
                               alpha_type, usage);
  return base::WrapRefCounted<ClientSharedImage>(new ClientSharedImage(
      mailbox, metadata, GenUnverifiedSyncToken(), holder_, texture_target));
}

Mailbox ClientSharedImageInterface::AddMailbox(const gpu::Mailbox& mailbox) {
  if (mailbox.IsZero())
    return mailbox;

  base::AutoLock lock(lock_);
  CHECK_GE(mailboxes_[mailbox], 0);
  mailboxes_[mailbox]++;
  return mailbox;
}

const SharedImageCapabilities& ClientSharedImageInterface::GetCapabilities() {
  return proxy_->GetCapabilities();
}

void ClientSharedImageInterface::CreateSharedImagePool(
    const SharedImagePoolId& pool_id,
    mojo::PendingRemote<mojom::SharedImagePoolClientInterface> client_remote) {
  proxy_->CreateSharedImagePool(pool_id, std::move(client_remote));
}

void ClientSharedImageInterface::DestroySharedImagePool(
    const SharedImagePoolId& pool_id) {
  proxy_->DestroySharedImagePool(pool_id);
}

}  // namespace gpu

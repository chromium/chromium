// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/client_shared_image_interface.h"

#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/shared_image_interface_proxy.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

ClientSharedImageInterface::ClientSharedImageInterface(
    SharedImageInterfaceProxy* proxy)
    : proxy_(proxy) {}

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

Mailbox ClientSharedImageInterface::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_EQ(surface_handle, kNullSurfaceHandle);
  DCHECK(gpu::IsValidClientUsage(usage));
  return AddMailbox(proxy_->CreateSharedImage(
      format, size, color_space, surface_origin, alpha_type, usage));
}

Mailbox ClientSharedImageInterface::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  // Pixel upload path only supports single-planar formats.
  DCHECK(format.is_single_plane());
  DCHECK(gpu::IsValidClientUsage(usage));
  return AddMailbox(proxy_->CreateSharedImage(format, size, color_space,
                                              surface_origin, alpha_type, usage,
                                              pixel_data));
}

Mailbox ClientSharedImageInterface::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK(gpu::IsValidClientUsage(usage));
  DCHECK(viz::HasEquivalentBufferFormat(format));
  DCHECK(format.is_multi_plane());
  return AddMailbox(proxy_->CreateSharedImage(format, size, color_space,
                                              surface_origin, alpha_type, usage,
                                              std::move(buffer_handle)));
}

Mailbox ClientSharedImageInterface::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferPlane plane,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  DCHECK(gpu::IsValidClientUsage(usage));
  return AddMailbox(proxy_->CreateSharedImage(
      gpu_memory_buffer->GetFormat(), plane, gpu_memory_buffer->GetSize(),
      color_space, surface_origin, alpha_type, usage,
      gpu_memory_buffer->CloneHandle()));
}

#if BUILDFLAG(IS_WIN)
void ClientSharedImageInterface::CopyToGpuMemoryBuffer(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  proxy_->CopyToGpuMemoryBuffer(sync_token, mailbox);
}
#endif

ClientSharedImageInterface::SwapChainMailboxes
ClientSharedImageInterface::CreateSwapChain(viz::ResourceFormat format,
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
  return mailboxes;
}

void ClientSharedImageInterface::DestroySharedImage(const SyncToken& sync_token,
                                                    const Mailbox& mailbox) {
  DCHECK(!mailbox.IsZero());

  {
    base::AutoLock lock(lock_);
    DCHECK_NE(mailboxes_.count(mailbox), 0u);
    mailboxes_.erase(mailbox);
  }
  proxy_->DestroySharedImage(sync_token, mailbox);
}

uint32_t ClientSharedImageInterface::UsageForMailbox(const Mailbox& mailbox) {
  return proxy_->UsageForMailbox(mailbox);
}

void ClientSharedImageInterface::NotifyMailboxAdded(const Mailbox& mailbox,
                                                    uint32_t usage) {
  AddMailbox(mailbox);
  proxy_->NotifyMailboxAdded(mailbox, usage);
}

Mailbox ClientSharedImageInterface::AddMailbox(const gpu::Mailbox& mailbox) {
  if (mailbox.IsZero())
    return mailbox;

  base::AutoLock lock(lock_);
  DCHECK_EQ(mailboxes_.count(mailbox), 0u);
  mailboxes_.insert(mailbox);
  return mailbox;
}

}  // namespace gpu

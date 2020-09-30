// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/client_shared_image_interface.h"

#include "gpu/ipc/client/shared_image_interface_proxy.h"
#include "ui/gfx/gpu_fence.h"

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

#if defined(OS_FUCHSIA)
void ClientSharedImageInterface::RegisterSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id,
    zx::channel token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  proxy_->RegisterSysmemBufferCollection(id, std::move(token), format, usage,
                                         register_with_image_pipe);
}

void ClientSharedImageInterface::ReleaseSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id) {
  proxy_->ReleaseSysmemBufferCollection(id);
}
#endif  // defined(OS_FUCHSIA)

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
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_EQ(surface_handle, kNullSurfaceHandle);
  return AddMailbox(proxy_->CreateSharedImage(
      format, size, color_space, surface_origin, alpha_type, usage));
}

Mailbox ClientSharedImageInterface::CreateSharedImage(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  return AddMailbox(proxy_->CreateSharedImage(format, size, color_space,
                                              surface_origin, alpha_type, usage,
                                              pixel_data));
}

Mailbox ClientSharedImageInterface::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  return AddMailbox(proxy_->CreateSharedImage(
      gpu_memory_buffer, gpu_memory_buffer_manager, color_space, surface_origin,
      alpha_type, usage));
}

#if defined(OS_ANDROID)
Mailbox ClientSharedImageInterface::CreateSharedImageWithAHB(
    const Mailbox& mailbox,
    uint32_t usage,
    const SyncToken& sync_token) {
  return AddMailbox(
      proxy_->CreateSharedImageWithAHB(mailbox, usage, sync_token));
}
#endif

ClientSharedImageInterface::SwapChainMailboxes
ClientSharedImageInterface::CreateSwapChain(viz::ResourceFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            GrSurfaceOrigin surface_origin,
                                            SkAlphaType alpha_type,
                                            uint32_t usage) {
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

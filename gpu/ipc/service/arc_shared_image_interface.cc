// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/arc_shared_image_interface.h"

#include "base/notimplemented.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {

ArcSharedImageInterface::ArcSharedImageInterface() = default;

ArcSharedImageInterface::~ArcSharedImageInterface() = default;

scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  NOTIMPLEMENTED();
  return nullptr;
}

void ArcSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    scoped_refptr<ClientSharedImage> client_shared_image) {
  NOTIMPLEMENTED();
}

scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    std::optional<SharedImagePoolId> pool_id /*=std::nullopt*/) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    std::optional<SharedImagePoolId> pool_id /*=std::nullopt*/) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage> ArcSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage>
ArcSharedImageInterface::CreateSharedImageForMLTensor(
    std::string debug_label,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    gpu::SharedImageUsageSet usage) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage>
ArcSharedImageInterface::CreateSharedImageForSoftwareCompositor(
    const SharedImageInfo& si_info) {
  NOTREACHED();
}
void ArcSharedImageInterface::UpdateSharedImage(const SyncToken& sync_token,
                                                const Mailbox& mailbox) {
  NOTREACHED();
}
void ArcSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  NOTREACHED();
}
void ArcSharedImageInterface::DestroySharedImage(const SyncToken& sync_token,
                                                 const Mailbox& mailbox) {
  NOTREACHED();
}
scoped_refptr<ClientSharedImage> ArcSharedImageInterface::ImportSharedImage(
    ExportedSharedImage exported_shared_image) {
  NOTREACHED();
}
SharedImageInterface::SwapChainSharedImages
ArcSharedImageInterface::CreateSwapChain(viz::SharedImageFormat format,
                                         const gfx::Size& size,
                                         const gfx::ColorSpace& color_space,
                                         GrSurfaceOrigin surface_origin,
                                         SkAlphaType alpha_type,
                                         SharedImageUsageSet usage,
                                         std::string_view debug_label) {
  NOTREACHED();
}
void ArcSharedImageInterface::PresentSwapChain(const SyncToken& sync_token,
                                               const Mailbox& mailbox) {
  NOTREACHED();
}
SyncToken ArcSharedImageInterface::GenUnverifiedSyncToken() {
  NOTREACHED();
}
SyncToken ArcSharedImageInterface::GenVerifiedSyncToken() {
  NOTREACHED();
}
void ArcSharedImageInterface::VerifySyncToken(SyncToken& sync_token) {
  NOTREACHED();
}
void ArcSharedImageInterface::WaitSyncToken(const SyncToken& sync_token) {
  NOTREACHED();
}

const SharedImageCapabilities& ArcSharedImageInterface::GetCapabilities() {
  NOTREACHED();
}

}  // namespace gpu

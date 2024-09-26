// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/shared_image_interface.h"

#include <GLES2/gl2.h>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

namespace gpu {

SharedImageInterface::SwapChainSharedImages::SwapChainSharedImages(
    scoped_refptr<gpu::ClientSharedImage> front_buffer,
    scoped_refptr<gpu::ClientSharedImage> back_buffer)
    : front_buffer(std::move(front_buffer)),
      back_buffer(std::move(back_buffer)) {}
SharedImageInterface::SwapChainSharedImages::SwapChainSharedImages(
    const SwapChainSharedImages& shared_images) = default;
SharedImageInterface::SwapChainSharedImages::~SwapChainSharedImages() = default;

SharedImageInterface::SharedImageInterface()
    : holder_(base::MakeRefCounted<SharedImageInterfaceHolder>(this)) {}
SharedImageInterface::~SharedImageInterface() = default;

scoped_refptr<ClientSharedImage> SharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  NOTREACHED_IN_MIGRATION();
  return base::MakeRefCounted<ClientSharedImage>(Mailbox(), si_info.meta,
                                                 GenUnverifiedSyncToken(),
                                                 holder_, gfx::EMPTY_BUFFER);
}

SharedImageUsageSet SharedImageInterface::UsageForMailbox(
    const Mailbox& mailbox) {
  return SharedImageUsageSet();
}

scoped_refptr<ClientSharedImage>
SharedImageInterface::AddReferenceToSharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    uint32_t texture_target) {
  return ImportSharedImage(ExportedSharedImage(
      mailbox,
      SharedImageMetadata{format, size, color_space, surface_origin, alpha_type,
                          usage},
      sync_token, texture_target));
}

scoped_refptr<ClientSharedImage> SharedImageInterface::NotifyMailboxAdded(
    const Mailbox& /*mailbox*/,
    viz::SharedImageFormat /*format*/,
    const gfx::Size& /*size*/,
    const gfx::ColorSpace& /*color_space*/,
    GrSurfaceOrigin /*surface_origin*/,
    SkAlphaType /*alpha_type*/,
    SharedImageUsageSet /*usage*/) {
  return nullptr;
}

scoped_refptr<ClientSharedImage> SharedImageInterface::NotifyMailboxAdded(
    const Mailbox& /*mailbox*/,
    viz::SharedImageFormat /*format*/,
    const gfx::Size& /*size*/,
    const gfx::ColorSpace& /*color_space*/,
    GrSurfaceOrigin /*surface_origin*/,
    SkAlphaType /*alpha_type*/,
    SharedImageUsageSet /*usage*/,
    uint32_t /*texture_target*/) {
  return nullptr;
}

void SharedImageInterface::CopyToGpuMemoryBuffer(const SyncToken& sync_token,
                                                 const Mailbox& mailbox) {
  NOTREACHED_IN_MIGRATION();
}

void SharedImageInterface::CopyToGpuMemoryBufferAsync(
    const SyncToken& sync_token,
    const Mailbox& mailbox,
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED_IN_MIGRATION();
}

bool SharedImageInterface::CopyNativeGmbToSharedMemorySync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region) {
  NOTREACHED();
}

void SharedImageInterface::CopyNativeGmbToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED();
}

void SharedImageInterface::Release() const {
  bool should_destroy = false;

  {
    base::AutoLock auto_lock(holder_->lock_);
    if (base::subtle::RefCountedThreadSafeBase::Release()) {
      ANALYZER_SKIP_THIS_PATH();
      holder_->OnDestroy();
      should_destroy = true;
    }
  }

  if (should_destroy) {
    delete this;
  }
}

#if BUILDFLAG(IS_WIN)
void SharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    scoped_refptr<gfx::D3DSharedFence> d3d_shared_fence,
    const Mailbox& mailbox) {
  NOTIMPLEMENTED_LOG_ONCE();
}
#endif  // BUILDFLAG(IS_WIN)

SharedImageInterface::SharedImageMapping::SharedImageMapping() = default;
SharedImageInterface::SharedImageMapping::SharedImageMapping(
    SharedImageInterface::SharedImageMapping&& mapped) = default;
SharedImageInterface::SharedImageMapping::SharedImageMapping(
    scoped_refptr<ClientSharedImage> shared_image,
    base::WritableSharedMemoryMapping mapping)
    : shared_image(std::move(shared_image)), mapping(std::move(mapping)) {}
SharedImageInterface::SharedImageMapping&
SharedImageInterface::SharedImageMapping::operator=(
    SharedImageInterface::SharedImageMapping&& mapped) = default;
SharedImageInterface::SharedImageMapping::~SharedImageMapping() = default;

SharedImageInterfaceHolder::SharedImageInterfaceHolder(
    SharedImageInterface* sii)
    : sii_(sii) {}
SharedImageInterfaceHolder::~SharedImageInterfaceHolder() = default;

scoped_refptr<SharedImageInterface> SharedImageInterfaceHolder::Get() {
  base::AutoLock auto_lock(lock_);
  return scoped_refptr<SharedImageInterface>(sii_);
}

void SharedImageInterfaceHolder::OnDestroy() {
  lock_.AssertAcquired();
  sii_ = nullptr;
}

}  // namespace gpu

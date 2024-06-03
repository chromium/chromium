// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/test_shared_image_interface.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

namespace {

gfx::GpuMemoryBufferType GetNativeBufferType() {
#if BUILDFLAG(IS_APPLE)
  return gfx::IO_SURFACE_BUFFER;
#elif BUILDFLAG(IS_ANDROID)
  return gfx::ANDROID_HARDWARE_BUFFER;
#elif BUILDFLAG(IS_WIN)
  return gfx::DXGI_SHARED_HANDLE;
#else
  // Ozone
  return gfx::NATIVE_PIXMAP;
#endif
}

// Creates a shared memory region and returns a handle to it.
gfx::GpuMemoryBufferHandle CreateGMBHandle(
    const gfx::BufferFormat& buffer_format,
    const gfx::Size& size,
    gfx::BufferUsage buffer_usage) {
  static int last_handle_id = 0;
  size_t buffer_size = 0u;
  CHECK(
      gfx::BufferSizeForBufferFormatChecked(size, buffer_format, &buffer_size));
  auto shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  CHECK(shared_memory_region.IsValid());

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = gfx::GpuMemoryBufferId(last_handle_id++);
  handle.offset = 0;
  handle.stride = static_cast<uint32_t>(
      gfx::RowSizeForBufferFormat(size.width(), buffer_format, 0));
  handle.region = std::move(shared_memory_region);

  return handle;
}

}  // namespace

TestSharedImageInterface::TestSharedImageInterface() = default;
TestSharedImageInterface::~TestSharedImageInterface() = default;

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImage(const SharedImageInfo& si_info,
                                            SurfaceHandle surface_handle) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.meta.size;
  auto gmb_handle_type = emulate_client_provided_native_buffer_
                             ? GetNativeBufferType()
                             : gfx::EMPTY_BUFFER;
  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, sync_token, holder_, gmb_handle_type);
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, sync_token, holder_, gfx::EMPTY_BUFFER);
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImage(const SharedImageInfo& si_info,
                                            SurfaceHandle surface_handle,
                                            gfx::BufferUsage buffer_usage) {
  if (fail_shared_image_creation_with_buffer_usage_) {
    return nullptr;
  }
  SyncToken sync_token = GenUnverifiedSyncToken();

  // Create a ClientSharedImage with a GMB.
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.meta.size;

  auto buffer_format =
      viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
          si_info.meta.format);
  if (test_gmb_manager_) {
    auto gpu_memory_buffer = test_gmb_manager_->CreateGpuMemoryBuffer(
        si_info.meta.size, buffer_format, buffer_usage, surface_handle,
        nullptr);

    // Since the |gpu_memory_buffer| here is always a shared memory, clear the
    // external sampler prefs if it is already set by client.
    // https://issues.chromium.org/339546249.
    SharedImageInfo si_info_copy = si_info;
    if (si_info_copy.meta.format.PrefersExternalSampler()) {
      si_info_copy.meta.format.ClearPrefersExternalSampler();
    }
    return ClientSharedImage::CreateForTesting(
        mailbox, si_info_copy.meta, sync_token, std::move(gpu_memory_buffer),
        holder_);
  }

  auto gmb_handle =
      CreateGMBHandle(buffer_format, si_info.meta.size, buffer_usage);

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, sync_token,
      GpuMemoryBufferHandleInfo(std::move(gmb_handle), si_info.meta.format,
                                     si_info.meta.size, buffer_usage),
      holder_);
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.meta.size;

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, sync_token,
      GpuMemoryBufferHandleInfo(std::move(buffer_handle),
                                     si_info.meta.format, si_info.meta.size,
                                     buffer_usage),
      holder_);
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.meta.size;
  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info.meta, sync_token, holder_, buffer_handle.type);
}

SharedImageInterface::SharedImageMapping
TestSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.meta.size;
  return {base::MakeRefCounted<ClientSharedImage>(
              mailbox, si_info.meta, sync_token, holder_, gfx::EMPTY_BUFFER),
          base::WritableSharedMemoryMapping()};
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferPlane plane,
    const SharedImageInfo& si_info) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = gpu_memory_buffer->GetSize();
  return base::MakeRefCounted<ClientSharedImage>(
      mailbox,
      SharedImageMetadata(
          viz::GetSinglePlaneSharedImageFormat(
              GetPlaneBufferFormat(plane, gpu_memory_buffer->GetFormat())),
          most_recent_size_, si_info.meta.color_space,
          si_info.meta.surface_origin, si_info.meta.alpha_type,
          si_info.meta.usage),
      sync_token, holder_, gpu_memory_buffer->GetType());
}

void TestSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  base::AutoLock locked(lock_);
  DCHECK(shared_images_.find(mailbox) != shared_images_.end());
}

void TestSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  base::AutoLock locked(lock_);
  DCHECK(shared_images_.find(mailbox) != shared_images_.end());
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::ImportSharedImage(
    const ExportedSharedImage& exported_shared_image) {
  shared_images_.insert(exported_shared_image.mailbox_);

  return base::WrapRefCounted<ClientSharedImage>(
      new ClientSharedImage(
          exported_shared_image.mailbox_, exported_shared_image.metadata_,
          exported_shared_image.creation_sync_token_, holder_,
          exported_shared_image.texture_target_));
}

void TestSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  base::AutoLock locked(lock_);
  shared_images_.erase(mailbox);
  most_recent_destroy_token_ = sync_token;
}

void TestSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    scoped_refptr<ClientSharedImage> client_shared_image) {
  CHECK(client_shared_image->HasOneRef());
  client_shared_image->UpdateDestructionSyncToken(sync_token);
  client_shared_image->MarkForDestruction();
}

SharedImageInterface::SwapChainSharedImages
TestSharedImageInterface::CreateSwapChain(viz::SharedImageFormat format,
                                          const gfx::Size& size,
                                          const gfx::ColorSpace& color_space,
                                          GrSurfaceOrigin surface_origin,
                                          SkAlphaType alpha_type,
                                          uint32_t usage) {
  auto front_buffer = Mailbox::Generate();
  auto back_buffer = Mailbox::Generate();
  SyncToken sync_token = GenUnverifiedSyncToken();
  shared_images_.insert(front_buffer);
  shared_images_.insert(back_buffer);
  return {base::MakeRefCounted<ClientSharedImage>(
              front_buffer,
              SharedImageMetadata(format, size, color_space,
                                       surface_origin, alpha_type, usage),
              sync_token, holder_, gfx::EMPTY_BUFFER),
          base::MakeRefCounted<ClientSharedImage>(
              back_buffer,
              SharedImageMetadata(format, size, color_space,
                                       surface_origin, alpha_type, usage),
              sync_token, holder_, gfx::EMPTY_BUFFER)};
}

void TestSharedImageInterface::PresentSwapChain(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {}

#if BUILDFLAG(IS_FUCHSIA)
void TestSharedImageInterface::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  NOTREACHED_IN_MIGRATION();
}
#endif  // BUILDFLAG(IS_FUCHSIA)

SyncToken TestSharedImageInterface::GenVerifiedSyncToken() {
  base::AutoLock locked(lock_);
  most_recent_generated_token_ =
      SyncToken(CommandBufferNamespace::GPU_IO,
                     CommandBufferId(), ++release_id_);
  VerifySyncToken(most_recent_generated_token_);
  return most_recent_generated_token_;
}

SyncToken TestSharedImageInterface::GenUnverifiedSyncToken() {
  base::AutoLock locked(lock_);
  most_recent_generated_token_ =
      SyncToken(CommandBufferNamespace::GPU_IO,
                     CommandBufferId(), ++release_id_);
  return most_recent_generated_token_;
}

void TestSharedImageInterface::VerifySyncToken(SyncToken& sync_token) {
  sync_token.SetVerifyFlush();
}

void TestSharedImageInterface::WaitSyncToken(const SyncToken& sync_token) {
  NOTREACHED_IN_MIGRATION();
}

void TestSharedImageInterface::Flush() {
  // No need to flush in this implementation.
}

scoped_refptr<gfx::NativePixmap> TestSharedImageInterface::GetNativePixmap(
    const Mailbox& mailbox) {
  return nullptr;
}

bool TestSharedImageInterface::CheckSharedImageExists(
    const Mailbox& mailbox) const {
  base::AutoLock locked(lock_);
  return shared_images_.contains(mailbox);
}

const SharedImageCapabilities&
TestSharedImageInterface::GetCapabilities() {
  return shared_image_capabilities_;
}

void TestSharedImageInterface::SetCapabilities(
    const SharedImageCapabilities& caps) {
  shared_image_capabilities_ = caps;
}

}  // namespace gpu

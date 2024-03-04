// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_ahardwarebuffer_image_representation.h"

#include <dawn/native/VulkanBackend.h>

#include "base/logging.h"

namespace gpu {

DawnAHardwareBufferImageRepresentation::DawnAHardwareBufferImageRepresentation(
    SharedImageManager* manager,
    AndroidImageBacking* backing,
    MemoryTypeTracker* tracker,
    wgpu::Device device,
    wgpu::TextureFormat format,
    std::vector<wgpu::TextureFormat> view_formats,
    AHardwareBuffer* buffer)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(std::move(device)),
      format_(format),
      view_formats_(std::move(view_formats)) {
  DCHECK(device_);
  handle_ = base::android::ScopedHardwareBufferHandle::Create(buffer);
}

DawnAHardwareBufferImageRepresentation::
    ~DawnAHardwareBufferImageRepresentation() {
  EndAccess();
}

wgpu::Texture DawnAHardwareBufferImageRepresentation::BeginAccess(
    wgpu::TextureUsage usage) {
  // It doesn't make sense to have two overlapping BeginAccess calls on the same
  // representation.
  if (texture_) {
    LOG(ERROR) << "Attempting to begin access before ending previous access.";
    return nullptr;
  }

  wgpu::TextureDescriptor texture_descriptor;
  texture_descriptor.format = format_;
  texture_descriptor.usage = static_cast<wgpu::TextureUsage>(usage);
  texture_descriptor.dimension = wgpu::TextureDimension::e2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  texture_descriptor.viewFormatCount = view_formats_.size();
  texture_descriptor.viewFormats = view_formats_.data();

  // We need to have internal usages of CopySrc for copies,
  // RenderAttachment for clears, and TextureBinding for copyTextureForBrowser.
  wgpu::DawnTextureInternalUsageDescriptor internalDesc;
  internalDesc.internalUsage = wgpu::TextureUsage::CopySrc |
                               wgpu::TextureUsage::RenderAttachment |
                               wgpu::TextureUsage::TextureBinding;

  texture_descriptor.nextInChain = &internalDesc;

  // Dawn currently doesn't support read-only access and hence concurrent reads.
  base::ScopedFD sync_fd;
  android_backing()->BeginWrite(&sync_fd);

  wgpu::SharedTextureMemoryBeginAccessDescriptor begin_access_desc = {};
  begin_access_desc.initialized = IsCleared();

  wgpu::SharedTextureMemoryVkImageLayoutBeginState begin_layout{};

  // TODO(crbug.com/327111284): Track layouts correctly.
  begin_layout.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  begin_layout.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  begin_access_desc.nextInChain = &begin_layout;

  wgpu::SharedFence shared_fence;
  // Pass 1 as the signaled value for the binary semaphore
  // (Dawn's SharedTextureMemoryVk verifies that this is the value passed).
  const uint64_t signaled_value = 1;

  // If the semaphore from BeginWrite is valid then pass it to
  // SharedTextureMemory::BeginAccess() below.
  if (sync_fd.is_valid()) {
    wgpu::SharedFenceVkSemaphoreSyncFDDescriptor sync_fd_desc;
    // NOTE: There is no ownership transfer here, as Dawn internally dup()s the
    // passed-in handle.
    sync_fd_desc.handle = sync_fd.get();
    wgpu::SharedFenceDescriptor fence_desc;
    fence_desc.nextInChain = &sync_fd_desc;
    shared_fence = device_.ImportSharedFence(&fence_desc);

    begin_access_desc.fenceCount = 1;
    begin_access_desc.fences = &shared_fence;
    begin_access_desc.signaledValues = &signaled_value;
  }

  if (!shared_texture_memory_) {
    wgpu::SharedTextureMemoryDescriptor desc = {};

    wgpu::SharedTextureMemoryAHardwareBufferDescriptor
        stm_ahardwarebuffer_desc = {};
    stm_ahardwarebuffer_desc.handle = handle_.get();

    desc.nextInChain = &stm_ahardwarebuffer_desc;
    shared_texture_memory_ = device_.ImportSharedTextureMemory(&desc);
    if (!shared_texture_memory_) {
      LOG(ERROR) << "Failed to create SharedTextureMemory from AHB";
      android_backing()->EndWrite(base::ScopedFD());
      return nullptr;
    }
  }

  texture_ = shared_texture_memory_.CreateTexture(&texture_descriptor);
  if (!texture_) {
    LOG(ERROR) << "Failed to create texture from SharedTextureMemory";
    android_backing()->EndWrite(base::ScopedFD());
    return nullptr;
  }

  if (!shared_texture_memory_.BeginAccess(texture_, &begin_access_desc)) {
    LOG(ERROR) << "Failed to begin access for texture";
    android_backing()->EndWrite(base::ScopedFD());
  }

  return texture_.Get();
}

void DawnAHardwareBufferImageRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  wgpu::SharedTextureMemoryEndAccessState end_access_desc = {};
  wgpu::SharedTextureMemoryVkImageLayoutEndState end_layout{};
  end_access_desc.nextInChain = &end_layout;

  CHECK(shared_texture_memory_.EndAccess(texture_, &end_access_desc));
  if (end_access_desc.initialized) {
    SetCleared();
  }

  wgpu::SharedFenceExportInfo export_info;
  wgpu::SharedFenceVkSemaphoreSyncFDExportInfo sync_fd_export_info;
  export_info.nextInChain = &sync_fd_export_info;

  // TODO(dawn:286): Handle waiting on multiple semaphores from dawn.
  DCHECK_EQ(end_access_desc.fenceCount, 1u);
  end_access_desc.fences[0].ExportInfo(&export_info);

  // Dawn will close its FD when `end_access_desc` falls out of scope, and so it
  // is necessary to dup() it to give AndroidImageBacking an FD that it can own.
  base::ScopedFD sync_fd = base::ScopedFD(dup(sync_fd_export_info.handle));
  android_backing()->EndWrite(std::move(sync_fd));

  texture_.Destroy();
  texture_ = nullptr;
}

}  // namespace gpu

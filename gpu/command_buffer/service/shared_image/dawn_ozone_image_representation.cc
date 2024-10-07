// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_ozone_image_representation.h"

#include <dawn/native/VulkanBackend.h>
#include <vulkan/vulkan.h>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"

namespace gpu {

DawnOzoneImageRepresentation::DawnOzoneImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    wgpu::Device device,
    wgpu::TextureFormat format,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<gfx::NativePixmap> pixmap)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(std::move(device)),
      format_(format),
      view_formats_(std::move(view_formats)),
      pixmap_(pixmap) {
  DCHECK(device_);
}

DawnOzoneImageRepresentation::~DawnOzoneImageRepresentation() {
  EndAccess();
}

wgpu::Texture DawnOzoneImageRepresentation::BeginAccess(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage) {
  // It doesn't make sense to have two overlapping BeginAccess calls on the same
  // representation.
  if (texture_) {
    return nullptr;
  }

  // For multi-planar formats, Mesa is yet to support to allocate and bind
  // vkmemory for each plane respectively.
  // https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/intel/vulkan/anv_formats.c#L765
  // For now we assume all plane handles are same, and we don't use the
  // VK_IMAGE_CREATE_DISJOINT_BIT when creating the vkimage for the pixmap.
  DCHECK(pixmap_->SupportsZeroCopyWebGPUImport() ||
         pixmap_->GetNumberOfPlanes() == 1)
      << "Disjoint Multi-plane importing is not supported.";

  std::vector<gfx::GpuFenceHandle> fences;
  bool need_end_fence;
  is_readonly_ =
      (usage & kWriteUsage) == 0 && (internal_usage & kWriteUsage) == 0;
  if (is_readonly_ && !IsCleared()) {
    // Read-only access of an uncleared texture is not allowed: clients
    // relying on Dawn's lazy clearing of uninitialized textures must make
    // this reliance explicit by passing a write usage.
    return nullptr;
  }

  if (!ozone_backing()->BeginAccess(
          /*readonly=*/is_readonly_, OzoneImageBacking::AccessStream::kWebGPU,
          &fences, need_end_fence)) {
    return nullptr;
  }
  DCHECK(need_end_fence || is_readonly_);

  gfx::Size pixmap_size = pixmap_->GetBufferSize();
  wgpu::TextureDescriptor texture_descriptor;
  texture_descriptor.format = format_;
  texture_descriptor.viewFormats = view_formats_.data();
  texture_descriptor.viewFormatCount = view_formats_.size();
  texture_descriptor.usage = static_cast<wgpu::TextureUsage>(usage);
  texture_descriptor.dimension = wgpu::TextureDimension::e2D;
  texture_descriptor.size = {static_cast<uint32_t>(pixmap_size.width()),
                             static_cast<uint32_t>(pixmap_size.height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;

  wgpu::DawnTextureInternalUsageDescriptor internalDesc;
  internalDesc.internalUsage = internal_usage;

  texture_descriptor.nextInChain = &internalDesc;

  dawn::native::vulkan::ExternalImageDescriptorDmaBuf descriptor = {};
  descriptor.cTextureDescriptor =
      reinterpret_cast<WGPUTextureDescriptor*>(&texture_descriptor);
  descriptor.isInitialized = IsCleared();

  // Import the dma-buf into Dawn via the Vulkan backend. As per the Vulkan
  // documentation, importing memory from a file descriptor transfers
  // ownership of the fd from the application to the Vulkan implementation.
  // Thus, we need to dup the fd so the fd corresponding to the dmabuf isn't
  // closed twice (once by ScopedFD and once by the Vulkan implementation).
  int fd = dup(pixmap_->GetDmaBufFd(0));
  descriptor.memoryFD = fd;
  for (uint32_t plane = 0u; plane < pixmap_->GetNumberOfPlanes(); ++plane) {
    descriptor.planeLayouts[plane].offset = pixmap_->GetDmaBufOffset(plane);
    descriptor.planeLayouts[plane].stride = pixmap_->GetDmaBufPitch(plane);
  }
  descriptor.drmModifier = pixmap_->GetBufferFormatModifier();
  descriptor.waitFDs = {};

  for (auto& fence : fences) {
    descriptor.waitFDs.push_back(fence.Release().release());
  }

  texture_ = wgpu::Texture::Acquire(
      dawn::native::vulkan::WrapVulkanImage(device_.Get(), &descriptor));
  if (!texture_) {
    ozone_backing()->EndAccess(is_readonly_,
                               OzoneImageBacking::AccessStream::kWebGPU,
                               gfx::GpuFenceHandle());
    close(fd);
  }

  return texture_.Get();
}

void DawnOzoneImageRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  // Grab the signal semaphore from dawn
  dawn::native::vulkan::ExternalImageExportInfoOpaqueFD export_info;
  if (!dawn::native::vulkan::ExportVulkanImage(
          texture_.Get(), VK_IMAGE_LAYOUT_UNDEFINED, &export_info)) {
    DLOG(ERROR) << "Failed to export Dawn Vulkan image.";
  } else {
    if (export_info.isInitialized) {
      SetCleared();
    }

    // TODO(hob): Handle waiting on multiple semaphores from dawn.
    DCHECK(export_info.semaphoreHandles.size() == 1);
    gfx::GpuFenceHandle fence;
    fence.Adopt(base::ScopedFD(export_info.semaphoreHandles[0]));
    ozone_backing()->EndAccess(is_readonly_,
                               OzoneImageBacking::AccessStream::kWebGPU,
                               std::move(fence));
  }
  texture_.Destroy();
  texture_ = nullptr;
}

}  // namespace gpu

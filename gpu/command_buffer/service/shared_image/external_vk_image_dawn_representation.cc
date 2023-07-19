// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/external_vk_image_dawn_representation.h"

#include <dawn/native/VulkanBackend.h>

#include <utility>
#include <vector>

#include "base/posix/eintr_wrapper.h"
#include "gpu/vulkan/vulkan_image.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurfaceMutableState.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"

namespace gpu {

ExternalVkImageDawnImageRepresentation::ExternalVkImageDawnImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    wgpu::Device device,
    wgpu::TextureFormat wgpu_format,
    std::vector<wgpu::TextureFormat> view_formats,
    base::ScopedFD memory_fd)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(std::move(device)),
      wgpu_format_(wgpu_format),
      view_formats_(std::move(view_formats)),
      memory_fd_(std::move(memory_fd)) {
  DCHECK(device_);
}

ExternalVkImageDawnImageRepresentation::
    ~ExternalVkImageDawnImageRepresentation() {
  EndAccess();
}

wgpu::Texture ExternalVkImageDawnImageRepresentation::BeginAccess(
    wgpu::TextureUsage usage) {
  DCHECK(begin_access_semaphores_.empty());
  if (!backing_impl()->BeginAccess(false, &begin_access_semaphores_,
                                   /*is_gl=*/false)) {
    return nullptr;
  }

  wgpu::TextureDescriptor texture_descriptor;
  texture_descriptor.format = wgpu_format_;
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

  dawn::native::vulkan::ExternalImageDescriptorOpaqueFD descriptor = {};
  descriptor.cTextureDescriptor =
      reinterpret_cast<WGPUTextureDescriptor*>(&texture_descriptor);
  descriptor.isInitialized = IsCleared();
  descriptor.allocationSize = backing_impl()->image()->device_size();
  descriptor.memoryTypeIndex = backing_impl()->image()->memory_type_index();
  descriptor.memoryFD = dup(memory_fd_.get());

  const GrBackendTexture& backend_texture = backing_impl()->backend_texture();
  GrVkImageInfo image_info;
  backend_texture.getVkImageInfo(&image_info);
  // We should either be importing the image from the external queue, or it
  // was just created with no queue ownership.
  DCHECK(image_info.fCurrentQueueFamily == VK_QUEUE_FAMILY_IGNORED ||
         image_info.fCurrentQueueFamily == VK_QUEUE_FAMILY_EXTERNAL);

  // Note: This assumes the previous owner of the shared image did not do a
  // layout transition on EndAccess, and saved the exported layout on the
  // GrBackendTexture.
  descriptor.releasedOldLayout = image_info.fImageLayout;
  descriptor.releasedNewLayout = image_info.fImageLayout;

  for (auto& external_semaphore : begin_access_semaphores_) {
    descriptor.waitFDs.push_back(
        external_semaphore.handle().TakeHandle().release());
  }

  texture_ = wgpu::Texture::Acquire(
      dawn::native::vulkan::WrapVulkanImage(device_.Get(), &descriptor));
  return texture_.Get();
}

void ExternalVkImageDawnImageRepresentation::EndAccess() {
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

    // Exporting to VK_IMAGE_LAYOUT_UNDEFINED means no transition should be
    // done. The old/new layouts are the same.
    DCHECK_EQ(export_info.releasedOldLayout, export_info.releasedNewLayout);

    // Save the layout on the GrBackendTexture. Other shared image
    // representations read it from here.
    GrBackendTexture backend_texture = backing_impl()->backend_texture();
    backend_texture.setMutableState(GrBackendSurfaceMutableState(
        export_info.releasedNewLayout, VK_QUEUE_FAMILY_EXTERNAL));

    // TODO(enga): Handle waiting on multiple semaphores from dawn
    DCHECK(export_info.semaphoreHandles.size() == 1);

    // Wrap file descriptor in a handle
    SemaphoreHandle handle(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
                           base::ScopedFD(export_info.semaphoreHandles[0]));

    auto semaphore = ExternalSemaphore::CreateFromHandle(
        backing_impl()->context_provider(), std::move(handle));

    backing_impl()->EndAccess(false, std::move(semaphore), /*is_gl=*/false);
  }

  // Destroy the texture, signaling the semaphore in dawn
  texture_.Destroy();
  texture_ = nullptr;

  // We have done with |begin_access_semaphores_|. They should have been waited.
  // So add them to pending semaphores for reusing or relaeasing.
  backing_impl()->AddSemaphoresToPendingListOrRelease(
      std::move(begin_access_semaphores_));
  begin_access_semaphores_.clear();
}

}  // namespace gpu

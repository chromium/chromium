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
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_defines.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/gbm_util.h"
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
  wgpu::TextureDescriptor texture_descriptor;
  texture_descriptor.format = format_;
  texture_descriptor.usage = static_cast<wgpu::TextureUsage>(usage);
  texture_descriptor.dimension = wgpu::TextureDimension::e2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()),
                             /*depthOrArrayLayers=*/1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  texture_descriptor.viewFormatCount = view_formats_.size();
  texture_descriptor.viewFormats = view_formats_.data();
  // It doesn't make sense to have two overlapping BeginAccess calls on the same
  // representation.
  // TODO(blundell):Switch to using the return value of
  // OzoneImageBacking::BeginAccess().
  if (texture_) {
    LOG(ERROR)
        << "Attempting to begin access with before ending previous access.";
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

  if (!ozone_backing()->VaSync()) {
    return nullptr;
  }

  std::vector<gfx::GpuFenceHandle> fences;
  bool need_end_fence;
  if (base::FeatureList::IsEnabled(
          features::kDawnSIRepsUseClientProvidedInternalUsages)) {
    is_readonly_ =
        (usage & kWriteUsage) == 0 && (internal_usage & kWriteUsage) == 0;
    if (is_readonly_ && !IsCleared()) {
      // Read-only access of an uncleared texture is not allowed: clients
      // relying on Dawn's lazy clearing of uninitialized textures must make
      // this reliance explicit by passing a write usage.
      return nullptr;
    }
  } else {
    // We will treat this access as a write if the client 'usage' implies write
    // access and if the image has been initialized (cleared). The latter test
    // is necessary because internally dawn will initialize (clear) the buffer
    // if it has not yet been cleared.
    is_readonly_ = (usage & kWriteUsage) == 0 && IsCleared();
  }

  if (!ozone_backing()->BeginAccess(is_readonly_,
                                    OzoneImageBacking::AccessStream::kWebGPU,
                                    &fences, need_end_fence)) {
    return nullptr;
  }
  DCHECK(need_end_fence || is_readonly_);

  wgpu::SharedTextureMemoryBeginAccessDescriptor begin_access_desc = {};
  begin_access_desc.initialized = IsCleared();

  wgpu::SharedTextureMemoryVkImageLayoutBeginState begin_layout{};

  // TODO(crbug.com/330385376): Track layouts correctly.
  begin_layout.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  begin_layout.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  begin_access_desc.nextInChain = &begin_layout;

  wgpu::SharedFence shared_fence;
  // Pass 1 as the signaled value for the binary semaphore
  // (Dawn's SharedTextureMemoryVk verifies that this is the value passed).
  const uint64_t signaled_value = 1;
  CHECK_LE(fences.size(), 1u);
  // If the semaphore from BeginWrite is valid then pass it to
  // SharedTextureMemory::BeginAccess() below.
  if (fences.size()) {
    wgpu::SharedFenceVkSemaphoreSyncFDDescriptor sync_fd_desc;
    // NOTE: There is no ownership transfer here, as Dawn internally dup()s
    // the passed-in handle.
    sync_fd_desc.handle = fences[0].Peek();
    wgpu::SharedFenceDescriptor fence_desc;
    fence_desc.nextInChain = &sync_fd_desc;
    shared_fence = device_.ImportSharedFence(&fence_desc);

    begin_access_desc.fenceCount = 1;
    begin_access_desc.fences = &shared_fence;
    begin_access_desc.signaledValues = &signaled_value;
  }

  gfx::Size pixmap_size = pixmap_->GetBufferSize();
  wgpu::SharedTextureMemoryDmaBufDescriptor dmaBufDesc;
  dmaBufDesc.size = {static_cast<uint32_t>(pixmap_size.width()),
                     static_cast<uint32_t>(pixmap_size.height())};

  wgpu::DawnTextureInternalUsageDescriptor internalDesc;
  if (base::FeatureList::IsEnabled(
          features::kDawnSIRepsUseClientProvidedInternalUsages)) {
    internalDesc.internalUsage = internal_usage;
  } else {
    // We need to have internal usages of CopySrc for copies and TextureBinding
    // for copyTextureForBrowser.
    internalDesc.internalUsage = wgpu::TextureUsage::CopySrc;
    // No write access to multi-planar pixmaps.
    if (pixmap_->GetNumberOfPlanes() == 1) {
      internalDesc.internalUsage |= wgpu::TextureUsage::TextureBinding;
      if (!IsCleared()) {
        // RenderAttachment for (lazy) clears.
        internalDesc.internalUsage |= wgpu::TextureUsage::RenderAttachment;
      }
    }
  }

  texture_descriptor.nextInChain = &internalDesc;
  // TODO(petermcneeley): We should have stored the FourCC format in the pixmap.
  // We should use that saved value rather than having to recalculated it here.
  dmaBufDesc.drmFormat =
      ui::GetFourCCFormatFromBufferFormat(pixmap_->GetBufferFormat());
  dmaBufDesc.drmModifier = pixmap_->GetBufferFormatModifier();

  wgpu::SharedTextureMemoryDmaBufPlane planes[GBM_MAX_PLANES];
  dmaBufDesc.planeCount = pixmap_->GetNumberOfPlanes();
  dmaBufDesc.planes = planes;
  CHECK_GE(static_cast<unsigned long>(GBM_MAX_PLANES), dmaBufDesc.planeCount);

  // We assume the pixmap is not disjoint (VK_IMAGE_CREATE_DISJOINT_BIT). All
  // planes will have the the same fd but different pitch/offsets. This will not
  // actually be reflected in the fds for each plane due to duping of the same
  // fd elsewhere. This is why we cannot (d)check for this condition.
  const int fd_for_all_planes = pixmap_->GetDmaBufFd(0);
  for (uint32_t plane_idx = 0; plane_idx < dmaBufDesc.planeCount; ++plane_idx) {
    // Dawn is not an ownership transfer. Dawn will internally duplicate fds as
    // necessary.
    planes[plane_idx].fd = fd_for_all_planes;
    planes[plane_idx].stride = pixmap_->GetDmaBufPitch(plane_idx);
    planes[plane_idx].offset = pixmap_->GetDmaBufOffset(plane_idx);
  }

  wgpu::SharedTextureMemoryDescriptor desc;
  desc.label = "DawnOzoneImageRepresentation";
  desc.nextInChain = &dmaBufDesc;

  if (!shared_texture_memory_) {
    shared_texture_memory_ = device_.ImportSharedTextureMemory(&desc);
  }

  texture_ = shared_texture_memory_.CreateTexture(&texture_descriptor);
  if (!shared_texture_memory_.BeginAccess(texture_, &begin_access_desc)) {
    LOG(ERROR) << "Failed to begin access for shared image.";
    // End the access on the backing and restore its fence, as Dawn did not
    // consume it.
    ozone_backing()->EndAccess(
        is_readonly_, OzoneImageBacking::AccessStream::kWebGPU,
        fences.empty() ? gfx::GpuFenceHandle() : std::move(fences[0]));

    // Set `texture_` to nullptr to signal failure to BeginScopedAccess(),
    // which will itself then return nullptr to signal failure to the client.
    texture_ = nullptr;
  }

  return texture_;
}

void DawnOzoneImageRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }
  wgpu::SharedTextureMemoryEndAccessState end_access_desc = {};
  wgpu::SharedTextureMemoryVkImageLayoutEndState end_layout{};
  end_access_desc.nextInChain = &end_layout;

  if (!shared_texture_memory_.EndAccess(texture_, &end_access_desc)) {
    LOG(ERROR) << "Failed to end access for DawnOzoneImageRepresentation";
    texture_.Destroy();
    texture_ = nullptr;
    return;
  }

  if (end_access_desc.initialized) {
    SetCleared();
  }

  // Note: Dawn may export zero fences if there were no begin fences,
  // AND the WGPUTexture was not used on the GPU queue within the
  // access scope. Otherwise, it should either export fences from Dawn
  // signaled after the WGPUTexture's last use, or it should re-export
  // the begin fences if the WGPUTexture was unused.
  if (end_access_desc.fenceCount) {
    wgpu::SharedFenceExportInfo export_info;
    wgpu::SharedFenceVkSemaphoreSyncFDExportInfo sync_fd_export_info;
    export_info.nextInChain = &sync_fd_export_info;

    DCHECK_EQ(end_access_desc.fenceCount, 1u);
    end_access_desc.fences[0].ExportInfo(&export_info);

    // Dawn will close its FD when `end_access_desc` falls out of scope, and
    // so it is necessary to dup() it to give OzoneImageBacking an FD that it
    // can own.
    gfx::GpuFenceHandle fence;
    fence.Adopt(base::ScopedFD(dup(sync_fd_export_info.handle)));
    ozone_backing()->EndAccess(is_readonly_,
                               OzoneImageBacking::AccessStream::kWebGPU,
                               std::move(fence));
  }
  texture_.Destroy();
  texture_ = nullptr;
}

}  // namespace gpu

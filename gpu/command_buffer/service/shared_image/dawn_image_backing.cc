// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_image_backing.h"

#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"
#include "gpu/command_buffer/service/shared_image/dawn_copy_strategy.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace gpu {

class DawnImageRepresentationImpl : public DawnImageRepresentation {
 public:
  DawnImageRepresentationImpl(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker,
                              const wgpu::Device& device,
                              wgpu::Texture texture)
      : DawnImageRepresentation(manager, backing, tracker),
        device_(device),
        texture_(std::move(texture)) {}

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) final {
    return texture_;
  }

  void EndAccess() final {}

 private:
  const wgpu::Device device_;
  wgpu::Texture texture_;
};

DawnImageBacking::DawnImageBacking(const Mailbox& mailbox,
                                   const SharedImageInfo& si_info)
    : SharedImageBacking(mailbox,
                         si_info,
                         si_info.format.EstimatedSizeInBytes(si_info.size),
                         /*is_thread_safe=*/false) {}

DawnImageBacking::~DawnImageBacking() {
  if (texture_) {
    texture_.Destroy();
  }
}

bool DawnImageBacking::CheckSupportForAccessStream(
    SharedImageAccessStream stream,
    const AccessParams& params,
    const wgpu::Device& backing_device) {
  if (stream != SharedImageAccessStream::kDawn) {
    return false;
  }

  // For Dawn access, the `wgpu_device` is essential. Unlike GL or Vulkan, Dawn
  // operations are explicitly tied to a specific device, and there is no
  // implicit "current" context. Therefore, we must have the device to ensure
  // that this backing is compatible with the intended operation.
  if (!params.wgpu_device) {
    return false;
  }

  // If there is no DawnImageBacking instance yet and the request for
  // CheckSupportForAccessStream() is coming from factory, then return true.
  if (!backing_device) {
    return true;
  }

  return backing_device.Get() == params.wgpu_device.Get();
}

bool DawnImageBacking::SupportsAccess(SharedImageAccessStream stream,
                                      const AccessParams& params) const {
  return CheckSupportForAccessStream(stream, params, device_);
}

SharedImageBackingType DawnImageBacking::GetType() const {
  return SharedImageBackingType::kDawn;
}

gfx::Rect DawnImageBacking::ClearedRect() const {
  return gfx::Rect(size());
}

void DawnImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {}

std::unique_ptr<DawnImageRepresentation> DawnImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  if (!device_) {
    texture_ = CreateTexture(device, std::move(view_formats));
    if (!texture_) {
      return nullptr;
    }
    device_ = device;
  } else if (device_.Get() != device.Get()) {
    return nullptr;
  }

  return std::make_unique<DawnImageRepresentationImpl>(manager, this, tracker,
                                                       device, texture_);
}

void DawnImageBacking::Update(gfx::GpuFenceHandle in_fence) {}

bool DawnImageBacking::UploadFromMemory(const std::vector<SkPixmap>& pixmaps) {
  if (!device_ || !texture_) {
    LOG(ERROR) << "wgpu device or texture not present.";
    return false;
  }
  if (pixmaps.size() != static_cast<size_t>(format().NumberOfPlanes())) {
    return false;
  }

  wgpu::Queue queue = device_.GetQueue();
  wgpu::TextureFormat wgpu_format = texture_.GetFormat();

  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       ++plane_index) {
    const SkPixmap& pixmap = pixmaps[plane_index];
    const gfx::Size plane_size = format().GetPlaneSize(plane_index, size());
    wgpu::TexelCopyTextureInfo texture_copy = {
        .texture = texture_,
        .aspect = ToDawnTextureAspect(wgpu_format, plane_index),
    };
    wgpu::TexelCopyBufferLayout data_layout = {
        .offset = 0,
        .bytesPerRow = base::checked_cast<uint32_t>(pixmap.rowBytes()),
        .rowsPerImage = static_cast<uint32_t>(plane_size.height()),
    };
    wgpu::Extent3D extent = {static_cast<uint32_t>(plane_size.width()),
                             static_cast<uint32_t>(plane_size.height()), 1};
    queue.WriteTexture(&texture_copy, pixmap.addr(), pixmap.computeByteSize(),
                       &data_layout, &extent);
  }

  return true;
}

bool DawnImageBacking::ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) {
  if (!device_ || !texture_) {
    LOG(ERROR) << "wgpu device or texture not present.";
    return false;
  }
  return DawnCopyStrategy::CopyFromTextureToMemory(texture_, format(), size(),
                                                   device_, pixmaps);
}

wgpu::Texture DawnImageBacking::CreateTexture(
    const wgpu::Device& device,
    std::vector<wgpu::TextureFormat> view_formats) {
  wgpu::TextureDescriptor descriptor;
  descriptor.usage = static_cast<wgpu::TextureUsage>(
      wgpu::TextureUsage::RenderAttachment |
      wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
      wgpu::TextureUsage::CopyDst);
  descriptor.dimension = wgpu::TextureDimension::e2D;
  descriptor.size = {static_cast<uint32_t>(size().width()),
                     static_cast<uint32_t>(size().height()), 1};
  descriptor.format = ToDawnFormat(format());
  descriptor.viewFormatCount = view_formats.size();
  descriptor.viewFormats = view_formats.data();
  descriptor.mipLevelCount = 1;
  descriptor.sampleCount = 1;
  return device.CreateTexture(&descriptor);
}

void DawnImageBacking::InitializeForTesting(const wgpu::Device& device) {
  texture_ = CreateTexture(device, /*view_formats=*/{});
  device_ = device;
}

}  // namespace gpu

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_image_backing.h"

#include <vector>

#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

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
                                   viz::SharedImageFormat format,
                                   const gfx::Size& size,
                                   const gfx::ColorSpace& color_space,
                                   GrSurfaceOrigin surface_origin,
                                   SkAlphaType alpha_type,
                                   SharedImageUsageSet usage,
                                   std::string debug_label)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         std::move(debug_label),
                         format.EstimatedSizeInBytes(size),
                         /*is_thread_safe=*/false) {}

DawnImageBacking::~DawnImageBacking() {
  if (texture_) {
    texture_.Destroy();
  }
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
    wgpu::TextureDescriptor descriptor;
    descriptor.usage =
        static_cast<wgpu::TextureUsage>(wgpu::TextureUsage::RenderAttachment |
                                        wgpu::TextureUsage::TextureBinding);
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size = {static_cast<uint32_t>(size().width()),
                       static_cast<uint32_t>(size().height()), 1};
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.mipLevelCount = 1;
    descriptor.sampleCount = 1;
    texture_ = device.CreateTexture(&descriptor);
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

void DawnImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {}

void DawnImageBacking::InitializeForTesting(const wgpu::Device& device) {
  wgpu::TextureDescriptor descriptor;
  descriptor.usage =
      static_cast<wgpu::TextureUsage>(wgpu::TextureUsage::RenderAttachment |
                                      wgpu::TextureUsage::TextureBinding);
  descriptor.dimension = wgpu::TextureDimension::e2D;
  descriptor.size = {static_cast<uint32_t>(size().width()),
                     static_cast<uint32_t>(size().height()), 1};
  descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
  descriptor.mipLevelCount = 1;
  descriptor.sampleCount = 1;
  texture_ = device.CreateTexture(&descriptor);
  device_ = device;
}

}  // namespace gpu

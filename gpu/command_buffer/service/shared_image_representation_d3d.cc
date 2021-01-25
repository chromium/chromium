// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_d3d.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing_d3d.h"

namespace gpu {

SharedImageRepresentationGLTexturePassthroughD3D::
    SharedImageRepresentationGLTexturePassthroughD3D(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        scoped_refptr<gles2::TexturePassthrough> texture)
    : SharedImageRepresentationGLTexturePassthrough(manager, backing, tracker),
      texture_(std::move(texture)) {}

const scoped_refptr<gles2::TexturePassthrough>&
SharedImageRepresentationGLTexturePassthroughD3D::GetTexturePassthrough() {
  return texture_;
}

SharedImageRepresentationGLTexturePassthroughD3D::
    ~SharedImageRepresentationGLTexturePassthroughD3D() = default;

bool SharedImageRepresentationGLTexturePassthroughD3D::BeginAccess(
    GLenum mode) {
  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());
  return d3d_image_backing->BeginAccessD3D11();
}

void SharedImageRepresentationGLTexturePassthroughD3D::EndAccess() {
  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());
  d3d_image_backing->EndAccessD3D11();
}

#if BUILDFLAG(USE_DAWN)
SharedImageRepresentationDawnD3D::SharedImageRepresentationDawnD3D(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device)
    : SharedImageRepresentationDawn(manager, backing, tracker),
      device_(device),
      dawn_procs_(dawn_native::GetProcs()) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_.deviceReference(device_);
}

SharedImageRepresentationDawnD3D::~SharedImageRepresentationDawnD3D() {
  EndAccess();
  dawn_procs_.deviceRelease(device_);
}

WGPUTexture SharedImageRepresentationDawnD3D::BeginAccess(
    WGPUTextureUsage usage) {
  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());

  const HANDLE shared_handle = d3d_image_backing->GetSharedHandle();
  const viz::ResourceFormat viz_resource_format = d3d_image_backing->format();
  WGPUTextureFormat wgpu_format = viz::ToWGPUFormat(viz_resource_format);
  if (wgpu_format == WGPUTextureFormat_Undefined) {
    DLOG(ERROR) << "Unsupported viz format found: " << viz_resource_format;
    return nullptr;
  }

  uint64_t shared_mutex_acquire_key;
  if (!d3d_image_backing->BeginAccessD3D12(&shared_mutex_acquire_key)) {
    return nullptr;
  }

  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.nextInChain = nullptr;
  texture_descriptor.format = wgpu_format;
  texture_descriptor.usage = usage;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {size().width(), size().height(), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;

  dawn_native::d3d12::ExternalImageDescriptorDXGISharedHandle descriptor;
  descriptor.cTextureDescriptor = &texture_descriptor;
  descriptor.isInitialized = IsCleared();
  descriptor.sharedHandle = shared_handle;
  descriptor.acquireMutexKey = shared_mutex_acquire_key;
  descriptor.isSwapChainTexture =
      (d3d_image_backing->usage() &
       SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE);

  texture_ = dawn_native::d3d12::WrapSharedHandle(device_, &descriptor);
  if (!texture_) {
    d3d_image_backing->EndAccessD3D12();
  }

  return texture_;
}

void SharedImageRepresentationDawnD3D::EndAccess() {
  if (!texture_) {
    return;
  }

  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());

  if (dawn_native::IsTextureSubresourceInitialized(texture_, 0, 1, 0, 1)) {
    SetCleared();
  }

  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);

  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;

  d3d_image_backing->EndAccessD3D12();
}
#endif  // BUILDFLAG(USE_DAWN)

SharedImageRepresentationOverlayD3D::SharedImageRepresentationOverlayD3D(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationOverlay(manager, backing, tracker) {}

bool SharedImageRepresentationOverlayD3D::BeginReadAccess(
    std::vector<gfx::GpuFence>* acquire_fences) {
  // Note: only the DX11 video decoder uses this overlay and does not need to
  // synchronize read access from different devices.
  return true;
}

void SharedImageRepresentationOverlayD3D::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
}

gl::GLImage* SharedImageRepresentationOverlayD3D::GetGLImage() {
  return static_cast<SharedImageBackingD3D*>(backing())->GetGLImage();
}

}  // namespace gpu

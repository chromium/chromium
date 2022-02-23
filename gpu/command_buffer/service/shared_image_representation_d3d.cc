// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_d3d.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/constants.h"
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
    WGPUDevice device,
    dawn::native::d3d12::ExternalImageDXGI* external_image)
    : SharedImageRepresentationDawn(manager, backing, tracker),
      device_(device),
      external_image_(external_image),
      dawn_procs_(dawn::native::GetProcs()) {
  DCHECK(device_);
  DCHECK(external_image_);

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

  if (!d3d_image_backing->BeginAccessD3D12())
    return nullptr;

  dawn::native::d3d12::ExternalImageAccessDescriptorDXGIKeyedMutex descriptor;
  descriptor.isInitialized = IsCleared();
  descriptor.acquireMutexKey = kDXGIKeyedMutexAcquireKey;
  descriptor.releaseMutexKey = kDXGIKeyedMutexAcquireKey;
  descriptor.isSwapChainTexture =
      (d3d_image_backing->usage() &
       SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE);
  descriptor.usage = usage;

  DCHECK(external_image_);
  texture_ = external_image_->ProduceTexture(device_, &descriptor);
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

  if (dawn::native::IsTextureSubresourceInitialized(texture_, 0, 1, 0, 1)) {
    SetCleared();
  }

  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);

  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;
  external_image_ = nullptr;

  d3d_image_backing->EndAccessD3D12();
}
#endif  // BUILDFLAG(USE_DAWN)

SharedImageRepresentationOverlayD3D::SharedImageRepresentationOverlayD3D(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gl::GLImage> gl_image)
    : SharedImageRepresentationOverlay(manager, backing, tracker),
      gl_image_(std::move(gl_image)) {}

SharedImageRepresentationOverlayD3D::~SharedImageRepresentationOverlayD3D() =
    default;

bool SharedImageRepresentationOverlayD3D::BeginReadAccess(
    std::vector<gfx::GpuFence>* acquire_fences) {
  // Only D3D images need keyed mutex synchronization.
  if (gl_image_->GetType() == gl::GLImage::Type::D3D)
    return static_cast<SharedImageBackingD3D*>(backing())->BeginAccessD3D11();
  return true;
}

void SharedImageRepresentationOverlayD3D::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
  // Only D3D images need keyed mutex synchronization.
  if (gl_image_->GetType() == gl::GLImage::Type::D3D)
    static_cast<SharedImageBackingD3D*>(backing())->EndAccessD3D11();
}

gl::GLImage* SharedImageRepresentationOverlayD3D::GetGLImage() {
  return gl_image_.get();
}

}  // namespace gpu

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_representation.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"

namespace gpu {

GLTexturePassthroughD3DImageRepresentation::
    GLTexturePassthroughD3DImageRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        scoped_refptr<gles2::TexturePassthrough> texture)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      texture_(std::move(texture)) {}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughD3DImageRepresentation::GetTexturePassthrough() {
  return texture_;
}

GLTexturePassthroughD3DImageRepresentation::
    ~GLTexturePassthroughD3DImageRepresentation() = default;

bool GLTexturePassthroughD3DImageRepresentation::BeginAccess(GLenum mode) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  return d3d_image_backing->BeginAccessD3D11();
}

void GLTexturePassthroughD3DImageRepresentation::EndAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessD3D11();
}

#if BUILDFLAG(USE_DAWN)
DawnD3DImageRepresentation::DawnD3DImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    dawn::native::d3d12::ExternalImageDXGI* external_image)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(device),
      external_image_(external_image),
      dawn_procs_(dawn::native::GetProcs()) {
  DCHECK(device_);
  DCHECK(external_image_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_.deviceReference(device_);
}

DawnD3DImageRepresentation::~DawnD3DImageRepresentation() {
  EndAccess();
  dawn_procs_.deviceRelease(device_);
}

WGPUTexture DawnD3DImageRepresentation::BeginAccess(WGPUTextureUsage usage) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());

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

void DawnD3DImageRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());

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

OverlayD3DImageRepresentation::OverlayD3DImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gl::GLImage> gl_image)
    : OverlayImageRepresentation(manager, backing, tracker),
      gl_image_(std::move(gl_image)) {}

OverlayD3DImageRepresentation::~OverlayD3DImageRepresentation() = default;

bool OverlayD3DImageRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  // Only D3D images need keyed mutex synchronization.
  if (gl_image_->GetType() == gl::GLImage::Type::D3D)
    return static_cast<D3DImageBacking*>(backing())->BeginAccessD3D11();
  return true;
}

void OverlayD3DImageRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
  // Only D3D images need keyed mutex synchronization.
  if (gl_image_->GetType() == gl::GLImage::Type::D3D)
    static_cast<D3DImageBacking*>(backing())->EndAccessD3D11();
}

gl::GLImage* OverlayD3DImageRepresentation::GetGLImage() {
  return gl_image_.get();
}

}  // namespace gpu

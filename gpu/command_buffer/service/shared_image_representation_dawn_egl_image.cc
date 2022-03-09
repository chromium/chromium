// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_dawn_egl_image.h"

#include "build/build_config.h"
#include "gpu/command_buffer/service/texture_manager.h"
#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/shared_image_backing_d3d.h"
#endif

#include <dawn/native/OpenGLBackend.h>

namespace gpu {

SharedImageRepresentationDawnEGLImage::SharedImageRepresentationDawnEGLImage(
    std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
        gl_representation,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    const WGPUTextureDescriptor& texture_descriptor)
    : SharedImageRepresentationDawn(manager, backing, tracker),
      gl_representation_(std::move(gl_representation)),
      device_(device),
      texture_descriptor_(texture_descriptor),
      dawn_procs_(dawn::native::GetProcs()) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid.
  dawn_procs_.deviceReference(device_);
}

SharedImageRepresentationDawnEGLImage::
    ~SharedImageRepresentationDawnEGLImage() {
  EndAccess();

  dawn_procs_.deviceRelease(device_);
}

WGPUTexture SharedImageRepresentationDawnEGLImage::BeginAccess(
    WGPUTextureUsage usage) {
#if BUILDFLAG(IS_WIN)
  // On D3D11 backings, we must acquire the keyed mutex to do interop. If we
  // ever switch to non-D3D backings on Windows, this code will break horribly.
  // TODO(senorblanco): This should probably be a virtual on SharedImageBacking
  // to avoid this cast.
  static_cast<SharedImageBackingD3D*>(backing())->BeginAccessD3D11();
#endif
  dawn::native::opengl::ExternalImageDescriptorEGLImage externalImageDesc;
  externalImageDesc.cTextureDescriptor = &texture_descriptor_;
  const auto& texture = gl_representation_->GetTexturePassthrough();
  externalImageDesc.image =
      texture->GetLevelImage(texture->target(), 0u)->GetEGLImage();
  DCHECK(externalImageDesc.image);
  externalImageDesc.isInitialized = true;
  texture_ =
      dawn::native::opengl::WrapExternalEGLImage(device_, &externalImageDesc);
  return texture_;
}

void SharedImageRepresentationDawnEGLImage::EndAccess() {
  if (!texture_) {
    return;
  }
  if (dawn::native::IsTextureSubresourceInitialized(texture_, 0, 1, 0, 1)) {
    SetCleared();
  }
#if BUILDFLAG(IS_WIN)
  // TODO(senorblanco): This should probably be a virtual on SharedImageBacking
  // to avoid this cast.
  static_cast<SharedImageBackingD3D*>(backing())->EndAccessD3D11();
#endif
  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);
  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;
}

}  // namespace gpu

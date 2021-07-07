// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_dawn_egl_image.h"

#include "build/build_config.h"
#if defined(OS_WIN)
#include "gpu/command_buffer/service/shared_image_backing_d3d.h"
#endif

#include <dawn_native/OpenGLBackend.h>

namespace gpu {

SharedImageRepresentationDawnEGLImage::SharedImageRepresentationDawnEGLImage(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    EGLImage image,
    const WGPUTextureDescriptor& texture_descriptor)
    : SharedImageRepresentationDawn(manager, backing, tracker),
      device_(device),
      image_(image),
      texture_descriptor_(texture_descriptor),
      dawn_procs_(dawn_native::GetProcs()) {
  DCHECK(device_);
  DCHECK(image_);

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
#if defined(OS_WIN)
  // On D3D11 backings, we must acquire the keyed mutex to do interop. If we
  // ever switch to non-D3D backings on Windows, this code will break horribly.
  // TODO(senorblanco): This should probably be a virtual on SharedImageBacking
  // to avoid this cast.
  static_cast<SharedImageBackingD3D*>(backing())->BeginAccessD3D11();
#endif
  dawn_native::opengl::ExternalImageDescriptorEGLImage externalImageDesc;
  externalImageDesc.cTextureDescriptor = &texture_descriptor_;
  externalImageDesc.image = image_;
  externalImageDesc.isInitialized = true;
  texture_ =
      dawn_native::opengl::WrapExternalEGLImage(device_, &externalImageDesc);
  return texture_;
}

void SharedImageRepresentationDawnEGLImage::EndAccess() {
  if (!texture_) {
    return;
  }
  if (dawn_native::IsTextureSubresourceInitialized(texture_, 0, 1, 0, 1)) {
    SetCleared();
  }
#if defined(OS_WIN)
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

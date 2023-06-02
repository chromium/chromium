// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_egl_image_representation.h"

#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"

#include <dawn/native/OpenGLBackend.h>

namespace {
GLenum ToSharedImageAccessGLMode(WGPUTextureUsage usage) {
  if (usage & (WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment |
               WGPUTextureUsage_StorageBinding)) {
    return GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  } else {
    return GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM;
  }
}
}  // namespace

namespace gpu {

DawnEGLImageRepresentation::DawnEGLImageRepresentation(
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
    void* egl_image,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device)
    : DawnImageRepresentation(manager, backing, tracker),
      gl_representation_(std::move(gl_representation)),
      egl_image_(std::move(egl_image)),
      device_(device),
      dawn_procs_(dawn::native::GetProcs()) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid.
  dawn_procs_.deviceReference(device_);
}

DawnEGLImageRepresentation::~DawnEGLImageRepresentation() {
  EndAccess();

  dawn_procs_.deviceRelease(device_);
}

WGPUTexture DawnEGLImageRepresentation::BeginAccess(WGPUTextureUsage usage) {
  gl_representation_->BeginAccess(ToSharedImageAccessGLMode(usage));
  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.nextInChain = nullptr;
  texture_descriptor.format = ToWGPUFormat(format());
  texture_descriptor.usage = WGPUTextureUsage_CopySrc |
                             WGPUTextureUsage_CopyDst |
                             WGPUTextureUsage_RenderAttachment;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  dawn::native::opengl::ExternalImageDescriptorEGLImage externalImageDesc;
  externalImageDesc.cTextureDescriptor = &texture_descriptor;
  externalImageDesc.image = egl_image_;
  DCHECK(externalImageDesc.image);
  externalImageDesc.isInitialized = true;
  texture_ =
      dawn::native::opengl::WrapExternalEGLImage(device_, &externalImageDesc);
  return texture_;
}

void DawnEGLImageRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }
  if (dawn::native::IsTextureSubresourceInitialized(texture_, 0, 1, 0, 1)) {
    SetCleared();
  }
  gl_representation_->EndAccess();
  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);
  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;
}

}  // namespace gpu

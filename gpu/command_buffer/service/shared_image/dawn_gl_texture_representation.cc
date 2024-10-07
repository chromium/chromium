// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_gl_texture_representation.h"

#include <dawn/native/OpenGLBackend.h>

#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_finch_features.h"

namespace {
GLenum ToSharedImageAccessGLMode(wgpu::TextureUsage usage) {
  if (usage &
      (wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::RenderAttachment |
       wgpu::TextureUsage::StorageBinding)) {
    return GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  } else {
    return GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM;
  }
}
}  // namespace

namespace gpu {

DawnGLTextureRepresentation::DawnGLTextureRepresentation(
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    wgpu::Device device)
    : DawnImageRepresentation(manager, backing, tracker),
      gl_representation_(std::move(gl_representation)),
      device_(device) {
  DCHECK(device_);
}

DawnGLTextureRepresentation::~DawnGLTextureRepresentation() {
  EndAccess();
}

wgpu::Texture DawnGLTextureRepresentation::BeginAccess(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage) {
  auto usage_to_check = usage;
  usage_to_check |= internal_usage;
  gl_representation_->BeginAccess(ToSharedImageAccessGLMode(usage_to_check));
  wgpu::TextureDescriptor texture_descriptor = {};
  texture_descriptor.nextInChain = nullptr;

  // TODO(crbug.com/40278761): implement support for multiplanar formats.
  texture_descriptor.format = ToDawnFormat(format());

  wgpu::DawnTextureInternalUsageDescriptor internalDesc;
  internalDesc.internalUsage = internal_usage;

  texture_descriptor.nextInChain = &internalDesc;

  texture_descriptor.usage = usage;
  texture_descriptor.dimension = wgpu::TextureDimension::e2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;

  // TODO(crbug.com/40897964): once the forceReadback path is removed, determine
  // the correct set of internal usages to apply and add
  // DawnTextureInternalUsageDescriptor to the descriptor chain.

  dawn::native::opengl::ExternalImageDescriptorGLTexture externalImageDesc;
  externalImageDesc.cTextureDescriptor =
      reinterpret_cast<WGPUTextureDescriptor*>(&texture_descriptor);
  externalImageDesc.texture =
      gl_representation_->GetTextureBase()->service_id();
  externalImageDesc.isInitialized = true;
  texture_ = wgpu::Texture::Acquire(dawn::native::opengl::WrapExternalGLTexture(
      device_.Get(), &externalImageDesc));
  return texture_;
}

void DawnGLTextureRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }
  if (dawn::native::IsTextureSubresourceInitialized(texture_.Get(), 0, 1, 0,
                                                    1)) {
    SetCleared();
  }
  gl_representation_->EndAccess();
  // All further operations on the textures are errors (they would be racy
  // with other backings).
  texture_.Destroy();
  texture_ = nullptr;
}

}  // namespace gpu

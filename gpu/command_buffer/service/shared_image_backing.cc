// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing.h"

#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

SharedImageBacking::SharedImageBacking(const Mailbox& mailbox,
                                       viz::ResourceFormat format,
                                       const gfx::Size& size,
                                       const gfx::ColorSpace& color_space,
                                       uint32_t usage)
    : mailbox_(mailbox),
      format_(format),
      size_(size),
      color_space_(color_space),
      usage_(usage) {}

SharedImageBacking::~SharedImageBacking() = default;

size_t SharedImageBacking::EstimatedSize() const {
  return 0;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBacking::ProduceGLTexture(SharedImageManager* manager) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationSkia> SharedImageBacking::ProduceSkia(
    SharedImageManager* manager) {
  return nullptr;
}

}  // namespace gpu

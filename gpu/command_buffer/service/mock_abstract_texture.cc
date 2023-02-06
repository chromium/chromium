// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mock_abstract_texture.h"

namespace gpu {

MockAbstractTexture::MockAbstractTexture() = default;

MockAbstractTexture::MockAbstractTexture(GLuint service_id)
    : texture_base_(std::make_unique<gpu::TextureBase>(service_id)) {}

MockAbstractTexture::~MockAbstractTexture() = default;

gpu::TextureBase* MockAbstractTexture::GetTextureBase() const {
  return texture_base_.get();
}

}  // namespace gpu

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mock_abstract_texture.h"

namespace gpu {

MockAbstractTexture::MockAbstractTexture() = default;

MockAbstractTexture::MockAbstractTexture(GLuint service_id)
    : texture_base_(std::make_unique<gpu::TextureBase>(service_id)) {
  ON_CALL(*this, GetTextureBase())
      .WillByDefault(::testing::Return(texture_base_.get()));
}

MockAbstractTexture::~MockAbstractTexture() = default;

}  // namespace gpu

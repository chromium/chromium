// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mock_texture_owner.h"

#include "gpu/command_buffer/service/mock_abstract_texture.h"

namespace gpu {

using testing::Invoke;
using testing::Return;

MockTextureOwner::MockTextureOwner(GLuint fake_texture_id,
                                   gl::GLContext* fake_context,
                                   gl::GLSurface* fake_surface,
                                   bool binds_texture_on_update)
    : TextureOwner(binds_texture_on_update,
                   std::make_unique<MockAbstractTexture>(fake_texture_id)),
      fake_context(fake_context),
      fake_surface(fake_surface),
      expect_update_tex_image(!binds_texture_on_update) {
  ON_CALL(*this, GetTextureId()).WillByDefault(Return(fake_texture_id));
  ON_CALL(*this, GetContext()).WillByDefault(Return(fake_context));
  ON_CALL(*this, GetSurface()).WillByDefault(Return(fake_surface));
  ON_CALL(*this, EnsureTexImageBound()).WillByDefault(Invoke([this] {
    CHECK(expect_update_tex_image);
  }));
}

MockTextureOwner::~MockTextureOwner() {
  // TextureOwner requires this.
  ClearAbstractTexture();
}

}  // namespace gpu

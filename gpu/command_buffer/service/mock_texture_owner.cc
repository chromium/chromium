// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mock_texture_owner.h"

#include "gpu/command_buffer/service/abstract_texture_android.h"

namespace gpu {

using testing::_;
using testing::Invoke;
using testing::Return;

MockTextureOwner::MockTextureOwner(GLuint fake_texture_id,
                                   gl::GLContext* fake_context,
                                   gl::GLSurface* fake_surface,
                                   bool binds_texture_on_update)
    : TextureOwner(binds_texture_on_update,
                   AbstractTextureAndroid::CreateForTesting(fake_texture_id)),
      fake_context(fake_context),
      fake_surface(fake_surface) {
  ON_CALL(*this, GetTextureId()).WillByDefault(Return(fake_texture_id));
  ON_CALL(*this, GetContext()).WillByDefault(Return(fake_context));
  ON_CALL(*this, GetSurface()).WillByDefault(Return(fake_surface));
  ON_CALL(*this, RunWhenBufferIsAvailable(_))
      .WillByDefault(Invoke([](base::OnceClosure cb) { std::move(cb).Run(); }));
}

MockTextureOwner::~MockTextureOwner() {}

}  // namespace gpu

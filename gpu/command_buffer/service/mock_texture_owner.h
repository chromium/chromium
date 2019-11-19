// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MOCK_TEXTURE_OWNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_MOCK_TEXTURE_OWNER_H_

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

using testing::NiceMock;

namespace gpu {

// This is a mock with a small amount of fake functionality too.
class MockTextureOwner : public TextureOwner {
 public:
  MockTextureOwner(GLuint fake_texture_id,
                   gl::GLContext* fake_context,
                   gl::GLSurface* fake_surface,
                   bool binds_texture_on_update = false);

  MOCK_CONST_METHOD0(GetTextureId, GLuint());
  MOCK_CONST_METHOD0(GetContext, gl::GLContext*());
  MOCK_CONST_METHOD0(GetSurface, gl::GLSurface*());
  MOCK_CONST_METHOD0(CreateJavaSurface, gl::ScopedJavaSurface());
  MOCK_METHOD0(UpdateTexImage, void());
  MOCK_METHOD0(EnsureTexImageBound, void());
  MOCK_METHOD1(GetTransformMatrix, void(float mtx[16]));
  MOCK_METHOD0(ReleaseBackBuffers, void());
  MOCK_METHOD1(OnTextureDestroyed, void(gpu::gles2::AbstractTexture*));
  MOCK_METHOD1(SetFrameAvailableCallback, void(const base::RepeatingClosure&));

  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override {
    get_a_hardware_buffer_count++;
    return nullptr;
  }

  gl::GLContext* fake_context;
  gl::GLSurface* fake_surface;
  int get_a_hardware_buffer_count = 0;
  bool expect_update_tex_image;

 protected:
  ~MockTextureOwner();
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MOCK_TEXTURE_OWNER_H_

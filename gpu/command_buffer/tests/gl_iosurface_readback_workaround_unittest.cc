// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "build/build_config.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

#if defined(OS_MAC)
// A test that exercises the glReadPixels workaround for IOSurface backed
// textures.
class GLIOSurfaceReadbackWorkaroundTest : public testing::Test {
 public:
  GLIOSurfaceReadbackWorkaroundTest() {}

 protected:
  void SetUp() override {
    gl_.Initialize(GLManager::Options());
    gl_.set_use_iosurface_memory_buffers(true);
  }

  void TearDown() override {
    GLTestHelper::CheckGLError("no errors", __LINE__);
    gl_.Destroy();
  }

  GLManager gl_;
};

TEST_F(GLIOSurfaceReadbackWorkaroundTest, ReadPixels) {
  int width = 1;
  int height = 1;
  GLuint source_texture = 0;
  GLenum source_target = GL_TEXTURE_RECTANGLE_ARB;
  glGenTextures(1, &source_texture);
  glBindTexture(source_target, source_texture);
  glTexParameteri(source_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(source_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(gl_.CreateGpuMemoryBuffer(
      gfx::Size(width, height), gfx::BufferFormat::RGBA_8888));
  GLuint image_id =
      glCreateImageCHROMIUM(buffer->AsClientBuffer(), width, height, GL_RGBA);
  ASSERT_NE(0u, image_id);
  glBindTexImage2DCHROMIUM(source_target, image_id);

  GLuint framebuffer = 0;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, source_target, source_texture, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  glClearColor(33.0 / 255.0, 44.0 / 255.0, 55.0 / 255.0, 66.0 / 255.0);
  glClear(GL_COLOR_BUFFER_BIT);
  const uint8_t expected[4] = {33, 44, 55, 66};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 1 /* tolerance */, expected,
                                        nullptr));

  glClearColor(14.0 / 255.0, 15.0 / 255.0, 16.0 / 255.0, 17.0 / 255.0);
  glClear(GL_COLOR_BUFFER_BIT);
  const uint8_t expected2[4] = {14, 15, 16, 17};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 1 /* tolerance */,
                                        expected2, nullptr));

  glReleaseTexImage2DCHROMIUM(source_target, image_id);
  glDestroyImageCHROMIUM(image_id);
  glDeleteTextures(1, &source_texture);
  glDeleteFramebuffers(1, &framebuffer);
}

#endif  // defined(OS_MAC)

}  // namespace gpu

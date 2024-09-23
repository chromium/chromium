// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <GLES2/gl2.h>
#include <stdint.h>

#include <memory>

#include "base/containers/heap_array.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {
const GLenum kCubeMapTextureTargets[] = {
    GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};
}  // namespace

// A collection of tests that exercise the cube map texture.
class GLCubeMapTextureTest : public testing::TestWithParam<GLenum> {
 protected:
  void SetUp() override {
    // ANGLE and NVidia fails ReadPixelsFromIncompleteCubeTexture without this
    // workaround.
    GpuDriverBugWorkarounds workarounds;
    workarounds.force_cube_complete = true;
    gl_.InitializeWithWorkarounds(GLManager::Options(), workarounds);
    DCHECK(gl_.workarounds().force_cube_complete);
    for (int i = 0; i < 256; i++) {
      pixels_[i * 4] = 255u;
      pixels_[(i * 4) + 1] = 0;
      pixels_[(i * 4) + 2] = 0;
      pixels_[(i * 4) + 3] = 255u;
    }

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    glGenFramebuffers(1, &framebuffer_id_);
  }

  void TearDown() override {
    glDeleteTextures(1, &texture_);
    glDeleteFramebuffers(1, &framebuffer_id_);
    gl_.Destroy();
  }

  GLManager gl_;
  uint8_t pixels_[256 * 4];
  const int width_ = 16;
  GLuint texture_;
  GLuint framebuffer_id_;
};

INSTANTIATE_TEST_SUITE_P(GLCubeMapTextureTests,
                         GLCubeMapTextureTest,
                         ::testing::ValuesIn(kCubeMapTextureTargets));

TEST_P(GLCubeMapTextureTest, TexImage2DAfterFBOBinding) {
  GLenum cube_map_target = GetParam();

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_map_target,
                         texture_, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
  // force_cube_map_positive_x_allocation workaround prevents Nexus 5 crash.
  // TODO(dshwang): remove the workaround when it's fixed. crbug.com/518889
  glTexImage2D(cube_map_target, 0, GL_RGBA, width_, width_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels_);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

// TODO(crbug.com/40246425): Re-enable this test
TEST_P(GLCubeMapTextureTest, DISABLED_ReadPixels) {
  GLenum cube_map_target = GetParam();

  glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Make a cube texture complete
  for (unsigned i = 0; i < std::size(kCubeMapTextureTargets); i++) {
    glTexImage2D(kCubeMapTextureTargets[i], 0, GL_RGBA, width_, width_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels_);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_map_target,
                         texture_, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Check that FB is complete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  GLTestHelper::CheckPixels(0, 0, width_, width_, 0, pixels_, nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

// TODO(crbug.com/40246425): Re-enable this test
TEST_P(GLCubeMapTextureTest, DISABLED_ReadPixelsFromIncompleteCubeTexture) {
  GLenum cube_map_target = GetParam();

  glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  glTexImage2D(cube_map_target, 0, GL_RGBA, width_, width_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels_);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_map_target,
                         texture_, 0);

  // force_cube_map_positive_x_allocation workaround prevents Nexus 5 crash.
  // TODO(dshwang): remove the workaround when it's fixed. crbug.com/518889
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Check that FB is not complete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));
  GLsizei size = width_ * width_ * 4;
  auto pixels = base::HeapArray<uint8_t>::Uninit(size);
  glReadPixels(0, 0, width_, width_, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_FRAMEBUFFER_OPERATION),
            glGetError());
}

}  // namespace gpu

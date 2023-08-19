// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdint.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

// A collection of tests that exercise the GL_EXT_srgb extension.
class GLBGRAMipMapTest : public testing::Test {
 protected:
  void SetUp() override { gl_.Initialize(GLManager::Options()); }
  void TearDown() override { gl_.Destroy(); }
  bool ShouldSkipBGRA() const {
    return !gl_.decoder()
                ->GetFeatureInfo()
                ->feature_flags()
                .ext_texture_format_bgra8888;
  }
  GLManager gl_;
};

// Test to ensure that using GL_BGRA as a texture internal format does
// not hinder the use of mipmaps. Support for GL_BGRA as an internal format
// is required by ES 2.0 (internal format must be equal to external format),
// but some desktop GL implementations may not fully support the use of
// GL_BGRA. For example, Mesa+Intel does not support mipmapping on textures
// that use the GL_BGRA internal format. This test verifies a workaround.
TEST_F(GLBGRAMipMapTest, GenerateMipmapsSucceeds) {
  if (ShouldSkipBGRA()) {
    return;
  }

  static const int kWidth = 100;
  static const int kHeight = 50;

  uint8_t pixels[kWidth * kHeight * 4];

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  memset(pixels, 128, sizeof(pixels));
  glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, kWidth, kHeight, 0,
               GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Without the workaround, the following call generates a
  // GL_INVALID_OPERATION error on some desktop GL implementations
  glGenerateMipmap(GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

}  // namespace gpu

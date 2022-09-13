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
class GLEXTSRGBTest : public testing::Test {
 protected:
  void SetUp() override { gl_.Initialize(GLManager::Options()); }
  void TearDown() override { gl_.Destroy(); }
  bool IsApplicable() const {
    return GLTestHelper::HasExtension("GL_EXT_sRGB");
  }
  GLManager gl_;
};

// Test to ensure that GL_SRGB_ALPHA as glTex(Sub)Image2D parameter works. This
// is tricky because GL_SRGB_ALPHA is valid in OpenGL ES 2.0 but not valid in
// OpenGL.
TEST_F(GLEXTSRGBTest, TexImageSRGBALPHAFormat) {
  if (!IsApplicable())
    return;
  static const int kWidth = 10;
  static const int kHeight = 10;
  static const int kSubImageX = kWidth / 2;
  static const int kSubImageY = kHeight / 2;
  static const int kSubImageWidth = kWidth / 2;
  static const int kSubImageHeight = kHeight / 2;
  static const uint8_t kImageColor[] = {255, 255, 255, 255};
  static const uint8_t kSubImageColor[] = {128, 128, 128, 128};

  uint8_t pixels[kWidth * kHeight * 4];

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  memset(pixels, kImageColor[0], sizeof(pixels));
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA_EXT, kWidth, kHeight, 0,
               GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE, pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  memset(pixels, kSubImageColor[0], sizeof(pixels));
  glTexSubImage2D(GL_TEXTURE_2D, 0, kSubImageX, kSubImageY, kSubImageWidth,
                  kSubImageHeight, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE, pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glBindTexture(GL_TEXTURE_2D, 0);
  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  GLTestHelper::CheckPixels(0, 0, kSubImageX, kHeight, 0, kImageColor, nullptr);
  GLTestHelper::CheckPixels(0, 0, kWidth, kSubImageY, 0, kImageColor, nullptr);
  GLTestHelper::CheckPixels(kSubImageX, kSubImageY, kSubImageWidth,
                            kSubImageHeight, 0, kSubImageColor, nullptr);
}

}  // namespace gpu

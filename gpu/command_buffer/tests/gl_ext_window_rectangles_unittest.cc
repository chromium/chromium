// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

// A collection of tests that exercise the GL_EXT_window_rectangles extension.
class GLEXTWindowRectanglesTest : public testing::Test {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    gl_.Initialize(options);
  }

  void TearDown() override { gl_.Destroy(); }

  bool IsApplicable() const {
    // Not applicable for devices not supporting OpenGLES3.
    if (!gl_.IsInitialized())
      return false;
    bool have_ext = GLTestHelper::HasExtension("GL_EXT_window_rectangles");
    return have_ext;
  }

  GLuint SetupFramebuffer() {
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE), status);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    return fbo;
  }

  void ClearTo(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }

  GLManager gl_;
};

#define CHECK_PIXEL(x, y, r, g, b, a)                               \
  do {                                                              \
    uint8_t pixel[4];                                               \
    glReadPixels((x), (y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel); \
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());      \
    EXPECT_NEAR((r), pixel[0], 2);                                  \
    EXPECT_NEAR((g), pixel[1], 2);                                  \
    EXPECT_NEAR((b), pixel[2], 2);                                  \
    EXPECT_NEAR((a), pixel[3], 2);                                  \
  } while (0)

// TODO(crbug.com/40246425): Re-enable this test
TEST_F(GLEXTWindowRectanglesTest, DISABLED_Defaults) {
  if (!IsApplicable()) {
    return;
  }

  GLint max = -1;
  {
    glGetIntegerv(GL_MAX_WINDOW_RECTANGLES_EXT, &max);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_GE(max, 4);
  }

  {
    GLint mode = -1;
    glGetIntegerv(GL_WINDOW_RECTANGLE_MODE_EXT, &mode);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_EQ(GL_EXCLUSIVE_EXT, mode);
  }
  {
    GLint num = -1;
    glGetIntegerv(GL_NUM_WINDOW_RECTANGLES_EXT, &num);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_EQ(0, num);
  }

  for (int i = 0; i < max; ++i) {
    GLint rect[4] = {-1, -1, -1, -1};
    glGetIntegeri_v(GL_WINDOW_RECTANGLE_EXT, i, rect);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_EQ(0, rect[0]);
    EXPECT_EQ(0, rect[1]);
    EXPECT_EQ(0, rect[2]);
    EXPECT_EQ(0, rect[3]);
  }
}

TEST_F(GLEXTWindowRectanglesTest, Defaults64) {
  if (!IsApplicable()) {
    return;
  }

  int64_t max = -1;
  {
    glGetInteger64v(GL_MAX_WINDOW_RECTANGLES_EXT, &max);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_GE(max, 4);
  }

  {
    int64_t mode = -1;
    glGetInteger64v(GL_WINDOW_RECTANGLE_MODE_EXT, &mode);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_EQ(GL_EXCLUSIVE_EXT, mode);
  }
  {
    int64_t num = -1;
    glGetInteger64v(GL_NUM_WINDOW_RECTANGLES_EXT, &num);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_EQ(0, num);
  }

  for (int i = 0; i < max; ++i) {
    int64_t rect[4] = {-1, -1, -1, -1};
    glGetInteger64i_v(GL_WINDOW_RECTANGLE_EXT, i, rect);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_EQ(0, rect[0]);
    EXPECT_EQ(0, rect[1]);
    EXPECT_EQ(0, rect[2]);
    EXPECT_EQ(0, rect[3]);
  }
}

TEST_F(GLEXTWindowRectanglesTest, BasicState) {
  if (!IsApplicable()) {
    return;
  }

  GLint box_exp[12] = {};
  for (int i = 0; i < 12; ++i) {
    box_exp[i] = i;
  }
  glWindowRectanglesEXT(GL_INCLUSIVE_EXT, 3, box_exp);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  {
    GLint mode = -1;
    glGetIntegerv(GL_WINDOW_RECTANGLE_MODE_EXT, &mode);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_EQ(GL_INCLUSIVE_EXT, mode);
  }
  {
    GLint num = -1;
    glGetIntegerv(GL_NUM_WINDOW_RECTANGLES_EXT, &num);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    EXPECT_EQ(3, num);
  }
  {
    GLint box[12] = {};
    for (int i = 0; i < 12; ++i) {
      box[i] = -1;
    }
    glGetIntegeri_v(GL_WINDOW_RECTANGLE_EXT, 0, &box[0]);
    glGetIntegeri_v(GL_WINDOW_RECTANGLE_EXT, 1, &box[4]);
    glGetIntegeri_v(GL_WINDOW_RECTANGLE_EXT, 2, &box[8]);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    for (int i = 0; i < 12; ++i) {
      EXPECT_EQ(box_exp[i], box[i]);
    }
  }
}

TEST_F(GLEXTWindowRectanglesTest, DefaultFramebuffer) {
  if (!IsApplicable()) {
    return;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  GLint box[4] = {0, 0, 1, 1};

  ClearTo(255, 0, 0, 255);
  // This says "DO NOT render inside 0,0", but
  // it should have no effect for the default framebuffer.
  glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 1, box);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  ClearTo(0, 255, 0, 255);
  // Inside rectangle
  CHECK_PIXEL(0, 0, 0, 255, 0, 255);

  ClearTo(255, 0, 0, 255);
  // This says "ONLY render inside 0,0", but
  // it should have no effect for the default framebuffer.
  glWindowRectanglesEXT(GL_INCLUSIVE_EXT, 1, box);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  ClearTo(0, 255, 0, 255);
  // Inside rectangle
  CHECK_PIXEL(0, 0, 0, 255, 0, 255);
}

TEST_F(GLEXTWindowRectanglesTest, OneRectangle) {
  if (!IsApplicable()) {
    return;
  }

  GLint box[4] = {
      8, 4, 1, 1,
  };
  SetupFramebuffer();

  ClearTo(255, 0, 0, 255);
  // "DO NOT render inside the pixel 8,4"
  glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 1, box);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  ClearTo(0, 255, 0, 255);
  // Inside rectangle
  CHECK_PIXEL(8, 4, 255, 0, 0, 255);
  // Outside rectangle
  CHECK_PIXEL(4, 8, 0, 255, 0, 255);

  glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr);
  ClearTo(255, 0, 0, 255);
  // "ONLY render inside the pixel 8,4"
  glWindowRectanglesEXT(GL_INCLUSIVE_EXT, 1, box);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  ClearTo(0, 255, 0, 255);
  // Inside rectangle
  CHECK_PIXEL(8, 4, 0, 255, 0, 255);
  // Outside rectangle
  CHECK_PIXEL(4, 8, 255, 0, 0, 255);
}

TEST_F(GLEXTWindowRectanglesTest, TwoRectangles) {
  if (!IsApplicable()) {
    return;
  }

  GLint box[8] = {
      8, 4, 1, 1, 2, 5, 1, 1,
  };
  SetupFramebuffer();

  ClearTo(255, 0, 0, 255);
  // "DO NOT render inside the pixels 8,4 and 2,5"
  glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 2, box);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  ClearTo(0, 255, 0, 255);
  // Inside rectangles
  CHECK_PIXEL(8, 4, 255, 0, 0, 255);
  CHECK_PIXEL(2, 5, 255, 0, 0, 255);
  // Outside rectangles
  CHECK_PIXEL(4, 8, 0, 255, 0, 255);
  CHECK_PIXEL(5, 2, 0, 255, 0, 255);

  glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr);
  ClearTo(255, 0, 0, 255);
  // "ONLY render inside the pixels 8,4 and 4,8"
  glWindowRectanglesEXT(GL_INCLUSIVE_EXT, 2, box);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  ClearTo(0, 255, 0, 255);
  // Inside rectangles
  CHECK_PIXEL(8, 4, 0, 255, 0, 255);
  CHECK_PIXEL(2, 5, 0, 255, 0, 255);
  // Outside rectangles
  CHECK_PIXEL(4, 8, 255, 0, 0, 255);
  CHECK_PIXEL(5, 2, 255, 0, 0, 255);
}

TEST_F(GLEXTWindowRectanglesTest, InclusiveEmptyBox) {
  if (!IsApplicable()) {
    return;
  }

  GLint box[4] = {
      0, 0, 0, 0,
  };
  SetupFramebuffer();

  ClearTo(255, 0, 0, 255);
  // "ONLY render inside the 0-by-0 box"
  glWindowRectanglesEXT(GL_INCLUSIVE_EXT, 1, box);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  ClearTo(0, 255, 0, 255);
  // Outside rectangle
  CHECK_PIXEL(4, 8, 255, 0, 0, 255);
}

TEST_F(GLEXTWindowRectanglesTest, TextureClearStateClearAndRestore) {
  if (!IsApplicable()) {
    return;
  }

  GLint box[4] = {
      0, 0, 0, 0,
  };
  GLuint main_fbo = SetupFramebuffer();

  ClearTo(255, 0, 0, 255);
  // "ONLY render inside the 0-by-0 box"
  glWindowRectanglesEXT(GL_INCLUSIVE_EXT, 1, box);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  GLuint test_fbo = 0;
  {
    glGenFramebuffers(1, &test_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, test_fbo);
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE), status);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }

  glBindFramebuffer(GL_FRAMEBUFFER, test_fbo);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  // Check the texture is initialized
  {
    constexpr size_t PIXELS_SIZE = 4 * 32 * 32;
    uint8_t pixels[PIXELS_SIZE];
    glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    for (size_t i = 0; i < PIXELS_SIZE; ++i) {
      EXPECT_NEAR(0, pixels[i], 2);
    }
  }

  glBindFramebuffer(GL_FRAMEBUFFER, main_fbo);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  // Make sure the window rectangle state is preserved
  ClearTo(0, 255, 0, 255);
  // So this clear doesn't do anything.
  CHECK_PIXEL(4, 8, 255, 0, 0, 255);
}

}  // namespace gpu

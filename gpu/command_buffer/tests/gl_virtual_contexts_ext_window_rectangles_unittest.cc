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

// A collection of tests that exercise the GL_EXT_srgb extension.
class GLVirtualContextsEXTWindowRectanglesTest : public testing::Test {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    gl_real_shared_.Initialize(options);

    if (!IsApplicable())
      return;

    options.virtual_manager = &gl_real_shared_;
    gl1_.Initialize(options);
    gl2_.Initialize(options);
  }

  void TearDown() override {
    if (IsApplicable()) {
      gl1_.Destroy();
      gl2_.Destroy();
    }
    gl_real_shared_.Destroy();
  }

  bool IsApplicable() const {
    // Not applicable for devices not supporting OpenGLES3.
    if (!gl_real_shared_.IsInitialized()) {
      return false;
    }

    bool have_ext = GLTestHelper::HasExtension("GL_EXT_window_rectangles");
    return have_ext;
  }

  GLManager gl_real_shared_;
  GLManager gl1_;
  GLManager gl2_;
};

TEST_F(GLVirtualContextsEXTWindowRectanglesTest, Basic) {
  if (!IsApplicable()) {
    return;
  }

  // Context 1: Set window rectangles state.
  gl1_.MakeCurrent();
  {
    GLint box[12] = {};
    for (int i = 0; i < 12; ++i) {
      box[i] = i;
    }
    glWindowRectanglesEXT(GL_INCLUSIVE_EXT, 3, box);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }

  // Context 2: Make sure it still has the default state.
  gl2_.MakeCurrent();
  {
    GLint max = -1;
    {
      glGetIntegerv(GL_MAX_WINDOW_RECTANGLES_EXT, &max);
      EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
      EXPECT_GE(max, 4);
    }

    {
      int mode = -1;
      glGetIntegerv(GL_WINDOW_RECTANGLE_MODE_EXT, &mode);
      EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
      EXPECT_EQ(GL_EXCLUSIVE_EXT, mode);
    }
    {
      GLint num = -1;
      glGetIntegerv(GL_NUM_WINDOW_RECTANGLES_EXT, &num);
      EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
      EXPECT_EQ(0, num);
    }

    for (int i = 0; i < max; ++i) {
      GLint rect[4] = {-1, -1, -1, -1};
      glGetIntegeri_v(GL_WINDOW_RECTANGLE_EXT, i, rect);
      EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
      EXPECT_EQ(0, rect[0]);
      EXPECT_EQ(0, rect[1]);
      EXPECT_EQ(0, rect[2]);
      EXPECT_EQ(0, rect[3]);
    }
  }

  // Context 1: Make sure it still has the state it set.
  gl1_.MakeCurrent();
  {
    {
      int mode = -1;
      glGetIntegerv(GL_WINDOW_RECTANGLE_MODE_EXT, &mode);
      EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
      EXPECT_EQ(GL_INCLUSIVE_EXT, mode);
    }
    {
      GLint num = -1;
      glGetIntegerv(GL_NUM_WINDOW_RECTANGLES_EXT, &num);
      EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
      EXPECT_EQ(3, num);
    }

    for (int i = 0; i < 3; ++i) {
      GLint rect[4] = {-1, -1, -1, -1};
      glGetIntegeri_v(GL_WINDOW_RECTANGLE_EXT, i, rect);
      EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
      EXPECT_EQ(4 * i + 0, rect[0]);
      EXPECT_EQ(4 * i + 1, rect[1]);
      EXPECT_EQ(4 * i + 2, rect[2]);
      EXPECT_EQ(4 * i + 3, rect[3]);
    }
  }
}

}  // namespace gpu

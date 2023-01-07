// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLDescheduleTest : public testing::Test {
 protected:
  void SetUp() override {
    GLManager::Options options;
    gl1_.Initialize(options);
  }

  void TearDown() override {
    gl1_.Destroy();
  }

  GLManager gl1_;
};

TEST_F(GLDescheduleTest, Deschedule) {
  gl1_.MakeCurrent();

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);

  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex, 0);

  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDescheduleUntilFinishedCHROMIUM();
  glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glFlush();
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glFlush();

  uint32_t result;
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
               &result);
  EXPECT_EQ(0xFF00FF00u, result);
}

}  // namespace gpu

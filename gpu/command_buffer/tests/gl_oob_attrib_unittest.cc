// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdint.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

class GLOOBAttribTest : public testing::Test {
 protected:
  void SetUp() override {
    if (GPUTestBotConfig::CurrentConfigMatches("Android ARM 0x92020010")) {
      // TODO(crbug.com/40160681): remove suppression when passthrough ships.
      // Crashes on Pixel 6 validating
      GTEST_SKIP();
    }
    gl_.Initialize(GLManager::Options());
  }
  void TearDown() override { gl_.Destroy(); }
  GLManager gl_;
};

// Tests that enabling a vertex array for a location that matches any column of
// a matrix attribute correctly triggers out-of-bounds checks.
TEST_F(GLOOBAttribTest, DrawUsingOOBMatrixAttrib) {
  // The passthrough command decoder uses robust buffer access behaviour. This
  // makes OOB error checks unimportant because OOB accesses will not cause
  // errors. OOB accesses will also return implementation-dependent values.
  // See the KHR_robust_buffer_access_behavior spec for more information.
  if (gl_.gpu_preferences().use_passthrough_cmd_decoder) {
    std::cout << "Test skipped, KHR_robust_buffer_access_behavior enabled.\n";
    return;
  }

  const char kVertexShader[] =
      "attribute mat3 attrib;\n"
      "varying vec4 color;\n"
      "void main () {\n"
      "  color = vec4(1.0,\n"
      "      attrib[0][0] + attrib[0][1] + attrib[0][2] +\n"
      "      attrib[1][0] + attrib[1][1] + attrib[1][2] +\n"
      "      attrib[2][0] + attrib[2][1] + attrib[2][2],\n"
      "      1.0,\n"
      "      1.0);\n"
      "}\n";
  const char kFragmentShader[] =
      "precision mediump float;\n"
      "varying vec4 color;\n"
      "void main() {\n"
      "  gl_FragColor = color;\n"
      "}\n";

  GLuint program = GLTestHelper::LoadProgram(kVertexShader, kFragmentShader);
  DCHECK(program);
  glUseProgram(program);
  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);
  GLint location = glGetAttribLocation(program, "attrib");
  EXPECT_GE(0, location);

  // All attribs disabled - no error.
  glDrawArrays(GL_TRIANGLES, 0, 1000);
  GLenum expected = GL_NO_ERROR;
  EXPECT_EQ(expected, glGetError());

  for (int i = 0; i < 3; ++i) {
    // Enable any of the valid locations for the attribute, should raise an
    // error if trying to access attributes out-of-bounds.
    glVertexAttribPointer(location + i, 4, GL_UNSIGNED_BYTE, false, 0, nullptr);
    glEnableVertexAttribArray(location + i);
    glDrawArrays(GL_TRIANGLES, 0, 1000);
    expected = GL_INVALID_OPERATION;
    EXPECT_EQ(expected, glGetError());

    // But in-bounds should pass.
    glDrawArrays(GL_TRIANGLES, 0, 3);
    expected = GL_NO_ERROR;
    EXPECT_EQ(expected, glGetError());
    glDisableVertexAttribArray(location + i);
  }

  // Enable an unused location, should not trigger out-of-bounds checks.
  glVertexAttribPointer(location + 3, 4, GL_UNSIGNED_BYTE, false, 0, nullptr);
  glEnableVertexAttribArray(location + 3);
  glDrawArrays(GL_TRIANGLES, 0, 1000);
  expected = GL_NO_ERROR;
  EXPECT_EQ(expected, glGetError());
}

}  // anonymous namespace

}  // namespace gpu

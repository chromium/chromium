// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include <array>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER(Src) #Src

namespace gpu {

class GetBufferSubDataTest : public testing::Test {
 protected:
  static const GLsizei kCanvasSize = 4;

  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kCanvasSize, kCanvasSize);
    options.context_type = CONTEXT_TYPE_OPENGLES3;

    gl_.Initialize(options);
  }

  bool ShouldSkipTest() const {
    // If a driver isn't capable of supporting ES3 context, creating
    // ContextGroup will fail.
    // See crbug.com/654709.
    return (!gl_.decoder() || !gl_.decoder()->GetContextGroup());
  }

  void TearDown() override { gl_.Destroy(); }

  GLuint SetupSimpleProgram(GLint* ret_position_loc) {
    static const char* v_shader_src =
        SHADER(attribute vec2 a_position;
               void main() { gl_Position = vec4(a_position, 0.0, 1.0); });
    static const char* f_shader_src =
        SHADER(precision mediump float;
               void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); });
    GLuint program = GLTestHelper::LoadProgram(v_shader_src, f_shader_src);
    EXPECT_LT(0u, program);
    glUseProgram(program);
    *ret_position_loc = glGetAttribLocation(program, "a_position");
    EXPECT_LE(0, *ret_position_loc);
    return program;
  }

  GLuint SetupTransformFeedbackProgram(GLint* ret_position_loc) {
    static const char* v_shader_src =
        "#version 300 es\n"
        "in vec2 a_position;\n"
        "out vec3 var0;\n"
        "out vec2 var1;\n"
        "void main() {\n"
        "  var0 = vec3(a_position, 0.5);\n"
        "  var1 = a_position;\n"
        "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "}";
    static const char* f_shader_src =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 color;\n"
        "in vec3 var0;\n"
        "in vec2 var1;\n"
        "void main() {\n"
        "  color = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}";
    static const char* varyings[] = {"var0", "var1"};
    GLuint program = GLTestHelper::LoadProgram(v_shader_src, f_shader_src);
    EXPECT_LT(0u, program);
    glTransformFeedbackVaryings(program, 2, varyings, GL_SEPARATE_ATTRIBS);
    glLinkProgram(program);
    GLint link_status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    EXPECT_EQ(GL_TRUE, link_status);
    glUseProgram(program);
    *ret_position_loc = glGetAttribLocation(program, "a_position");
    EXPECT_LE(0, *ret_position_loc);
    return program;
  }

  GLManager gl_;
};

TEST_F(GetBufferSubDataTest, TransformFeedback) {
  if (ShouldSkipTest()) {
    return;
  }

  GLuint transform_feedback = 0;
  glGenTransformFeedbacks(1, &transform_feedback);
  glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transform_feedback);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLuint transform_feedback_buffer[2];
  glGenBuffers(2, transform_feedback_buffer);
  EXPECT_LT(0u, transform_feedback_buffer[0]);
  EXPECT_LT(0u, transform_feedback_buffer[1]);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
                   transform_feedback_buffer[0]);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 3 * 6 * sizeof(float), nullptr,
               GL_STATIC_READ);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1,
                   transform_feedback_buffer[1]);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 2 * 6 * sizeof(float), nullptr,
               GL_STATIC_READ);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLint position_loc = 0;
  SetupTransformFeedbackProgram(&position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLuint buffer = GLTestHelper::SetupUnitQuad(position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_LT(0u, buffer);

  uint8_t buffer_data[6] = {0};
  glGetBufferSubDataCHROMIUM(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 6, buffer_data);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glBeginTransformFeedback(GL_TRIANGLES);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glGetBufferSubDataCHROMIUM(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 6, buffer_data);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  // Bind the used buffer to another binding target and try to map it using
  // that target - should still fail.
  glBindBuffer(GL_PIXEL_PACK_BUFFER, transform_feedback_buffer[0]);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glGetBufferSubDataCHROMIUM(GL_PIXEL_PACK_BUFFER, 0, 6, buffer_data);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glEndTransformFeedback();
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glGetBufferSubDataCHROMIUM(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 6, buffer_data);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

// Tests end-to-end that GetBufferSubData delivers exact buffer contents
// with the workaround forced on, on both decoders (crbug.com/558109451).
TEST_F(GetBufferSubDataTest, GetBufferSubDataHealthyWithResetCheckWorkaround) {
  GLManager::Options options;
  options.size = gfx::Size(kCanvasSize, kCanvasSize);
  options.context_type = CONTEXT_TYPE_OPENGLES3;
  GpuDriverBugWorkarounds workarounds;
  workarounds.check_graphics_reset_status_after_readback = true;
  gl_.Destroy();
  gl_.InitializeWithWorkarounds(options, workarounds);
  if (ShouldSkipTest()) {
    return;
  }

  constexpr GLsizeiptr kSize = 64;
  std::array<uint8_t, kSize> data;
  for (GLsizeiptr i = 0; i < kSize; ++i) {
    data[i] = static_cast<uint8_t>(i * 3 + 1);
  }

  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  EXPECT_LT(0u, buffer);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
  glBufferData(GL_PIXEL_PACK_BUFFER, kSize, data.data(), GL_STATIC_DRAW);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  std::array<uint8_t, kSize> out = {};
  glGetBufferSubDataCHROMIUM(GL_PIXEL_PACK_BUFFER, 0, kSize, out.data());
  GLTestHelper::CheckGLError("no errors", __LINE__);

  for (GLsizeiptr i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], out[i]) << "byte " << i;
  }
  ASSERT_TRUE(gl_.decoder());
  EXPECT_FALSE(gl_.decoder()->WasContextLost());
}

}  // namespace gpu

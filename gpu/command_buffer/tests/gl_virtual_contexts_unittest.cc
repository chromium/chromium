// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER(Src) #Src

namespace gpu {

class GLVirtualContextsTest
    : public testing::TestWithParam<GpuDriverBugWorkarounds> {
 protected:
  static const int kSize0 = 4;
  static const int kSize1 = 8;
  static const int kSize2 = 16;

  void SetUp() override {
    GpuDriverBugWorkarounds workarounds = GetParam();
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    options.size = gfx::Size(kSize0, kSize0);
    gl_real_.InitializeWithWorkarounds(options, workarounds);
    // If the gl_real context is not initialised, switch to ES2 context type
    // and re-initialise
    if (!gl_real_.IsInitialized()) {
      gl_real_.Reset();  // Must reset object before initializing again
      options.context_type = CONTEXT_TYPE_OPENGLES2;
      gl_real_.InitializeWithWorkarounds(options, workarounds);
    }
    gl_real_shared_.InitializeWithWorkarounds(options, workarounds);
    options.virtual_manager = &gl_real_shared_;
    options.size = gfx::Size(kSize1, kSize1);
    gl1_.InitializeWithWorkarounds(options, workarounds);
    options.size = gfx::Size(kSize2, kSize2);
    gl2_.InitializeWithWorkarounds(options, workarounds);
  }

  void TearDown() override {
    gl1_.Destroy();
    gl2_.Destroy();
    gl_real_shared_.Destroy();
    gl_real_.Destroy();
  }

  GLuint SetupColoredVertexProgram() {
    static const char* v_shader_str = SHADER(
        attribute vec4 a_position;
        attribute vec4 a_color;
        varying vec4 v_color;
        void main()
        {
           gl_Position = a_position;
           v_color = a_color;
        }
     );

    static const char* f_shader_str = SHADER(
        precision mediump float;
        varying vec4 v_color;
        void main()
        {
          gl_FragColor = v_color;
        }
    );

    GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);
    glUseProgram(program);
    return program;
  }

  void SetUpColoredUnitQuad(const GLfloat* color) {
    GLuint program1 = SetupColoredVertexProgram();
    GLuint position_loc1 = glGetAttribLocation(program1, "a_position");
    GLuint color_loc1 = glGetAttribLocation(program1, "a_color");
    GLTestHelper::SetupUnitQuad(position_loc1);
    GLTestHelper::SetupColorsForUnitQuad(color_loc1, color, GL_STATIC_DRAW);
  }

  GLManager gl_real_;
  GLManager gl_real_shared_;
  GLManager gl1_;
  GLManager gl2_;
};

constexpr GLfloat kFloatRed[4] = {
    1.0f, 0.0f, 0.0f, 1.0f,
};
constexpr GLfloat kFloatGreen[4] = {
    0.0f, 1.0f, 0.0f, 1.0f,
};
constexpr uint8_t kExpectedRed[4] = {
    255, 0, 0, 255,
};
constexpr uint8_t kExpectedGreen[4] = {
    0, 255, 0, 255,
};

namespace {

void SetupSimpleShader(const uint8_t* color) {
  static const char* v_shader_str = SHADER(
      attribute vec4 a_Position;
      void main()
      {
         gl_Position = a_Position;
      }
   );

  static const char* f_shader_str = SHADER(
      precision mediump float;
      uniform vec4 u_color;
      void main()
      {
        gl_FragColor = u_color;
      }
  );

  GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);
  glUseProgram(program);

  GLuint position_loc = glGetAttribLocation(program, "a_Position");

  GLTestHelper::SetupUnitQuad(position_loc);

  GLuint color_loc = glGetUniformLocation(program, "u_color");
  glUniform4f(
      color_loc,
      color[0] / 255.0f,
      color[1] / 255.0f,
      color[2] / 255.0f,
      color[3] / 255.0f);
}

void TestDraw(int size) {
  uint8_t expected_clear[] = {
      127, 0, 255, 0,
  };
  glClearColor(0.5f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, size, size, 1, expected_clear, nullptr));
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

}  // anonymous namespace

// http://crbug.com/281565
TEST_P(GLVirtualContextsTest, Basic) {
  struct TestInfo {
    int size;
    uint8_t color[4];
    raw_ptr<GLManager> manager;
  };
  const int kNumTests = 3;
  TestInfo tests[] = {
    { kSize0, { 255, 0, 0, 0, }, &gl_real_, },
    { kSize1, { 0, 255, 0, 0, }, &gl1_, },
    { kSize2, { 0, 0, 255, 0, }, &gl2_, },
  };

  for (int ii = 0; ii < kNumTests; ++ii) {
    const TestInfo& test = tests[ii];
    GLManager* gl_manager = test.manager;
    gl_manager->MakeCurrent();
    SetupSimpleShader(test.color);
  }

  for (int ii = 0; ii < kNumTests; ++ii) {
    const TestInfo& test = tests[ii];
    GLManager* gl_manager = test.manager;
    gl_manager->MakeCurrent();
    TestDraw(test.size);
  }

  for (int ii = 0; ii < kNumTests; ++ii) {
    const TestInfo& test = tests[ii];
    GLManager* gl_manager = test.manager;
    gl_manager->MakeCurrent();
    EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, test.size, test.size, 0,
                                          test.color, nullptr));
  }

  for (int ii = 0; ii < kNumTests; ++ii) {
    const TestInfo& test = tests[ii];
    GLManager* gl_manager = test.manager;
    gl_manager->MakeCurrent();
    GLTestHelper::CheckGLError("no errors", __LINE__);
  }
}

// http://crbug.com/363407
TEST_P(GLVirtualContextsTest, VertexArrayObjectRestore) {
  GLuint vao1 = 0, vao2 = 0;

  gl1_.MakeCurrent();
  // Set up red quad in vao1.
  glGenVertexArraysOES(1, &vao1);
  glBindVertexArrayOES(vao1);
  SetUpColoredUnitQuad(kFloatRed);
  glFinish();

  gl2_.MakeCurrent();
  // Set up green quad in vao2.
  glGenVertexArraysOES(1, &vao2);
  glBindVertexArrayOES(vao2);
  SetUpColoredUnitQuad(kFloatGreen);
  glFinish();

  gl1_.MakeCurrent();
  // Test to ensure that vao1 is still the active VAO for this context.
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kSize1, kSize1, 0, kExpectedRed,
                                        nullptr));
  glFinish();
  GLTestHelper::CheckGLError("no errors", __LINE__);

  gl2_.MakeCurrent();
  // Test to ensure that vao2 is still the active VAO for this context.
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kSize2, kSize2, 0, kExpectedGreen,
                                        nullptr));
  glFinish();
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

// http://crbug.com/363407
TEST_P(GLVirtualContextsTest, VertexArrayObjectRestoreRebind) {
  GLuint vao1 = 0, vao2 = 0;

  gl1_.MakeCurrent();
  // Set up red quad in vao1.
  glGenVertexArraysOES(1, &vao1);
  glBindVertexArrayOES(vao1);
  SetUpColoredUnitQuad(kFloatRed);
  glFinish();

  gl2_.MakeCurrent();
  // Set up green quad in new vao2.
  glGenVertexArraysOES(1, &vao2);
  glBindVertexArrayOES(vao2);
  SetUpColoredUnitQuad(kFloatGreen);
  glFinish();

  gl1_.MakeCurrent();
  // Test to ensure that vao1 hasn't been corrupted after rebinding.
  // Bind 0 is required so that bind vao1 is not optimized away in the service.
  glBindVertexArrayOES(0);
  glBindVertexArrayOES(vao1);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kSize1, kSize1, 0, kExpectedRed,
                                        nullptr));
  glFinish();
  GLTestHelper::CheckGLError("no errors", __LINE__);

  gl2_.MakeCurrent();
  // Test to ensure that vao1 hasn't been corrupted after rebinding.
  // Bind 0 is required so that bind vao2 is not optimized away in the service.
  glBindVertexArrayOES(0);
  glBindVertexArrayOES(vao2);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kSize2, kSize2, 0, kExpectedGreen,
                                        nullptr));
  glFinish();

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

// http://crbug.com/363407
TEST_P(GLVirtualContextsTest, VertexArrayObjectRestoreDefault) {
  gl1_.MakeCurrent();
  // Set up red quad in default VAO.
  SetUpColoredUnitQuad(kFloatRed);
  glFinish();

  gl2_.MakeCurrent();
  // Set up green quad in default VAO.
  SetUpColoredUnitQuad(kFloatGreen);
  glFinish();

  // Gen & bind a non-default VAO.
  GLuint vao;
  glGenVertexArraysOES(1, &vao);
  glBindVertexArrayOES(vao);
  glFinish();

  gl1_.MakeCurrent();
  // Test to ensure that default VAO on gl1_ is still valid.
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kSize1, kSize1, 0, kExpectedRed,
                                        nullptr));
  glFinish();

  gl2_.MakeCurrent();
  // Test to ensure that default VAO on gl2_ is still valid.
  // This tests that a default VAO is restored even when it's not currently
  // bound during the context switch.
  glBindVertexArrayOES(0);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kSize2, kSize2, 0, kExpectedGreen,
                                        nullptr));
  glFinish();

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_P(GLVirtualContextsTest, VirtualQueries) {
  const GLenum query_targets[] = {
    GL_ANY_SAMPLES_PASSED_EXT,
    GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT,
    GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM,
    GL_COMMANDS_COMPLETED_CHROMIUM,
    GL_COMMANDS_ISSUED_CHROMIUM,
    GL_GET_ERROR_QUERY_CHROMIUM,
    GL_TIME_ELAPSED_EXT,
  };

  for (GLenum query_target : query_targets) {
    GLuint query1 = 0;
    gl1_.MakeCurrent();
    glGenQueriesEXT(1, &query1);
    glBeginQueryEXT(query_target, query1);
    const GLenum begin_error = glGetError();
    if (GL_INVALID_OPERATION == begin_error) {
      // Not supported, simply skip.
      glDeleteQueriesEXT(1, &query1);
      continue;
    }
    ASSERT_TRUE(GL_NO_ERROR == begin_error);

    GLuint query2 = 0;
    gl2_.MakeCurrent();
    glGenQueriesEXT(1, &query2);
    glBeginQueryEXT(query_target, query2);
    EXPECT_TRUE(GL_NO_ERROR == glGetError())
        << "Virtualized Query " << query_target << " failed.";

    gl1_.MakeCurrent();
    glEndQueryEXT(query_target);
    glFinish();

    GLuint query1_available = 0;
    glGetQueryObjectuivEXT(query1, GL_QUERY_RESULT_AVAILABLE_EXT,
                           &query1_available);
    EXPECT_TRUE(query1_available);

    glDeleteQueriesEXT(1, &query1);
    GLTestHelper::CheckGLError("no errors", __LINE__);

    gl2_.MakeCurrent();
    glEndQueryEXT(query_target);
    glFinish();

    GLuint query2_available = 0;
    glGetQueryObjectuivEXT(query2, GL_QUERY_RESULT_AVAILABLE_EXT,
                           &query2_available);
    EXPECT_TRUE(query2_available);

    glDeleteQueriesEXT(1, &query2);
    GLTestHelper::CheckGLError("no errors", __LINE__);
  }
}

// http://crbug.com/930327
TEST_P(GLVirtualContextsTest, Texture2DArrayAnd3DRestore) {
  // This test should only be run for ES3 or higher context
  // So if the current version is ES2, do not run this test
  if (gl1_.GetContextType() == CONTEXT_TYPE_OPENGLES2)
    return;

  // Context 1
  gl1_.MakeCurrent();
  GLuint tex1_2d_array = 0, tex1_3d = 0;
  glActiveTexture(GL_TEXTURE0);
  // 2d array texture
  glGenTextures(1, &tex1_2d_array);
  glBindTexture(GL_TEXTURE_2D_ARRAY, tex1_2d_array);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // 3d texture
  glGenTextures(1, &tex1_3d);
  glBindTexture(GL_TEXTURE_3D, tex1_3d);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST_MIPMAP_NEAREST);
  glFinish();

  // switch to context 2
  gl2_.MakeCurrent();
  GLuint tex2_2d_array = 0, tex2_3d = 0;
  glActiveTexture(GL_TEXTURE0);
  // 2d array texture
  glGenTextures(1, &tex2_2d_array);
  glBindTexture(GL_TEXTURE_2D_ARRAY, tex2_2d_array);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  // 3d texture
  glGenTextures(1, &tex2_3d);
  glBindTexture(GL_TEXTURE_3D, tex2_3d);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glFinish();

  // switch back to context1
  gl1_.MakeCurrent();

  // get the texture parameters which were programmed earlier for context1
  GLint tex_2d_array_params = 0, tex_3d_params = 0;
  glGetTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
                      &tex_2d_array_params);
  glGetTexParameteriv(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, &tex_3d_params);
  // Do checks to make sure texture params are restored correctly after context
  // switching
  EXPECT_EQ(GL_NEAREST, tex_2d_array_params);
  EXPECT_EQ(GL_NEAREST_MIPMAP_NEAREST, tex_3d_params);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}
static const GpuDriverBugWorkarounds workarounds_cases[] = {
    // No extra workarounds.
    GpuDriverBugWorkarounds(),

#if BUILDFLAG(IS_ANDROID)
    // Regression tests for https://crbug.com/768324
    //
    // TODO(kainino): The #if is added because this case does not pass on Mac
    // or Linux. My guess is that this workaround requires the backing context
    // to be OpenGL ES (not OpenGL Core Profile).
    GpuDriverBugWorkarounds({
        USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS,
    }),
#endif

};

INSTANTIATE_TEST_SUITE_P(WithWorkarounds,
                         GLVirtualContextsTest,
                         ::testing::ValuesIn(workarounds_cases));

}  // namespace gpu

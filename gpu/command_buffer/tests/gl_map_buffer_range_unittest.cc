// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include "build/build_config.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER(Src) #Src

namespace gpu {

class ES3MapBufferRangeTest : public testing::Test {
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

TEST_F(ES3MapBufferRangeTest, DrawArraysAndInstanced) {
  if (ShouldSkipTest())
    return;

  const uint8_t kRedColor[] = {255, 0, 0, 255};
  const uint8_t kBlackColor[] = {0, 0, 0, 255};
  const GLsizei kPrimCount = 4;

  GLint position_loc = 0;
  SetupSimpleProgram(&position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLuint buffer = GLTestHelper::SetupUnitQuad(position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_LT(0u, buffer);

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glMapBufferRange(GL_ARRAY_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  // The following test is necessary to make sure draw calls do not just check
  // bound buffers, but actual buffers that are attached to the enabled vertex
  // attribs.
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glUnmapBuffer(GL_ARRAY_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));
}

TEST_F(ES3MapBufferRangeTest, DrawElementsAndInstanced) {
  if (ShouldSkipTest())
    return;

  const uint8_t kRedColor[] = {255, 0, 0, 255};
  const uint8_t kBlackColor[] = {0, 0, 0, 255};
  const GLsizei kPrimCount = 4;

  GLint position_loc = 0;
  SetupSimpleProgram(&position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  std::vector<GLuint> buffers =
      GLTestHelper::SetupIndexedUnitQuad(position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_EQ(2u, buffers.size());
  EXPECT_LT(0u, buffers[0]);
  EXPECT_LT(0u, buffers[1]);

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glMapBufferRange(GL_ARRAY_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  // The following test is necessary to make sure draw calls do not just check
  // bound buffers, but actual buffers that are attached to the enabled vertex
  // attribs.
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);

  glUnmapBuffer(GL_ARRAY_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));
}

TEST_F(ES3MapBufferRangeTest, ReadPixels) {
  if (ShouldSkipTest())
    return;

  // TODO(crbug.com/angleproject/5213) consistent driver errors on this config.
  if (GPUTestBotConfig::CurrentConfigMatches("Linux AMD"))
    return;

  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  EXPECT_LT(0u, buffer);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
  glBufferData(GL_PIXEL_PACK_BUFFER, 4 * kCanvasSize * kCanvasSize, 0,
               GL_STATIC_READ);

  glReadPixels(0, 0, kCanvasSize, kCanvasSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 6, GL_MAP_WRITE_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glReadPixels(0, 0, kCanvasSize, kCanvasSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40778773): This step causes a crash on mac intel-uhd bot.
  if (GPUTestBotConfig::CurrentConfigMatches("Mac Intel 0x3e9b"))
    return;
#endif

  glReadPixels(0, 0, kCanvasSize, kCanvasSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(ES3MapBufferRangeTest, TexImageAndSubImage2D) {
  if (ShouldSkipTest())
    return;

  const GLsizei kTextureSize = 4;
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  EXPECT_LT(0u, buffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, 4 * kTextureSize * kTextureSize, 0,
               GL_STATIC_READ);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  EXPECT_LT(0u, tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTextureSize, kTextureSize, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTextureSize, kTextureSize, GL_RGBA,
                  GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTextureSize, kTextureSize, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTextureSize, kTextureSize, GL_RGBA,
                  GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTextureSize, kTextureSize, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTextureSize, kTextureSize, GL_RGBA,
                  GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

// TODO(crbug.com/40904610): Fix flakiness and re-enable the test.
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_TexImageAndSubImage3D DISABLED_TexImageAndSubImage3D
#else
#define MAYBE_TexImageAndSubImage3D TexImageAndSubImage3D
#endif
TEST_F(ES3MapBufferRangeTest, MAYBE_TexImageAndSubImage3D) {
  if (ShouldSkipTest())
    return;

  const GLsizei kTextureSize = 4;
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  EXPECT_LT(0u, buffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
  glBufferData(GL_PIXEL_UNPACK_BUFFER,
               4 * kTextureSize * kTextureSize * kTextureSize, 0,
               GL_STATIC_READ);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  EXPECT_LT(0u, tex);
  glBindTexture(GL_TEXTURE_3D, tex);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kTextureSize, kTextureSize,
               kTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, kTextureSize, kTextureSize,
                  kTextureSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kTextureSize, kTextureSize,
               kTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, kTextureSize, kTextureSize,
                  kTextureSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kTextureSize, kTextureSize,
               kTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, kTextureSize, kTextureSize,
                  kTextureSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

// TODO(zmo): Add tests for CompressedTex* functions.

TEST_F(ES3MapBufferRangeTest, TransformFeedback) {
  if (ShouldSkipTest())
    return;

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
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 3 * 6 * sizeof(float), 0,
               GL_STATIC_READ);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1,
                   transform_feedback_buffer[1]);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 2 * 6 * sizeof(float), 0,
               GL_STATIC_READ);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLint position_loc = 0;
  SetupTransformFeedbackProgram(&position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLuint buffer = GLTestHelper::SetupUnitQuad(position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_LT(0u, buffer);

  glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 6, GL_MAP_WRITE_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glBeginTransformFeedback(GL_TRIANGLES);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glBeginTransformFeedback(GL_TRIANGLES);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 6, GL_MAP_WRITE_BIT);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  // Bind the used buffer to another binding target and try to map it using
  // that target - should still fail.
  glBindBuffer(GL_PIXEL_PACK_BUFFER, transform_feedback_buffer[0]);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 6, GL_MAP_READ_BIT);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glEndTransformFeedback();
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 6, GL_MAP_WRITE_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(ES3MapBufferRangeTest, GetBufferParameteriv) {
  if (ShouldSkipTest())
    return;

  GLuint buffer;
  glGenBuffers(1, &buffer);
  EXPECT_LT(0u, buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, 64, 0, GL_STATIC_READ);

  GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
  glMapBufferRange(GL_ARRAY_BUFFER, 16, 32, access);
  GLint param = 0;
  glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS_FLAGS, &param);
  EXPECT_EQ(access, static_cast<GLbitfield>(param));
  glUnmapBuffer(GL_ARRAY_BUFFER);
  param = 0;
  glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS_FLAGS, &param);
  EXPECT_EQ(0u, static_cast<GLbitfield>(param));

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(ES3MapBufferRangeTest, CopyBufferSubData) {
  if (ShouldSkipTest())
    return;

  const GLsizeiptr kSize = 64;
  const GLsizeiptr kHalfSize = kSize / 2;
  const GLintptr kReadOffset = 0;
  const GLintptr kWriteOffset = kHalfSize;
  const GLsizeiptr kCopySize = 5;
  const uint8_t kValue0 = 3;
  const uint8_t kValue1 = 21;

  GLuint buffer;
  glGenBuffers(1, &buffer);
  EXPECT_NE(0u, buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, kSize, nullptr, GL_STREAM_DRAW);

  std::array<uint8_t, kHalfSize> data0;
  data0.fill(kValue0);
  glBufferSubData(GL_ARRAY_BUFFER, 0, kHalfSize, data0.data());

  std::array<uint8_t, kHalfSize> data1;
  data1.fill(kValue1);
  glBufferSubData(GL_ARRAY_BUFFER, kHalfSize, kHalfSize, data1.data());

  GLTestHelper::CheckGLError("no errors", __LINE__);

  // Verify the data is initialized.
  const uint8_t* map_ptr = static_cast<uint8_t*>(
      glMapBufferRange(GL_ARRAY_BUFFER, 0, kSize, GL_MAP_READ_BIT));
  ASSERT_NE(nullptr, map_ptr);

  EXPECT_EQ(0, memcmp(map_ptr, data0.data(), kHalfSize));
  EXPECT_EQ(0, memcmp(map_ptr + kHalfSize, data1.data(), kHalfSize));

  glUnmapBuffer(GL_ARRAY_BUFFER);

  glCopyBufferSubData(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, kReadOffset,
                      kWriteOffset, kCopySize);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  // Verify the data is copied.
  map_ptr = static_cast<uint8_t*>(
      glMapBufferRange(GL_ARRAY_BUFFER, 0, kSize, GL_MAP_READ_BIT));
  ASSERT_NE(nullptr, map_ptr);

  for (GLsizeiptr ii = 0; ii < kHalfSize; ++ii) {
    EXPECT_EQ(kValue0, map_ptr[ii]);
  }
  for (GLsizeiptr ii = kHalfSize; ii < kSize; ++ii) {
    if (ii >= kWriteOffset && ii < kWriteOffset + kCopySize) {
      EXPECT_EQ(kValue0, map_ptr[ii]);
    } else {
      EXPECT_EQ(kValue1, map_ptr[ii]);
    }
  }
}

TEST_F(ES3MapBufferRangeTest, Delete) {
  // Test that we can unbind a mapped buffer and deleting it still unmaps it.
  if (ShouldSkipTest())
    return;

  const int kNumBuffers = 3;
  const int kSize = sizeof(GLuint);

  GLuint buffers[kNumBuffers];
  glGenBuffers(kNumBuffers, buffers);
  // Set each buffer to contain its name.
  for (int i = 0; i < kNumBuffers; ++i) {
    EXPECT_NE(0u, buffers[i]);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[i]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLuint), &buffers[i], GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  GLTestHelper::CheckGLError("no errors", __LINE__);

  glBindBuffer(GL_ARRAY_BUFFER, buffers[2]);

  // Use a different binding point to map the buffer than we originally used,
  // to test if we are improperly using Buffer::initial_target() anywhere.
  glBindBuffer(GL_COPY_READ_BUFFER, buffers[0]);
  const GLuint* map_ptr_0 = static_cast<GLuint*>(
      glMapBufferRange(GL_COPY_READ_BUFFER, 0, kSize, GL_MAP_READ_BIT));
  ASSERT_NE(nullptr, map_ptr_0);
  EXPECT_EQ(buffers[0], *map_ptr_0);
  glBindBuffer(GL_COPY_READ_BUFFER, buffers[1]);

  // The buffer is no longer bound. Delete it.
  glDeleteBuffers(1, &buffers[0]);

  GLint copy_read_buffer = 0;
  glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &copy_read_buffer);
  EXPECT_EQ(copy_read_buffer, (GLint)buffers[1]);
  GLint array_buffer = 0;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer);
  EXPECT_EQ(array_buffer, (GLint)buffers[2]);

  // Make sure buffers[1] [2] are truly still bound by mapping them and
  // checking the contents.
  const GLuint* map_ptr_1 = static_cast<GLuint*>(
      glMapBufferRange(GL_COPY_READ_BUFFER, 0, kSize, GL_MAP_READ_BIT));
  ASSERT_NE(nullptr, map_ptr_1);
  EXPECT_EQ(buffers[1], *map_ptr_1);
  glUnmapBuffer(GL_COPY_READ_BUFFER);

  const GLuint* map_ptr_2 = static_cast<GLuint*>(
      glMapBufferRange(GL_ARRAY_BUFFER, 0, kSize, GL_MAP_READ_BIT));
  ASSERT_NE(nullptr, map_ptr_2);
  EXPECT_EQ(buffers[2], *map_ptr_2);
  glUnmapBuffer(GL_ARRAY_BUFFER);

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

// TODO(zmo): add tests for uniform buffer mapping.

}  // namespace gpu

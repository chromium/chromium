// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>

#include "base/command_line.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {
class ANGLEShaderPixelLocalStorageTest : public testing::Test {
 public:
  ANGLEShaderPixelLocalStorageTest() = default;

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    auto* command_line = base::CommandLine::ForCurrentProcess();
    if (gles2::UsePassthroughCommandDecoder(command_line)) {
      // TODO(crbug.com/40278644): fix the test for passthrough.
      GTEST_SKIP();
    }
#endif

    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    gl_.Initialize(options);
  }

  void TearDown() override { gl_.Destroy(); }

 protected:
  GLManager gl_;
};

static GLint gl_get_integer(GLenum pname) {
  GLint value = -1;
  glGetIntegerv(pname, &value);
  return value;
}

// Verifies that GetIntegerv accepts the new tokens from
// ANGLE_shader_pixel_local_storage.
TEST_F(ANGLEShaderPixelLocalStorageTest, GetIntegerv) {
  if (!gl_.IsInitialized() ||
      !GLTestHelper::HasExtension("GL_ANGLE_shader_pixel_local_storage")) {
    GTEST_SKIP();
  }

  EXPECT_GT(gl_get_integer(GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE), 4);
  EXPECT_GT(gl_get_integer(
                GL_MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE),
            0);
  EXPECT_GT(
      gl_get_integer(
          GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE),
      4);
  EXPECT_EQ(gl_get_integer(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE), 0);
}

#define EXPECT_PLS_INTEGER(plane, pname, expected)                           \
  {                                                                          \
    GLint value = -1;                                                        \
    glGetFramebufferPixelLocalStorageParameterivANGLE(plane, pname, &value); \
    EXPECT_EQ(value, GLint(expected));                                       \
  }

#define EXPECT_PLS_CLEAR_VALUE_FLOAT(plane, rgba)                     \
  {                                                                   \
    std::array<GLfloat, 4> expected rgba;                             \
    std::array<GLfloat, 4> value{};                                   \
    glGetFramebufferPixelLocalStorageParameterfvANGLE(                \
        plane, GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE, value.data()); \
    EXPECT_EQ(value, expected);                                       \
  }

#define EXPECT_PLS_CLEAR_VALUE_INT(plane, rgba)                     \
  {                                                                 \
    std::array<GLint, 4> expected rgba;                             \
    std::array<GLint, 4> value{-1, -1, -1, -1};                     \
    glGetFramebufferPixelLocalStorageParameterivANGLE(              \
        plane, GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE, value.data()); \
    EXPECT_EQ(value, expected);                                     \
  }

#define EXPECT_PLS_CLEAR_VALUE_UNSIGNED_INT(plane, rgba)                      \
  {                                                                           \
    std::array<GLuint, 4> expected rgba;                                      \
    std::array<GLint, 4> valuei{-1, -1, -1, -1};                              \
    glGetFramebufferPixelLocalStorageParameterivANGLE(                        \
        plane, GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE, valuei.data()); \
    std::array<GLuint, 4> value;                                              \
    memcpy(value.data(), valuei.data(), sizeof(value));                       \
    EXPECT_EQ(value, expected);                                               \
  }

#define EXPECT_GL_ERROR(error) \
  EXPECT_EQ(glGetError(), static_cast<GLenum>(error))

// Verifies that glGetFramebufferPixelLocalStorageParameter{f,i}vANGLE is
// marshalled properly over the command buffer. Thorough testing of these
// commands is done in angle_end2end_tests.
TEST_F(ANGLEShaderPixelLocalStorageTest,
       GetFramebufferPixelLocalStorageParameter) {
  if (!gl_.IsInitialized() ||
      !GLTestHelper::HasExtension("GL_ANGLE_shader_pixel_local_storage")) {
    return;
  }

  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexStorage2DEXT(GL_TEXTURE_2D, 3, GL_RGBA8UI, 10, 10);

  GLint maxPLSPlanes = gl_get_integer(GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE);
  for (GLint plane : {0, maxPLSPlanes - 1}) {
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    glFramebufferTexturePixelLocalStorageANGLE(plane, tex, 1, 0);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8UI);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, tex);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    // Using texture name 0 deinitializes the entire plane.
    glFramebufferTexturePixelLocalStorageANGLE(plane, 0, 1, 2);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(plane, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    EXPECT_PLS_CLEAR_VALUE_FLOAT(plane, ({0, 0, 0, 0}));
    EXPECT_PLS_CLEAR_VALUE_INT(plane, ({0, 0, 0, 0}));
    EXPECT_PLS_CLEAR_VALUE_UNSIGNED_INT(plane, ({0, 0, 0, 0}));

    glFramebufferPixelLocalClearValuefvANGLE(
        plane, std::array{0.f, -1.f, .5f, 999.f}.data());
    glFramebufferPixelLocalClearValueivANGLE(
        plane, std::array{0, -100, 99999, -99999}.data());
    glFramebufferPixelLocalClearValueuivANGLE(
        plane, std::array{0u, 100u, 99999u, 9999999u}.data());
    EXPECT_GL_ERROR(GL_NO_ERROR);

    EXPECT_PLS_CLEAR_VALUE_FLOAT(plane, ({0, -1, .5f, 999}));
    EXPECT_PLS_CLEAR_VALUE_INT(plane, ({0, -100, 99999, -99999}));
    EXPECT_PLS_CLEAR_VALUE_UNSIGNED_INT(plane, ({0, 100, 99999, 9999999}));
  }

  EXPECT_GL_ERROR(GL_NO_ERROR);
}

// Verifies all LOAD_OP_*_ANGLE and STORE_OP_*_ANGLE tokens are accepted by the
// command buffer.
TEST_F(ANGLEShaderPixelLocalStorageTest, LoadStoreTokens) {
  if (!gl_.IsInitialized() ||
      !GLTestHelper::HasExtension("GL_ANGLE_shader_pixel_local_storage")) {
    return;
  }

// Test skipped on Intel-based Macs when running Metal. crbug.com/326278125
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64)
  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    return;
  }
#endif

  GLuint texs[4];
  glGenTextures(4, texs);
  for (GLuint tex : texs) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, 10, 10);
    EXPECT_GL_ERROR(GL_NO_ERROR);
  }

  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  glDisable(GL_DITHER);

  glFramebufferTexturePixelLocalStorageANGLE(0, texs[0], 0, 0);
  glFramebufferTexturePixelLocalStorageANGLE(1, texs[1], 0, 0);
  glFramebufferTexturePixelLocalStorageANGLE(2, texs[2], 0, 0);
  glFramebufferTexturePixelLocalStorageANGLE(3, texs[3], 0, 0);
  glBeginPixelLocalStorageANGLE(
      4, std::array<GLenum, 4>{GL_LOAD_OP_CLEAR_ANGLE, GL_LOAD_OP_LOAD_ANGLE,
                               GL_LOAD_OP_ZERO_ANGLE, GL_DONT_CARE}
             .data());
  EXPECT_GL_ERROR(GL_NO_ERROR);
  EXPECT_EQ(gl_get_integer(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE), 4);

  glEndPixelLocalStorageANGLE(
      4, std::array<GLenum, 4>{GL_STORE_OP_STORE_ANGLE, GL_DONT_CARE,
                               GL_DONT_CARE, GL_DONT_CARE}
             .data());
  EXPECT_GL_ERROR(GL_NO_ERROR);
  EXPECT_EQ(gl_get_integer(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE), 0);
}

// Verifies that ANGLE_shader_pixel_local_storage commands related to drawing
// are all marshalled properly over the command buffer. Thorough testing of
// these commands is done in angle_end2end_tests.
TEST_F(ANGLEShaderPixelLocalStorageTest, DrawAPI) {
  if (!gl_.IsInitialized() ||
      !GLTestHelper::HasExtension("GL_ANGLE_shader_pixel_local_storage")) {
    return;
  }

// Test skipped on Intel-based Macs when running Metal. crbug.com/326278125
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64)
  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    return;
  }
#endif

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, 10, 10);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  glViewport(0, 0, 10, 10);

  GLuint vs = GLTestHelper::CompileShader(GL_VERTEX_SHADER, R"(#version 300 es
void main() {
    gl_Position.x = (gl_VertexID & 1) == 0 ? -1. : 1.;
    gl_Position.y = (gl_VertexID & 2) == 0 ? -1. : 1.;
    gl_Position.zw = vec2(0, 1);
})");
  GLuint fs = GLTestHelper::CompileShader(GL_FRAGMENT_SHADER, R"(#version 300 es
#extension GL_ANGLE_shader_pixel_local_storage : require
precision lowp float;
uniform vec4 color;
layout(binding=0, rgba8) uniform lowp pixelLocalANGLE pls;
void main() {
  vec4 newColor = color + pixelLocalLoadANGLE(pls);
  pixelLocalStoreANGLE(pls, newColor);
})");
  GLuint program = GLTestHelper::LinkProgram(vs, fs);
  glUseProgram(program);
  GLint colorUniLocation = glGetUniformLocation(program, "color");
  EXPECT_GL_ERROR(GL_NO_ERROR);

  glDisable(GL_DITHER);

  glBeginPixelLocalStorageANGLE(
      1, std::array<GLenum, 1>{GL_LOAD_OP_ZERO_ANGLE}.data());
  EXPECT_GL_ERROR(GL_NO_ERROR);
  EXPECT_EQ(gl_get_integer(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE), 1);

  glUniform4f(colorUniLocation, 1, 0, 0, 0);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glPixelLocalStorageBarrierANGLE();

  glUniform4f(colorUniLocation, 0, 0, 1, 0);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glEndPixelLocalStorageANGLE(
      1, std::array<GLenum, 1>{GL_STORE_OP_STORE_ANGLE}.data());
  EXPECT_GL_ERROR(GL_NO_ERROR);
  EXPECT_EQ(gl_get_integer(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE), 0);

  GLuint readFBO;
  glGenFramebuffers(1, &readFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, readFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex, 0);
  EXPECT_TRUE(GLTestHelper::CheckPixels(
      0, 0, 10, 10, 0, std::array<uint8_t, 4>{255, 0, 255, 0}.data(), nullptr));
}

// Verifies that the PLS ban on FBO 0 is also enforced when FBO "0" is emulated
// by the command buffer on an application-side framebuffer object. In this
// scenario, ANGLE will not generate an error, and the command buffer is
// responsible for synthesizing a GL error if the app tries to use PLS on FBO 0.
TEST_F(ANGLEShaderPixelLocalStorageTest, BlockEmulatedDefaultFramebuffer) {
  if (!gl_.IsInitialized() ||
      !GLTestHelper::HasExtension("GL_ANGLE_shader_pixel_local_storage")) {
    return;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, 10, 10);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
  EXPECT_GL_ERROR(GL_INVALID_OPERATION);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  glFramebufferPixelLocalClearValuefvANGLE(
      0, std::array{0.f, 0.f, 0.f, 0.f}.data());
  EXPECT_GL_ERROR(GL_INVALID_OPERATION);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  glFramebufferPixelLocalClearValueivANGLE(0, std::array{0, 0, 0, 0}.data());
  EXPECT_GL_ERROR(GL_INVALID_OPERATION);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  glFramebufferPixelLocalClearValueuivANGLE(0,
                                            std::array{0u, 0u, 0u, 0u}.data());
  EXPECT_GL_ERROR(GL_INVALID_OPERATION);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  glBeginPixelLocalStorageANGLE(
      1, std::array<GLenum, 1>{GL_LOAD_OP_ZERO_ANGLE}.data());
  EXPECT_GL_ERROR(GL_INVALID_OPERATION);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  glBeginPixelLocalStorageANGLE(
      1, std::array<GLenum, 1>{GL_STORE_OP_STORE_ANGLE}.data());
  EXPECT_GL_ERROR(GL_INVALID_OPERATION);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  glPixelLocalStorageBarrierANGLE();
  EXPECT_GL_ERROR(GL_INVALID_OPERATION);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  std::array<GLfloat, 4> valuef{};
  glGetFramebufferPixelLocalStorageParameterfvANGLE(
      0, GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE, valuef.data());
  EXPECT_GL_ERROR(GL_INVALID_OPERATION);
  EXPECT_GL_ERROR(GL_NO_ERROR);

  GLint valuei = -1;
  glGetFramebufferPixelLocalStorageParameterivANGLE(
      0, GL_PIXEL_LOCAL_FORMAT_ANGLE, &valuei);
  EXPECT_GL_ERROR(GL_INVALID_OPERATION);
  EXPECT_GL_ERROR(GL_NO_ERROR);
}
}  // namespace gpu

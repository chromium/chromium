// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdint.h>

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

// Blits texture bound to active texture unit to currently bound framebuffer.
// Viewport must be set by caller.
void BlitTexture() {
  const GLuint kVertexPositionAttrib = 0;
  const GLfloat kQuadVertices[] = {-1.0f, -1.0f, 1.0f,  -1.0f,
                                   1.0f,  1.0f,  -1.0f, 1.0f};
  GLuint buffer_id = 0;
  glGenBuffers(1, &buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(kVertexPositionAttrib);
  glVertexAttribPointer(kVertexPositionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

  GLuint shader_program = glCreateProgram();

  static const char* kBlitTextureVertexShader =
      "precision mediump float;\n"
      "attribute vec2 a_position;\n"
      "varying mediump vec2 v_uv;\n"
      "void main(void) {\n"
      "  gl_Position = vec4(a_position, 0, 1);\n"
      "  v_uv = 0.5 * (a_position + vec2(1, 1));\n"
      "}\n";
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &kBlitTextureVertexShader, 0);
  glCompileShader(vertex_shader);
  glAttachShader(shader_program, vertex_shader);

  static const char* kBlitTextureFragmentShader =
      "uniform sampler2D u_sampler;\n"
      "varying mediump vec2 v_uv;\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(u_sampler, v_uv);\n"
      "}\n";
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &kBlitTextureFragmentShader, 0);
  glCompileShader(fragment_shader);
  glAttachShader(shader_program, fragment_shader);

  glBindAttribLocation(shader_program, kVertexPositionAttrib, "a_position");

  glLinkProgram(shader_program);
  glUseProgram(shader_program);

  GLuint sampler_handle = glGetUniformLocation(shader_program, "u_sampler");
  glUniform1i(sampler_handle, 0);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  ASSERT_TRUE(glGetError() == GL_NONE);
}

}  // namespace

class TextureStorageTest : public testing::Test {
 protected:
  static const GLsizei kResolution = 64;
  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kResolution, kResolution);
    gl_.Initialize(options);
    gl_.MakeCurrent();

    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex_, 0);

    const GLubyte* extensions = glGetString(GL_EXTENSIONS);
    ext_texture_storage_available_ = strstr(
        reinterpret_cast<const char*>(extensions), "GL_EXT_texture_storage");
  }

  void TearDown() override { gl_.Destroy(); }

  GLManager gl_;
  GLuint tex_ = 0;
  GLuint fbo_ = 0;
  bool ext_texture_storage_available_ = false;
};

TEST_F(TextureStorageTest, CorrectPixels) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 2, 2);

  // Mesa drivers crash without rebinding to FBO. It's why
  // DISABLE_TEXTURE_STORAGE workaround is introduced. crbug.com/521904
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex_, 0);

  uint8_t source_pixels[16] = {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
  glTexSubImage2D(GL_TEXTURE_2D,
                  0,
                  0, 0,
                  2, 2,
                  GL_RGBA, GL_UNSIGNED_BYTE,
                  source_pixels);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 2, 2, 0, source_pixels, nullptr));
}

TEST_F(TextureStorageTest, IsImmutable) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);

  GLint param = 0;
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT_EXT, &param);
  EXPECT_TRUE(param);
}

TEST_F(TextureStorageTest, OneLevel) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);

  uint8_t source_pixels[64] = {0};

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
}

TEST_F(TextureStorageTest, MultipleLevels) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 2, GL_RGBA8_OES, 2, 2);

  uint8_t source_pixels[16] = {0};

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 1, 1,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 2, 0, 0, 1, 1,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
}

TEST_F(TextureStorageTest, BadTarget) {
  if (!ext_texture_storage_available_)
    return;

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexStorage2DEXT(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1, GL_RGBA8_OES, 4, 4);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), glGetError());
}

TEST_F(TextureStorageTest, InvalidId) {
  if (!ext_texture_storage_available_)
    return;

  glDeleteTextures(1, &tex_);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
}

TEST_F(TextureStorageTest, CannotRedefine) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
}

TEST_F(TextureStorageTest, InternalFormatBleedingToTexImage) {
  if (!ext_texture_storage_available_)
    return;

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  // The context is ES2 context.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8_OES, 4, 4, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  EXPECT_NE(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

TEST_F(TextureStorageTest, LuminanceEmulation) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 2, 2);
  // Mesa drivers crash without rebinding to FBO. It's why
  // DISABLE_TEXTURE_STORAGE workaround is introduced. crbug.com/521904
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex_, 0);
  ASSERT_TRUE(glGetError() == GL_NONE);

  GLuint luminance_tex = 0;
  glGenTextures(1, &luminance_tex);
  glBindTexture(GL_TEXTURE_2D, luminance_tex);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_LUMINANCE8_EXT, 2, 2);
  ASSERT_TRUE(glGetError() == GL_NONE);

  const uint8_t source_data[4] = {1, 1, 1, 1};
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                  source_data);
  ASSERT_TRUE(glGetError() == GL_NONE);

  BlitTexture();

  const uint8_t swizzled_pixel[4] = {1, 1, 1, 255};
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 2, 2, 0, swizzled_pixel, nullptr));
}

TEST_F(TextureStorageTest, AlphaEmulation) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 2, 2);
  // Mesa drivers crash without rebinding to FBO. It's why
  // DISABLE_TEXTURE_STORAGE workaround is introduced. crbug.com/521904
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex_, 0);
  ASSERT_TRUE(glGetError() == GL_NONE);

  GLuint alpha_tex = 0;
  glGenTextures(1, &alpha_tex);
  glBindTexture(GL_TEXTURE_2D, alpha_tex);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_ALPHA8_EXT, 2, 2);
  ASSERT_TRUE(glGetError() == GL_NONE);

  const uint8_t source_data[4] = {1, 1, 1, 1};
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_ALPHA, GL_UNSIGNED_BYTE,
                  source_data);
  ASSERT_TRUE(glGetError() == GL_NONE);

  BlitTexture();

  const uint8_t swizzled_pixel[4] = {0, 0, 0, 1};
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 2, 2, 0, swizzled_pixel, nullptr));
}

TEST_F(TextureStorageTest, LuminanceAlphaEmulation) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 2, 2);
  // Mesa drivers crash without rebinding to FBO. It's why
  // DISABLE_TEXTURE_STORAGE workaround is introduced. crbug.com/521904
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex_, 0);
  ASSERT_TRUE(glGetError() == GL_NONE);

  GLuint luminance_alpha_tex = 0;
  glGenTextures(1, &luminance_alpha_tex);
  glBindTexture(GL_TEXTURE_2D, luminance_alpha_tex);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_LUMINANCE8_ALPHA8_EXT, 2, 2);
  ASSERT_TRUE(glGetError() == GL_NONE);

  const uint8_t source_data[8] = {1, 2, 1, 2, 1, 2, 1, 2};
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_LUMINANCE_ALPHA,
                  GL_UNSIGNED_BYTE, source_data);
  ASSERT_TRUE(glGetError() == GL_NONE);

  BlitTexture();

  const uint8_t swizzled_pixel[4] = {1, 1, 1, 2};
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 2, 2, 0, swizzled_pixel, nullptr));
}

}  // namespace gpu




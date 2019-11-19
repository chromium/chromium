// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stddef.h>
#include <stdint.h>

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_share_group.h"

namespace gpu {

class GLUnallocatedTextureTest : public testing::Test {
 protected:
  void SetUp() override { gl_.Initialize(GLManager::Options()); }

  void TearDown() override { gl_.Destroy(); }

  GLuint MakeProgram(const char* fragment_shader) {
    constexpr const char kVertexShader[] =
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }";
    GLuint program = GLTestHelper::LoadProgram(kVertexShader, fragment_shader);
    if (!program)
      return 0;
    glUseProgram(program);

    GLint location_sampler = glGetUniformLocation(program, "sampler");
    glUniform1i(location_sampler, 0);
    return program;
  }

  // Creates a texture on target, setting up filters but without setting any
  // level image.
  GLuint MakeUninitializedTexture(GLenum target) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(target, texture);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
  }

  GLManager gl_;
};

// Test that we can render with GL_TEXTURE_2D textures that are unallocated.
// This should not generate errors or assert.
TEST_F(GLUnallocatedTextureTest, RenderUnallocatedTexture2D) {
  constexpr const char kFragmentShader[] =
      "uniform sampler2D sampler;\n"
      "void main() { gl_FragColor = texture2D(sampler, vec2(0.0, 0.0)); }\n";
  GLuint program = MakeProgram(kFragmentShader);
  ASSERT_TRUE(program);
  GLuint texture = MakeUninitializedTexture(GL_TEXTURE_2D);

  glDrawArrays(GL_TRIANGLES, 0, 3);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  glDeleteTextures(1, &texture);
  glDeleteProgram(program);
}

// Test that we can render with GL_TEXTURE_EXTERNAL_OES textures that are
// unallocated. This should not generate errors or assert.
TEST_F(GLUnallocatedTextureTest, RenderUnallocatedTextureExternal) {
  if (!gl_.GetCapabilities().egl_image_external) {
    LOG(INFO) << "GL_OES_EGL_image_external not supported, skipping test";
    return;
  }
  constexpr const char kFragmentShader[] =
      "#extension GL_OES_EGL_image_external : enable\n"
      "uniform samplerExternalOES sampler;\n"
      "void main() { gl_FragColor = texture2D(sampler, vec2(0.0, 0.0)); }\n";
  GLuint program = MakeProgram(kFragmentShader);
  ASSERT_TRUE(program);
  GLuint texture = MakeUninitializedTexture(GL_TEXTURE_EXTERNAL_OES);

  glDrawArrays(GL_TRIANGLES, 0, 3);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  glDeleteTextures(1, &texture);
  glDeleteProgram(program);
}

// Test that we can render with GL_TEXTURE_RECTANGLE_ARB textures that are
// unallocated. This should not generate errors or assert.
TEST_F(GLUnallocatedTextureTest, RenderUnallocatedTextureRectange) {
  if (!gl_.GetCapabilities().texture_rectangle) {
    LOG(INFO) << "GL_ARB_texture_rectangle not supported, skipping test";
    return;
  }
  constexpr const char kFragmentShader[] =
      "#extension GL_ARB_texture_rectangle : enable\n"
      "uniform sampler2DRect sampler;\n"
      "void main() {\n"
      "  gl_FragColor = texture2DRect(sampler, vec2(0.0, 0.0));\n"
      "}\n";
  GLuint program = MakeProgram(kFragmentShader);
  ASSERT_TRUE(program);
  GLuint texture = MakeUninitializedTexture(GL_TEXTURE_RECTANGLE_ARB);

  glDrawArrays(GL_TRIANGLES, 0, 3);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  glDeleteTextures(1, &texture);
  glDeleteProgram(program);
}

}  // namespace gpu

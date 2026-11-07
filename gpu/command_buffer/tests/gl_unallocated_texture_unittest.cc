// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2chromium.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_service_helper.h"
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

class GLUnclearedSharedImageTest : public testing::Test {
 protected:
  static constexpr GLsizei kSize = 4;

  void SetUp() override {
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_WEBGL2;
    options.size = gfx::Size(kSize, kSize);
    gl_.Initialize(options);
  }

  void TearDown() override { gl_.Destroy(); }

  bool IsApplicable() const {
    return gl_.IsInitialized() &&
           gl_.gpu_preferences().use_passthrough_cmd_decoder;
  }

  void DrawQuadSamplingTexture(GLuint texture) {
    constexpr const char kVertexShader[] =
        "attribute vec4 a_position;\n"
        "void main() { gl_Position = a_position; }\n";
    constexpr const char kFragmentShader[] =
        "precision mediump float;\n"
        "uniform sampler2D u_sampler;\n"
        "void main() { gl_FragColor = texture2D(u_sampler, vec2(0.5, 0.5)); "
        "}\n";
    GLuint program = GLTestHelper::LoadProgram(kVertexShader, kFragmentShader);
    ASSERT_NE(0u, program);
    glUseProgram(program);

    GLint position_loc = glGetAttribLocation(program, "a_position");
    GLuint vbo = GLTestHelper::SetupUnitQuad(position_loc);
    GLint sampler_loc = glGetUniformLocation(program, "u_sampler");
    glUniform1i(sampler_loc, 0);

    // Clear the destination framebuffer to green first so that we verify the
    // draw call overwrites it with the sampled texture color.
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    glDeleteBuffers(1, &vbo);
    glDeleteProgram(program);
  }

  GLManager gl_;
};

// Verify that importing an uncleared RGBA_F16 SharedImage into a WebGL2 context
// without GL_EXT_color_buffer_float fails with GL_INVALID_OPERATION and maps
// the client texture ID to an unallocated placeholder texture that samples as
// opaque black (0, 0, 0, 255) rather than exposing the uninitialized backing.
TEST_F(GLUnclearedSharedImageTest,
       RenderUnclearedHalfFloatSharedImageWithoutColorBufferFloat) {
  if (!IsApplicable()) {
    GTEST_SKIP() << "Passthrough WebGL2 context not available.";
  }

  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      RegisterDirtyUnclearedHalfFloatSharedImage(&gl_, &memory_tracker, mailbox,
                                                 kSize, kSize);
  EXPECT_FALSE(shared_image->IsCleared());

  GLuint texture =
      gl_.gles2_implementation()->CreateAndTexStorage2DSharedImageCHROMIUM(
          mailbox.name);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_FALSE(shared_image->IsCleared());

  DrawQuadSamplingTexture(texture);
  constexpr uint8_t kOpaqueBlack[] = {0, 0, 0, 255};
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, kSize, kSize, 0, kOpaqueBlack, nullptr));

  glDeleteTextures(1, &texture);
  shared_image.reset();
}

// Verify that once GL_EXT_color_buffer_float is enabled on the WebGL2 context,
// importing the uncleared RGBA_F16 SharedImage lazily clears its dirty backing
// memory and samples as transparent black (0, 0, 0, 0).
TEST_F(GLUnclearedSharedImageTest,
       RenderUnclearedHalfFloatSharedImageWithColorBufferFloat) {
  if (!IsApplicable()) {
    GTEST_SKIP() << "Passthrough WebGL2 context not available.";
  }
  std::string_view requestable_extensions(
      reinterpret_cast<const char*>(glGetRequestableExtensionsCHROMIUM()));
  if (!requestable_extensions.contains("GL_EXT_color_buffer_float")) {
    GTEST_SKIP() << "GL_EXT_color_buffer_float not supported.";
  }

  glRequestExtensionCHROMIUM("GL_EXT_color_buffer_float");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      RegisterDirtyUnclearedHalfFloatSharedImage(&gl_, &memory_tracker, mailbox,
                                                 kSize, kSize);
  EXPECT_FALSE(shared_image->IsCleared());

  GLuint texture =
      gl_.gles2_implementation()->CreateAndTexStorage2DSharedImageCHROMIUM(
          mailbox.name);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_TRUE(shared_image->IsCleared());

  gl_.gles2_implementation()->BeginSharedImageAccessDirectCHROMIUM(
      texture, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  DrawQuadSamplingTexture(texture);
  constexpr uint8_t kTransparentBlack[] = {0, 0, 0, 0};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kSize, kSize, 0,
                                        kTransparentBlack, nullptr));

  gl_.gles2_implementation()->EndSharedImageAccessDirectCHROMIUM(texture);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  glDeleteTextures(1, &texture);
  shared_image.reset();
}

}  // namespace gpu

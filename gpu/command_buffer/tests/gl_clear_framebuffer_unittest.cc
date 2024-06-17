// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include <vector>

#include "build/build_config.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_context.h"

namespace gpu {

// A collection of tests that exercise the glClear workaround.
class GLClearFramebufferTest : public testing::TestWithParam<bool> {
 public:
  GLClearFramebufferTest() : color_handle_(0u), depth_handle_(0u) {}

 protected:
  void SetUp() override {
    if (GetParam()) {
      // Force the glClear() workaround so we can test it here.
      GpuDriverBugWorkarounds workarounds;
      workarounds.gl_clear_broken = true;
      gl_.InitializeWithWorkarounds(GLManager::Options(), workarounds);
      DCHECK(gl_.workarounds().gl_clear_broken);
    } else {
      gl_.Initialize(GLManager::Options());
      DCHECK(!gl_.workarounds().gl_clear_broken);
    }
  }

  void InitDraw();
  void SetDrawColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
  void SetDrawDepth(GLfloat depth);
  void DrawQuad();

  void TearDown() override {
    GLTestHelper::CheckGLError("no errors", __LINE__);
    gl_.Destroy();
  }

 protected:
  GLManager gl_;
  GLuint color_handle_;
  GLuint depth_handle_;
};

void GLClearFramebufferTest::InitDraw() {
  static const char* v_shader_str =
      "attribute vec4 a_Position;\n"
      "uniform float u_depth;\n"
      "void main()\n"
      "{\n"
      "   gl_Position = a_Position;\n"
      "   gl_Position.z = u_depth;\n"
      "}\n";
  static const char* f_shader_str =
      "precision mediump float;\n"
      "uniform vec4 u_draw_color;\n"
      "void main()\n"
      "{\n"
      "  gl_FragColor = u_draw_color;\n"
      "}\n";

  GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);
  DCHECK(program);
  glUseProgram(program);
  GLuint position_loc = glGetAttribLocation(program, "a_Position");

  GLTestHelper::SetupUnitQuad(position_loc);
  color_handle_ = glGetUniformLocation(program, "u_draw_color");
  DCHECK(color_handle_ != static_cast<GLuint>(-1));
  depth_handle_ = glGetUniformLocation(program, "u_depth");
  DCHECK(depth_handle_ != static_cast<GLuint>(-1));
}

void GLClearFramebufferTest::SetDrawColor(GLfloat r,
                                          GLfloat g,
                                          GLfloat b,
                                          GLfloat a) {
  glUniform4f(color_handle_, r, g, b, a);
}

void GLClearFramebufferTest::SetDrawDepth(GLfloat depth) {
  glUniform1f(depth_handle_, depth);
}

void GLClearFramebufferTest::DrawQuad() {
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

INSTANTIATE_TEST_SUITE_P(GLClearFramebufferTestWithParam,
                         GLClearFramebufferTest,
                         ::testing::Values(true, false));

TEST_P(GLClearFramebufferTest, ClearColor) {
  glClearColor(1.0f, 0.5f, 0.25f, 0.5f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Verify.
  const uint8_t expected[] = {255, 128, 64, 128};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 1 /* tolerance */, expected,
                                        nullptr));
}

TEST_P(GLClearFramebufferTest, ClearColorWithMask) {
  glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Verify.
  const uint8_t expected[] = {255, 0, 0, 0};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, expected,
                                        nullptr));
}

// crbug.com/434094
#if !BUILDFLAG(IS_MAC)
TEST_P(GLClearFramebufferTest, ClearColorWithScissor) {
  // TODO(jonahr): Test fails on Linux with ANGLE/passthrough
  // (crbug.com/1099770)
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("linux passthrough")) {
    return;
  }

  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Verify.
  const uint8_t expected[] = {255, 255, 255, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, expected,
                                        nullptr));

  glScissor(0, 0, 0, 0);
  glEnable(GL_SCISSOR_TEST);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  // Verify - no changes.
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, expected,
                                        nullptr));
}
#endif

TEST_P(GLClearFramebufferTest, ClearDepthStencil) {
  // TODO(kainino): https://crbug.com/782317
  if (GPUTestBotConfig::CurrentConfigMatches("Intel")) {
    return;
  }

  const GLuint kStencilRef = 1 << 2;
  InitDraw();
  SetDrawColor(1.0f, 0.0f, 0.0f, 1.0f);
  DrawQuad();
  // Verify.
  const uint8_t kRed[] = {255, 0, 0, 255};
  const uint8_t kGreen[] = {0, 255, 0, 255};
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kRed, nullptr));

  glClearStencil(kStencilRef);
  glClear(GL_STENCIL_BUFFER_BIT);
  glEnable(GL_STENCIL_TEST);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilFunc(GL_NOTEQUAL, kStencilRef, 0xFFFFFFFF);

  SetDrawColor(0.0f, 1.0f, 0.0f, 1.0f);
  DrawQuad();
  // Verify - stencil should have failed, so still red.
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kRed, nullptr));

  glStencilFunc(GL_EQUAL, kStencilRef, 0xFFFFFFFF);
  DrawQuad();
  // Verify - stencil should have passed, so green.
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kGreen,
                                        nullptr));

  glEnable(GL_DEPTH_TEST);
  glClearDepthf(0.0f);
  glClear(GL_DEPTH_BUFFER_BIT);

  SetDrawDepth(0.5f);
  SetDrawColor(1.0f, 0.0f, 0.0f, 1.0f);
  DrawQuad();
  // Verify - depth test should have failed, so still green.
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kGreen,
                                        nullptr));

  glClearDepthf(0.9f);
  glClear(GL_DEPTH_BUFFER_BIT);
  DrawQuad();
  // Verify - depth test should have passed, so red.
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kRed, nullptr));
}

TEST_P(GLClearFramebufferTest, SeparateFramebufferClear) {
  const char* extension_string =
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  gfx::ExtensionSet extensions = gfx::MakeExtensionSet(extension_string);
  bool has_separate_framebuffer =
      gfx::HasExtension(extensions, "GL_CHROMIUM_framebuffer_multisample");
  if (!has_separate_framebuffer) {
    return;
  }

  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Bind incomplete read framebuffer, should not affect clear.
  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
  EXPECT_NE(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER),
            static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE));

  glClearColor(1.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  gl_.BindOffscreenFramebuffer(GL_READ_FRAMEBUFFER);
  const uint8_t kRed[] = {255, 0, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kRed, nullptr));

  // Bind complete, but smaller read framebuffer, should not affect clear.
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, texture, 0);
  EXPECT_EQ(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER),
            static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE));

  glClearColor(0.f, 1.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  gl_.BindOffscreenFramebuffer(GL_READ_FRAMEBUFFER);
  const uint8_t kGreen[] = {0, 255, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(3, 3, 1, 1, 0, kGreen, nullptr));
}

class ES3ClearBufferTest : public testing::Test {
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

  void TearDown() override {
    gl_.Destroy();
  }

  GLManager gl_;
};

TEST_F(ES3ClearBufferTest, ClearBuffersuiv) {
  if (ShouldSkipTest())
    return;

  // This is a regression test for https://crbug.com/908749
  GLuint value[1] = {0u};
  glClearBufferuiv(GL_STENCIL, 0, value);
  // The above call should not crash in ASAN build.
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), glGetError());
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

}  // namespace gpu

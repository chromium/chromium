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
  virtual GLManager::Options GetGlManagerOptions() {
    return GLManager::Options();
  }

  void SetUp() override {
    if (GetParam()) {
      // Force the glClear() workaround so we can test it here.
      GpuDriverBugWorkarounds workarounds;
      workarounds.gl_clear_broken = true;
      gl_.InitializeWithWorkarounds(GetGlManagerOptions(), workarounds);
      DCHECK(gl_.workarounds().gl_clear_broken);
    } else {
      gl_.Initialize(GetGlManagerOptions());
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

class ES3ClearBufferTest : public GLClearFramebufferTest {
 protected:
  static const GLsizei kCanvasSize = 4;

  GLManager::Options GetGlManagerOptions() override {
    GLManager::Options options;
    options.size = gfx::Size(kCanvasSize, kCanvasSize);
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    return options;
  }

  bool ShouldSkipTest() const {
    // If a driver isn't capable of supporting ES3 context, creating
    // ContextGroup will fail.
    // See crbug.com/654709.
    return (!gl_.decoder() || !gl_.decoder()->GetContextGroup());
  }
};

INSTANTIATE_TEST_SUITE_P(ES3ClearBufferTestWithParam,
                         ES3ClearBufferTest,
                         ::testing::Values(true, false));

TEST_P(ES3ClearBufferTest, ClearBuffersuiv) {
  if (ShouldSkipTest())
    return;

  // This is a regression test for https://crbug.com/908749
  GLuint value[1] = {0u};
  glClearBufferuiv(GL_STENCIL, 0, value);
  // The above call should not crash in ASAN build.
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), glGetError());
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_P(ES3ClearBufferTest, RasterizerDiscardIntegerClearBypass) {
  if (ShouldSkipTest()) {
    return;
  }

  const GLsizei kDirtyWidth = 256;
  const GLsizei kDirtyHeight = 256;

  // Step 0: Dirty VRAM using a separate context.
  {
    GLManager gl2;
    gl2.Initialize(GetGlManagerOptions());
    gl2.MakeCurrent();

    for (int i = 0; i < 8; ++i) {
      GLuint rb = 0;
      glGenRenderbuffers(1, &rb);
      glBindRenderbuffer(GL_RENDERBUFFER, rb);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kDirtyWidth,
                            kDirtyHeight);
      GLuint fb = 0;
      glGenFramebuffers(1, &fb);
      glBindFramebuffer(GL_FRAMEBUFFER, fb);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_RENDERBUFFER, rb);
      EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
                glCheckFramebufferStatus(GL_FRAMEBUFFER));

      glClearColor(0.8f, 0.2f, 0.6f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      glDeleteFramebuffers(1, &fb);
      glDeleteRenderbuffers(1, &rb);
    }
    glFinish();
    gl2.Destroy();
  }

  // Restore main context.
  gl_.MakeCurrent();

  // Step 1: Trigger the bug.
  GLuint rb = 0;
  glGenRenderbuffers(1, &rb);
  glBindRenderbuffer(GL_RENDERBUFFER, rb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8UI, kDirtyWidth, kDirtyHeight);

  GLuint fb = 0;
  glGenFramebuffers(1, &fb);
  glBindFramebuffer(GL_FRAMEBUFFER, fb);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, rb);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  glEnable(GL_RASTERIZER_DISCARD);
  ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // readPixels to trigger lazy clear.
  std::vector<GLuint> pixels(kDirtyWidth * kDirtyHeight * 4, 0xAAAAAAAAu);
  glReadPixels(0, 0, kDirtyWidth, kDirtyHeight, GL_RGBA_INTEGER,
               GL_UNSIGNED_INT, pixels.data());

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Inspect results.
  uint32_t nonzero_components = 0;
  for (GLuint val : pixels) {
    if (val != 0) {
      nonzero_components++;
    }
  }

  // If bug is present, we expect non-zero components (leak from dirty VRAM).
  // If fixed, we expect ALL zero.
  EXPECT_EQ(0u, nonzero_components);

  // Cleanup.
  glDisable(GL_RASTERIZER_DISCARD);
  glDeleteFramebuffers(1, &fb);
  glDeleteRenderbuffers(1, &rb);
}

class GLReattachFboDepthStencilTest : public GLClearFramebufferTest {
 protected:
  void SetUp() override {
    GpuDriverBugWorkarounds workarounds;
    if (GetParam()) {
      workarounds.reattach_fbo_depth_stencil_on_reallocation = true;
    }
    gl_.InitializeWithWorkarounds(GetGlManagerOptions(), workarounds);
  }

  void RunReattachFboDepthStencilTest(bool use_texture,
                                      bool bind_fbo_during_reallocation);
};

INSTANTIATE_TEST_SUITE_P(GLReattachFboDepthStencilTestWithParam,
                         GLReattachFboDepthStencilTest,
                         ::testing::Bool());

void GLReattachFboDepthStencilTest::RunReattachFboDepthStencilTest(
    bool use_texture,
    bool bind_fbo_during_reallocation) {
  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  GLuint color_tex = 0;
  glGenTextures(1, &color_tex);
  glBindTexture(GL_TEXTURE_2D, color_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color_tex, 0);

  GLuint depth_obj = 0;
  if (use_texture) {
    glGenTextures(1, &depth_obj);
    glBindTexture(GL_TEXTURE_2D, depth_obj);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 16, 16, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           depth_obj, 0);
  } else {
    glGenRenderbuffers(1, &depth_obj);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_obj);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 16, 16);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depth_obj);
  }

  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  // Clear color to Green, depth to 1.0 (far).
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClearDepthf(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const uint8_t kGreen[] = {0, 255, 0, 255};
  const uint8_t kRed[] = {255, 0, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kGreen, nullptr));

  // Enable depth test. Draw Red quad at depth 0.5. Should pass (0.5 < 1.0).
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  InitDraw();
  SetDrawDepth(0.5f);
  SetDrawColor(1.0f, 0.0f, 0.0f, 1.0f);
  DrawQuad();

  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kRed, nullptr));

  // Clear color to Green, depth to 0.0 (near).
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClearDepthf(0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kGreen, nullptr));

  // Draw Red quad at depth 0.5. Should fail (0.5 is not < 0.0).
  DrawQuad();
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kGreen, nullptr));

  if (!bind_fbo_during_reallocation) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // Redefine depth attachment (reallocate to 16x16).
  if (use_texture) {
    glBindTexture(GL_TEXTURE_2D, depth_obj);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 16, 16, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
  } else {
    glBindRenderbuffer(GL_RENDERBUFFER, depth_obj);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 16, 16);
  }

  if (!bind_fbo_during_reallocation) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  }

  // Clear depth to 1.0.
  glClearDepthf(1.0f);
  glClear(GL_DEPTH_BUFFER_BIT);

  // Draw Red quad at depth 0.5. Should pass (0.5 < 1.0).
  SetDrawDepth(0.5f);
  SetDrawColor(1.0f, 0.0f, 0.0f, 1.0f);
  DrawQuad();

  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kRed, nullptr));

  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &color_tex);
  if (use_texture) {
    glDeleteTextures(1, &depth_obj);
  } else {
    glDeleteRenderbuffers(1, &depth_obj);
  }
}

TEST_P(GLReattachFboDepthStencilTest, TextureReallocationBound) {
  RunReattachFboDepthStencilTest(true, true);
}

TEST_P(GLReattachFboDepthStencilTest, TextureReallocationUnbound) {
  RunReattachFboDepthStencilTest(true, false);
}

TEST_P(GLReattachFboDepthStencilTest, RenderbufferReallocationBound) {
  RunReattachFboDepthStencilTest(false, true);
}

TEST_P(GLReattachFboDepthStencilTest, RenderbufferReallocationUnbound) {
  RunReattachFboDepthStencilTest(false, false);
}

class ES3ReattachFboDepthStencilTest : public GLReattachFboDepthStencilTest {
 protected:
  GLManager::Options GetGlManagerOptions() override {
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    return options;
  }

  bool ShouldSkipTest() const {
    return (!gl_.decoder() || !gl_.decoder()->GetContextGroup());
  }

  void RunTexImage2DToTexStorage2DTest(bool bind_fbo_during_reallocation);
};

INSTANTIATE_TEST_SUITE_P(ES3ReattachFboDepthStencilTestWithParam,
                         ES3ReattachFboDepthStencilTest,
                         ::testing::Bool());

void ES3ReattachFboDepthStencilTest::RunTexImage2DToTexStorage2DTest(
    bool bind_fbo_during_reallocation) {
  if (ShouldSkipTest()) {
    return;
  }

  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  GLuint color_tex = 0;
  glGenTextures(1, &color_tex);
  glBindTexture(GL_TEXTURE_2D, color_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color_tex, 0);

  GLuint depth_tex = 0;
  glGenTextures(1, &depth_tex);
  glBindTexture(GL_TEXTURE_2D, depth_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 16, 16, 0,
               GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depth_tex, 0);

  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  // Clear color to Green, depth to 1.0 (far).
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClearDepthf(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const uint8_t kGreen[] = {0, 255, 0, 255};
  const uint8_t kRed[] = {255, 0, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kGreen, nullptr));

  // Enable depth test. Draw Red quad at depth 0.5. Should pass.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  InitDraw();
  SetDrawDepth(0.5f);
  SetDrawColor(1.0f, 0.0f, 0.0f, 1.0f);
  DrawQuad();

  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kRed, nullptr));

  // Clear color to Green, depth to 0.0 (near).
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClearDepthf(0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kGreen, nullptr));

  // Draw Red quad at depth 0.5. Should fail.
  DrawQuad();
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kGreen, nullptr));

  if (!bind_fbo_during_reallocation) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // Redefine depth attachment via glTexStorage2D.
  glBindTexture(GL_TEXTURE_2D, depth_tex);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 16, 16);

  if (!bind_fbo_during_reallocation) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  }

  // Clear depth to 1.0.
  glClearDepthf(1.0f);
  glClear(GL_DEPTH_BUFFER_BIT);

  // Draw Red quad at depth 0.5. Should pass.
  SetDrawDepth(0.5f);
  SetDrawColor(1.0f, 0.0f, 0.0f, 1.0f);
  DrawQuad();

  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, kRed, nullptr));

  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &color_tex);
  glDeleteTextures(1, &depth_tex);
}

TEST_P(ES3ReattachFboDepthStencilTest, TexImage2DToTexStorage2DBound) {
  RunTexImage2DToTexStorage2DTest(true);
}

TEST_P(ES3ReattachFboDepthStencilTest, TexImage2DToTexStorage2DUnbound) {
  RunTexImage2DToTexStorage2DTest(false);
}

}  // namespace gpu

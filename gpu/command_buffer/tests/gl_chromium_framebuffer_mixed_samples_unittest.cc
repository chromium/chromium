// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/command_line.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER(Src) #Src

namespace gpu {

class CHROMIUMFramebufferMixedSamplesTest : public testing::Test {
 protected:
  const GLuint kWidth = 100;
  const GLuint kHeight = 100;

  void SetUp() override {
    gl_.Initialize(GLManager::Options());
  }

  void TearDown() override { gl_.Destroy(); }

  bool IsApplicable() const {
    return GLTestHelper::HasExtension(
               "GL_CHROMIUM_framebuffer_mixed_samples") &&
           GLTestHelper::HasExtension("GL_OES_rgb8_rgba8");
  }

  enum SetupFBOType {
    MixedSampleFBO,   // 1 color sample, N stencil samples.
    SingleSampleFBO,  // 1 color sample, 1 stencil sample.
  };

  void SetupState(SetupFBOType fbo_type) {
    // clang-format off
    static const char* kVertexShaderSource = SHADER(
        attribute mediump vec4 position;
        void main() {
          gl_Position = position;
        });
    static const char* kFragmentShaderSource = SHADER(
        uniform mediump vec4 color;
        void main() {
          gl_FragColor = color;
        });
    // clang-format on
    GLuint program =
        GLTestHelper::LoadProgram(kVertexShaderSource, kFragmentShaderSource);
    glUseProgram(program);
    GLuint position_loc = glGetAttribLocation(program, "position");
    color_loc_ = glGetUniformLocation(program, "color");

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    static float vertices[] = {
        1.0f,  1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f, 1.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(position_loc);
    glVertexAttribPointer(position_loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLuint texture = 0;
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint stencil_rb = 0;
    glGenRenderbuffers(1, &stencil_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, stencil_rb);

    if (fbo_type == MixedSampleFBO) {
      // Create a sample buffer.
      GLsizei num_samples = 8, max_samples = 0;
      glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
      num_samples = std::min(num_samples, max_samples);
      glRenderbufferStorageMultisampleCHROMIUM(
          GL_RENDERBUFFER, num_samples, GL_STENCIL_INDEX8, kWidth, kHeight);
      GLint param = 0;
      glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES,
                                   &param);
      EXPECT_GT(param, 1);
    } else {
      glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, kWidth,
                            kHeight);
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    GLuint sample_fbo = 0;
    glGenFramebuffers(1, &sample_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, sample_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, stencil_rb);
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glViewport(0, 0, kWidth, kHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearStencil(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glStencilMask(0xffffffff);
    glStencilFunc(GL_EQUAL, 1, 0xffffffff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
  }

  GLManager gl_;
  GLint color_loc_;
};

TEST_F(CHROMIUMFramebufferMixedSamplesTest, Simple) {
  if (!IsApplicable()) {
    return;
  }
  GLint value = -1;
  glGetIntegerv(GL_COVERAGE_MODULATION_CHROMIUM, &value);
  EXPECT_EQ(0, value);
  GLenum kValues[] = {GL_NONE, GL_RGB, GL_RGBA, GL_ALPHA};
  for (auto expect : kValues) {
    glCoverageModulationCHROMIUM(expect);
    value = -1;
    glGetIntegerv(GL_COVERAGE_MODULATION_CHROMIUM, &value);
    EXPECT_EQ(expect, static_cast<GLenum>(value));
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }

  glCoverageModulationCHROMIUM(GL_BYTE);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), glGetError());
  value = -1;
  glGetIntegerv(GL_COVERAGE_MODULATION_CHROMIUM, &value);
  EXPECT_EQ(static_cast<GLenum>(GL_ALPHA), static_cast<GLenum>(value));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

// The test pattern is as follows:
// A green triangle (bottom left, top right, top left).
// A blue triangle (top left, bottom right, bottom left).
// The triangles will overlap but overlap only contains green pixels,
// due to each draw erasing its area from stencil.
// The blue triangle will fill only the area (bottom left, center,
// bottom right).

// The test tests that CoverageModulation call works.
// The fractional pixels of both triangles end up being modulated
// by the coverage of the fragment. Test that drawing with and without
// CoverageModulation causes the result to be different.
TEST_F(CHROMIUMFramebufferMixedSamplesTest, CoverageModulation) {
  if (!IsApplicable()) {
    return;
  }
  static const float kBlue[] = {0.0f, 0.0f, 1.0f, 1.0f};
  static const float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};
  std::unique_ptr<uint8_t[]> results[3];
  const GLint kResultSize = kWidth * kHeight * 4;

  for (int pass = 0; pass < 3; ++pass) {
    SetupState(MixedSampleFBO);
    if (pass == 1) {
      glCoverageModulationCHROMIUM(GL_RGBA);
    }
    glUniform4fv(color_loc_, 1, kGreen);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUniform4fv(color_loc_, 1, kBlue);
    glDrawArrays(GL_TRIANGLES, 3, 3);
    if (pass == 1) {
      glCoverageModulationCHROMIUM(GL_NONE);
    }

    results[pass].reset(new uint8_t[kResultSize]);
    memset(results[pass].get(), GLTestHelper::kCheckClearValue, kResultSize);
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 results[pass].get());
  }

  EXPECT_NE(0, memcmp(results[0].get(), results[1].get(), kResultSize));
  // Verify that rendering is deterministic, so that the pass above does not
  // come from non-deterministic rendering.
  EXPECT_EQ(0, memcmp(results[0].get(), results[2].get(), kResultSize));
}

// The test tests that the stencil buffer can be multisampled, even though the
// color buffer is single-sampled. Draws the same pattern with single-sample
// stencil buffer and with multisample stencil buffer. The images should differ.
TEST_F(CHROMIUMFramebufferMixedSamplesTest, MultisampleStencilEffective) {
  if (!IsApplicable()) {
    return;
  }

  static const float kBlue[] = {0.0f, 0.0f, 1.0f, 1.0f};
  static const float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};

  std::unique_ptr<uint8_t[]> results[3];
  const GLint kResultSize = kWidth * kHeight * 4;

  for (int pass = 0; pass < 3; ++pass) {
    if (pass == 1) {
      SetupState(MixedSampleFBO);
    } else {
      SetupState(SingleSampleFBO);
    }
    glUniform4fv(color_loc_, 1, kGreen);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUniform4fv(color_loc_, 1, kBlue);
    glDrawArrays(GL_TRIANGLES, 3, 3);

    results[pass].reset(new uint8_t[kResultSize]);
    memset(results[pass].get(), GLTestHelper::kCheckClearValue, kResultSize);
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 results[pass].get());
  }

  EXPECT_NE(0, memcmp(results[0].get(), results[1].get(), kResultSize));
  // Verify that rendering is deterministic, so that the pass above does not
  // come from non-deterministic rendering.
  EXPECT_EQ(0, memcmp(results[0].get(), results[2].get(), kResultSize));
}

}  // namespace gpu

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include <vector>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

void BlitTexture(GLuint program) {
  glUseProgram(program);
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

  GLuint sampler_handle = glGetUniformLocation(program, "u_sampler");
  glUniform1i(sampler_handle, 0);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  glDisableVertexAttribArray(kVertexPositionAttrib);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &buffer_id);
}

}  // namespace

class GLNPOTHostTwiddledTextureTest : public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    options.size = gfx::Size(256, 256);

    GpuDriverBugWorkarounds workarounds;
    workarounds.use_tex_sub_image_for_host_twiddled_npot_uploads = GetParam();
    gl_.InitializeWithWorkarounds(options, workarounds);

    if (!IsApplicable()) {
      return;
    }

    gl_.MakeCurrent();

    static const char* kVertexShader =
        "attribute vec2 a_position;\n"
        "varying mediump vec2 v_uv;\n"
        "void main(void) {\n"
        "  gl_Position = vec4(a_position, 0, 1);\n"
        "  v_uv = 0.5 * (a_position + vec2(1, 1));\n"
        "}\n";
    static const char* kFragmentShader =
        "precision mediump float;\n"
        "uniform sampler2D u_sampler;\n"
        "varying mediump vec2 v_uv;\n"
        "void main(void) {\n"
        "  gl_FragColor = texture2D(u_sampler, v_uv);\n"
        "}\n";

    program_ = GLTestHelper::LoadProgram(kVertexShader, kFragmentShader);
    ASSERT_NE(0u, program_);
    glBindAttribLocation(program_, 0, "a_position");
    glLinkProgram(program_);

    glGenFramebuffers(1, &fbo_);
  }

  void TearDown() override {
    if (IsApplicable()) {
      if (program_) {
        glDeleteProgram(program_);
      }
      if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
      }
    }
    gl_.Destroy();
  }

  bool IsApplicable() const { return gl_.IsInitialized(); }

  GLManager gl_;
  GLuint program_ = 0;
  GLuint fbo_ = 0;
};

INSTANTIATE_TEST_SUITE_P(GLNPOTHostTwiddledTextureTests,
                         GLNPOTHostTwiddledTextureTest,
                         ::testing::Bool());

// Test that non-power-of-two uploads of RGB10_A2 with
// UNSIGNED_INT_2_10_10_10_REV data succeed and verify texture contents.
TEST_P(GLNPOTHostTwiddledTextureTest, RGB10A2) {
  if (!IsApplicable()) {
    return;
  }

  constexpr GLsizei kWidth = 65;
  constexpr GLsizei kHeight = 64;

  for (int iter = 0; iter < 50; ++iter) {
    std::vector<uint32_t> data(kWidth * kHeight);
    for (GLsizei y = 0; y < kHeight; ++y) {
      for (GLsizei x = 0; x < kWidth; ++x) {
        uint32_t r = ((x + iter) % 2 == 0) ? 0x3FF : 0;
        uint32_t g = ((y + iter) % 2 == 0) ? 0x3FF : 0;
        uint32_t b = ((x + y + iter) % 2 == 0) ? 0x3FF : 0;
        uint32_t a = 3;  // 1.0 in 2-bit alpha
        data[y * kWidth + x] = (r & 0x3FF) | ((g & 0x3FF) << 10) |
                               ((b & 0x3FF) << 20) | ((a & 0x3) << 30);
      }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, kWidth, kHeight, 0, GL_RGBA,
                 GL_UNSIGNED_INT_2_10_10_10_REV, data.data());
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    // Verify texture contents via FBO readback
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex, 0);
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatus(GL_FRAMEBUFFER));

    std::vector<uint8_t> readback(kWidth * kHeight * 4);
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 readback.data());
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    for (GLsizei y = 0; y < kHeight; ++y) {
      for (GLsizei x = 0; x < kWidth; ++x) {
        uint8_t expected_r = ((x + iter) % 2 == 0) ? 255 : 0;
        uint8_t expected_g = ((y + iter) % 2 == 0) ? 255 : 0;
        uint8_t expected_b = ((x + y + iter) % 2 == 0) ? 255 : 0;
        uint8_t expected_a = 255;
        size_t idx = (y * kWidth + x) * 4;
        EXPECT_EQ(readback[idx], expected_r)
            << "Mismatch at (" << x << ", " << y << ") on iter " << iter;
        EXPECT_EQ(readback[idx + 1], expected_g)
            << "Mismatch at (" << x << ", " << y << ") on iter " << iter;
        EXPECT_EQ(readback[idx + 2], expected_b)
            << "Mismatch at (" << x << ", " << y << ") on iter " << iter;
        EXPECT_EQ(readback[idx + 3], expected_a)
            << "Mismatch at (" << x << ", " << y << ") on iter " << iter;
      }
    }

    // Also verify sampling via BlitTexture
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 256, 256);
    BlitTexture(program_);

    glDeleteTextures(1, &tex);
  }
}

// Test that non-power-of-two uploads of SRGB8_ALPHA8 with UNSIGNED_BYTE data
// succeed and verify texture contents, including non-default unpack alignment
// and skip pixels.
TEST_P(GLNPOTHostTwiddledTextureTest, SRGB8Alpha8) {
  if (!IsApplicable()) {
    return;
  }

  constexpr GLsizei kWidth = 255;
  constexpr GLsizei kHeight = 256;
  constexpr GLint kSkipPixels = 1;
  constexpr GLsizei kRowLength = kWidth + kSkipPixels;

  for (int iter = 0; iter < 50; ++iter) {
    std::vector<uint8_t> data(kHeight * kRowLength * 4);
    for (size_t i = 0; i < data.size() / 4; ++i) {
      data[i * 4] = static_cast<uint8_t>((i * 17 + iter * 3) % 256);
      data[i * 4 + 1] = static_cast<uint8_t>((i * 31 + iter * 5 + 7) % 256);
      data[i * 4 + 2] = static_cast<uint8_t>((i * 53 + iter * 11 + 13) % 256);
      data[i * 4 + 3] = 255;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, kRowLength);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, kSkipPixels);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, kWidth, kHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data.data());
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

    // Verify texture contents via FBO readback
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex, 0);
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatus(GL_FRAMEBUFFER));

    std::vector<uint8_t> readback(kWidth * kHeight * 4);
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 readback.data());
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    for (GLsizei y = 0; y < kHeight; ++y) {
      for (GLsizei x = 0; x < kWidth; ++x) {
        size_t src_idx = (y * kRowLength + x + kSkipPixels) * 4;
        size_t dst_idx = (y * kWidth + x) * 4;
        EXPECT_EQ(readback[dst_idx], data[src_idx])
            << "Mismatch at (" << x << ", " << y << ") on iter " << iter;
        EXPECT_EQ(readback[dst_idx + 1], data[src_idx + 1])
            << "Mismatch at (" << x << ", " << y << ") on iter " << iter;
        EXPECT_EQ(readback[dst_idx + 2], data[src_idx + 2])
            << "Mismatch at (" << x << ", " << y << ") on iter " << iter;
        EXPECT_EQ(readback[dst_idx + 3], data[src_idx + 3])
            << "Mismatch at (" << x << ", " << y << ") on iter " << iter;
      }
    }

    // Also verify sampling via BlitTexture
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 256, 256);
    BlitTexture(program_);

    glDeleteTextures(1, &tex);
  }
}

}  // namespace gpu

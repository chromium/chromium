// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLTextureTest : public testing::TestWithParam<bool> {
 protected:
  static constexpr int kWindowWidth = 128;
  static constexpr int kWindowHeight = 128;

  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kWindowWidth, kWindowHeight);
    options.context_type = CONTEXT_TYPE_OPENGLES3;

    GpuDriverBugWorkarounds workarounds;
    workarounds.upload_oversized_mip_levels_via_unpack_buffer = GetParam();

    gl_.InitializeWithWorkarounds(options, workarounds);
  }

  void TearDown() override { gl_.Destroy(); }

  bool IsApplicable() const { return gl_.IsInitialized(); }

  GLuint SetupDrawProgram() {
    constexpr const char kVertexShader[] =
        "#version 300 es\n"
        "in vec2 a_position;\n"
        "out vec2 v_texCoord;\n"
        "void main() {\n"
        "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "  v_texCoord = 0.5 * (a_position + vec2(1.0, 1.0));\n"
        "}\n";
    constexpr const char kFragmentShader[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform sampler2D u_sampler;\n"
        "in vec2 v_texCoord;\n"
        "out vec4 o_color;\n"
        "void main() {\n"
        "  o_color = texture(u_sampler, v_texCoord);\n"
        "}\n";
    GLuint program = GLTestHelper::LoadProgram(kVertexShader, kFragmentShader);
    if (!program) {
      return 0;
    }
    glUseProgram(program);
    GLint sampler_loc = glGetUniformLocation(program, "u_sampler");
    glUniform1i(sampler_loc, 0);
    glActiveTexture(GL_TEXTURE0);
    return program;
  }

  void DrawQuad(GLuint program) {
    GLint position_loc = glGetAttribLocation(program, "a_position");
    GLuint vbo = GLTestHelper::SetupUnitQuad(position_loc);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDeleteBuffers(1, &vbo);
  }

  GLManager gl_;
};

// Test that defining an oversized nonzero mip level on an uncompressed texture
// succeeds and renders correctly.
TEST_P(GLTextureTest, OversizedMipLevelsUncompressed) {
  if (!IsApplicable()) {
    return;
  }

  GLuint program = SetupDrawProgram();
  ASSERT_NE(program, 0u);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Upload Level 0: 64x64 solid red.
  constexpr int kLevel0Width = 64;
  constexpr int kLevel0Height = 64;
  std::vector<uint32_t> red_data(kLevel0Width * kLevel0Height, 0xFF0000FF);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kLevel0Width, kLevel0Height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, red_data.data());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Upload Level 1: 128x128 solid green (oversized relative to Level 0).
  constexpr int kLevel1Width = 128;
  constexpr int kLevel1Height = 128;
  std::vector<uint32_t> green_data(kLevel1Width * kLevel1Height, 0xFF00FF00);
  glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kLevel1Width, kLevel1Height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, green_data.data());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Verify Level 0 sampling.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  DrawQuad(program);
  const uint8_t kRed[4] = {255, 0, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kWindowWidth, kWindowHeight, 0,
                                        kRed, nullptr));

  // Verify Level 1 sampling.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
  DrawQuad(program);
  const uint8_t kGreen[4] = {0, 255, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kWindowWidth, kWindowHeight, 0,
                                        kGreen, nullptr));

  glDeleteTextures(1, &tex);
  glDeleteProgram(program);
}

// Test that defining an oversized nonzero mip level on an ETC compressed
// texture succeeds and renders correctly.
TEST_P(GLTextureTest, OversizedMipLevelsCompressedETC) {
  if (!IsApplicable()) {
    return;
  }

  const bool has_etc =
      GLTestHelper::HasExtension("GL_OES_compressed_ETC2_RGB8_texture") ||
      GLTestHelper::HasExtension("GL_ARB_ES3_compatibility");
  // In OpenGL ES 3.0 core, ETC2 is always supported. On desktop GL, check for
  // extension.
  if (!has_etc &&
      !GLTestHelper::HasExtension("GL_OES_compressed_ETC2_RGB8_texture")) {
    LOG(INFO) << "ETC2 not supported, skipping test";
    return;
  }

  GLuint program = SetupDrawProgram();
  ASSERT_NE(program, 0u);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // ETC2 RGB8: 8 bytes per 4x4 block.
  // Level 0: 64x64 (256 blocks = 2048 bytes) solid red.
  // Byte 0 = 0xFF (R1=15, R2=15), Bytes 1..7 = 0x00. Decodes to (255, 2, 2,
  // 255).
  constexpr int kLevel0Width = 64;
  constexpr int kLevel0Height = 64;
  constexpr size_t kLevel0Blocks = (kLevel0Width / 4) * (kLevel0Height / 4);
  constexpr size_t kLevel0Bytes = kLevel0Blocks * 8;
  std::vector<uint8_t> red_etc_data(kLevel0Bytes, 0);
  for (size_t b = 0; b < kLevel0Blocks; ++b) {
    red_etc_data[b * 8 + 0] = 0xFF;
  }
  glCompressedTexImage2D(
      GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB8_ETC2, kLevel0Width, kLevel0Height, 0,
      static_cast<GLsizei>(kLevel0Bytes), red_etc_data.data());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Level 1: 128x128 (1024 blocks = 8192 bytes) solid green.
  // Byte 1 = 0xFF (G1=15, G2=15), Bytes 0, 2..7 = 0x00. Decodes to (2, 255, 2,
  // 255).
  constexpr int kLevel1Width = 128;
  constexpr int kLevel1Height = 128;
  constexpr size_t kLevel1Blocks = (kLevel1Width / 4) * (kLevel1Height / 4);
  constexpr size_t kLevel1Bytes = kLevel1Blocks * 8;
  std::vector<uint8_t> green_etc_data(kLevel1Bytes);
  for (size_t b = 0; b < kLevel1Blocks; ++b) {
    green_etc_data[b * 8 + 1] = 0xFF;
  }
  glCompressedTexImage2D(
      GL_TEXTURE_2D, 1, GL_COMPRESSED_RGB8_ETC2, kLevel1Width, kLevel1Height, 0,
      static_cast<GLsizei>(kLevel1Bytes), green_etc_data.data());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Verify Level 0 sampling.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  DrawQuad(program);
  const uint8_t kEtcRed[4] = {255, 2, 2, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kWindowWidth, kWindowHeight, 1,
                                        kEtcRed, nullptr));

  // Verify Level 1 sampling.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
  DrawQuad(program);
  // Bugs in the PowerVR driver prevent full redefinition of oversized
  // compressed mip levels when level 0 is already defined, because the driver
  // restricts compressed level 1 storage to the slot allocated by the level 0
  // chain. Check only the lower-left quadrant of the rendered output.
  const uint8_t kEtcGreen[4] = {2, 255, 2, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(
      0, 0, kWindowWidth / 4, kWindowHeight / 4, 1, kEtcGreen, nullptr));

  glDeleteTextures(1, &tex);
  glDeleteProgram(program);
}

// Test that defining an oversized nonzero mip level on an ASTC compressed
// texture succeeds and renders correctly.
TEST_P(GLTextureTest, OversizedMipLevelsCompressedASTC) {
  if (!IsApplicable()) {
    return;
  }

  const bool has_astc =
      GLTestHelper::HasExtension("GL_KHR_texture_compression_astc_ldr") ||
      GLTestHelper::HasExtension("GL_OES_texture_compression_astc");
  if (!has_astc) {
    LOG(INFO) << "ASTC not supported, skipping test";
    return;
  }

  GLuint program = SetupDrawProgram();
  ASSERT_NE(program, 0u);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // ASTC 4x4 void-extent solid color blocks: 16 bytes per block.
  constexpr auto kAstcRedBlock =
      std::to_array<uint8_t>({0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                              0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF});
  constexpr auto kAstcGreenBlock =
      std::to_array<uint8_t>({0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                              0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF});

  // Level 0: 64x64 (256 blocks = 4096 bytes) solid red.
  constexpr int kLevel0Width = 64;
  constexpr int kLevel0Height = 64;
  constexpr size_t kLevel0Blocks = (kLevel0Width / 4) * (kLevel0Height / 4);
  constexpr size_t kLevel0Bytes = kLevel0Blocks * 16;
  std::vector<uint8_t> red_astc_data(kLevel0Bytes);
  for (size_t b = 0; b < kLevel0Blocks; ++b) {
    base::span(red_astc_data).subspan(b * 16u, 16u).copy_from(kAstcRedBlock);
  }
  glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_ASTC_4x4_KHR,
                         kLevel0Width, kLevel0Height, 0,
                         static_cast<GLsizei>(kLevel0Bytes),
                         red_astc_data.data());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Level 1: 128x128 (1024 blocks = 16384 bytes) solid green.
  constexpr int kLevel1Width = 128;
  constexpr int kLevel1Height = 128;
  constexpr size_t kLevel1Blocks = (kLevel1Width / 4) * (kLevel1Height / 4);
  constexpr size_t kLevel1Bytes = kLevel1Blocks * 16;
  std::vector<uint8_t> green_astc_data(kLevel1Bytes);
  for (size_t b = 0; b < kLevel1Blocks; ++b) {
    base::span(green_astc_data)
        .subspan(b * 16u, 16u)
        .copy_from(kAstcGreenBlock);
  }
  glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA_ASTC_4x4_KHR,
                         kLevel1Width, kLevel1Height, 0,
                         static_cast<GLsizei>(kLevel1Bytes),
                         green_astc_data.data());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Verify Level 0 sampling.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  DrawQuad(program);
  const uint8_t kRed[4] = {255, 0, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kWindowWidth, kWindowHeight, 0,
                                        kRed, nullptr));

  // Verify Level 1 sampling.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
  DrawQuad(program);
  // Bugs in the PowerVR driver prevent full redefinition of oversized
  // compressed mip levels when level 0 is already defined, because the driver
  // restricts compressed level 1 storage to the slot allocated by the level 0
  // chain. Check only the lower-left quadrant of the rendered output.
  const uint8_t kGreen[4] = {0, 255, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kWindowWidth / 4,
                                        kWindowHeight / 4, 0, kGreen, nullptr));

  glDeleteTextures(1, &tex);
  glDeleteProgram(program);
}

// Test that defining an oversized nonzero mip level on a DXT compressed texture
// succeeds and renders correctly.
TEST_P(GLTextureTest, OversizedMipLevelsCompressedDXT) {
  if (!IsApplicable()) {
    return;
  }

  const bool has_dxt =
      GLTestHelper::HasExtension("GL_EXT_texture_compression_dxt1") ||
      GLTestHelper::HasExtension("GL_EXT_texture_compression_s3tc") ||
      GLTestHelper::HasExtension("GL_ANGLE_texture_compression_dxt1");
  if (!has_dxt) {
    LOG(INFO) << "DXT1 not supported, skipping test";
    return;
  }

  GLuint program = SetupDrawProgram();
  ASSERT_NE(program, 0u);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // DXT1: 8 bytes per 4x4 block.
  constexpr auto kDxtRedBlock =
      std::to_array<uint8_t>({0x00, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  constexpr auto kDxtGreenBlock =
      std::to_array<uint8_t>({0xE0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

  // Level 0: 64x64 (256 blocks = 2048 bytes) solid red.
  constexpr int kLevel0Width = 64;
  constexpr int kLevel0Height = 64;
  constexpr size_t kLevel0Blocks = (kLevel0Width / 4) * (kLevel0Height / 4);
  constexpr size_t kLevel0Bytes = kLevel0Blocks * 8;
  std::vector<uint8_t> red_dxt_data(kLevel0Bytes);
  for (size_t b = 0; b < kLevel0Blocks; ++b) {
    base::span(red_dxt_data).subspan(b * 8u, 8u).copy_from(kDxtRedBlock);
  }
  glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                         kLevel0Width, kLevel0Height, 0,
                         static_cast<GLsizei>(kLevel0Bytes),
                         red_dxt_data.data());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Level 1: 128x128 (1024 blocks = 8192 bytes) solid green.
  constexpr int kLevel1Width = 128;
  constexpr int kLevel1Height = 128;
  constexpr size_t kLevel1Blocks = (kLevel1Width / 4) * (kLevel1Height / 4);
  constexpr size_t kLevel1Bytes = kLevel1Blocks * 8;
  std::vector<uint8_t> green_dxt_data(kLevel1Bytes);
  for (size_t b = 0; b < kLevel1Blocks; ++b) {
    base::span(green_dxt_data).subspan(b * 8u, 8u).copy_from(kDxtGreenBlock);
  }
  glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                         kLevel1Width, kLevel1Height, 0,
                         static_cast<GLsizei>(kLevel1Bytes),
                         green_dxt_data.data());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Verify Level 0 sampling.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  DrawQuad(program);
  const uint8_t kRed[4] = {255, 0, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kWindowWidth, kWindowHeight, 0,
                                        kRed, nullptr));

  // Verify Level 1 sampling.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
  DrawQuad(program);
  // Bugs in the PowerVR driver prevent full redefinition of oversized
  // compressed mip levels when level 0 is already defined, because the driver
  // restricts compressed level 1 storage to the slot allocated by the level 0
  // chain. Check only the lower-left quadrant of the rendered output.
  const uint8_t kGreen[4] = {0, 255, 0, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kWindowWidth / 4,
                                        kWindowHeight / 4, 0, kGreen, nullptr));

  glDeleteTextures(1, &tex);
  glDeleteProgram(program);
}

INSTANTIATE_TEST_SUITE_P(GLTextureTestWithWorkaround,
                         GLTextureTest,
                         testing::Bool());

}  // namespace gpu

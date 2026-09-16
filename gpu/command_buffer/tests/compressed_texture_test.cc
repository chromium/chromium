// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <vector>

#include "base/compiler_specific.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER(src) #src

namespace gpu {

static const uint16_t kRedMask = 0xF800;
static const uint16_t kGreenMask = 0x07E0;
static const uint16_t kBlueMask = 0x001F;

// Color palette in 565 format.
static const auto kPalette = std::to_array<uint16_t>({
    kGreenMask | kBlueMask,  // Cyan.
    kBlueMask | kRedMask,    // Magenta.
    kRedMask | kGreenMask,   // Yellow.
    0x0000,                  // Black.
    kRedMask,                // Red.
    kGreenMask,              // Green.
    kBlueMask,               // Blue.
    0xFFFF,                  // White.
});
static const unsigned kBlockSize = 4;
static const unsigned kTextureWidth = kBlockSize * kPalette.size();
static const unsigned kTextureHeight = kBlockSize;

static const char* extension(GLenum format) {
  switch(format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
      return "GL_ANGLE_texture_compression_dxt1";
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
      return "GL_ANGLE_texture_compression_dxt3";
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
      return "GL_ANGLE_texture_compression_dxt5";
    default:
      NOTREACHED();
  }
}

// Index that chooses the given colors (color_0 and color_1),
// not the interpolated colors (color_2 and color_3).
static const uint16_t kColor0 = 0x0000;
static const uint16_t kColor1 = 0x5555;

static GLuint LoadCompressedTexture(const void* data,
                                    GLsizeiptr size,
                                    GLenum format,
                                    GLsizei width,
                                    GLsizei height) {
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glCompressedTexImage2D(
      GL_TEXTURE_2D, 0, format, width, height, 0, size, data);
  return texture;
}

GLuint LoadTextureDXT1(bool alpha) {
  const unsigned kStride = 4;
  std::array<uint16_t, kStride * kPalette.size()> data;
  for (unsigned i = 0; i < kPalette.size(); ++i) {
    // Each iteration defines a 4x4 block of texture.
    unsigned j = kStride * i;
    data[j++] = kPalette[i];  // color_0.
    data[j++] = kPalette[i];  // color_1.
    data[j++] = kColor0;      // color index.
    data[j++] = kColor1;      // color index.
  }
  GLenum format = alpha ?
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
  return LoadCompressedTexture(data.data(), sizeof(data), format, kTextureWidth,
                               kTextureHeight);
}

GLuint LoadTextureDXT3() {
  const unsigned kStride = 8;
  const uint16_t kOpaque = 0xFFFF;
  std::array<uint16_t, kStride * kPalette.size()> data;
  for (unsigned i = 0; i < kPalette.size(); ++i) {
    // Each iteration defines a 4x4 block of texture.
    unsigned j = kStride * i;
    data[j++] = kOpaque;      // alpha row 0.
    data[j++] = kOpaque;      // alpha row 1.
    data[j++] = kOpaque;      // alpha row 2.
    data[j++] = kOpaque;      // alpha row 3.
    data[j++] = kPalette[i];  // color_0.
    data[j++] = kPalette[i];  // color_1.
    data[j++] = kColor0;      // color index.
    data[j++] = kColor1;      // color index.
  }
  return LoadCompressedTexture(data.data(), sizeof(data),
                               GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, kTextureWidth,
                               kTextureHeight);
}

GLuint LoadTextureDXT5() {
  const unsigned kStride = 8;
  const uint16_t kClear = 0x0000;
  const uint16_t kAlpha7 = 0xFFFF;  // Opaque alpha index.
  std::array<uint16_t, kStride * kPalette.size()> data;
  for (unsigned i = 0; i < kPalette.size(); ++i) {
    // Each iteration defines a 4x4 block of texture.
    unsigned j = kStride * i;
    data[j++] = kClear;       // alpha_0 | alpha_1.
    data[j++] = kAlpha7;      // alpha index.
    data[j++] = kAlpha7;      // alpha index.
    data[j++] = kAlpha7;      // alpha index.
    data[j++] = kPalette[i];  // color_0.
    data[j++] = kPalette[i];  // color_1.
    data[j++] = kColor0;      // color index.
    data[j++] = kColor1;      // color index.
  }
  return LoadCompressedTexture(data.data(), sizeof(data),
                               GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, kTextureWidth,
                               kTextureHeight);
}

static void ToRGB888(uint16_t rgb565, uint8_t rgb888[]) {
  uint8_t r5 = (rgb565 & kRedMask) >> 11;
  uint8_t g6 = (rgb565 & kGreenMask) >> 5;
  uint8_t b5 = (rgb565 & kBlueMask);
  // Replicate upper bits to lower empty bits.
  rgb888[0] = (r5 << 3) | (r5 >> 2);
  UNSAFE_TODO(rgb888[1]) = (g6 << 2) | (g6 >> 4);
  UNSAFE_TODO(rgb888[2]) = (b5 << 3) | (b5 >> 2);
}

class CompressedTextureTest : public ::testing::TestWithParam<GLenum> {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kTextureWidth, kTextureHeight);
    gl_.Initialize(options);
  }

  void TearDown() override { gl_.Destroy(); }

  GLuint LoadProgram() {
    const char* v_shader_src = SHADER(
        attribute vec2 a_position;
        varying vec2 v_texcoord;
        void main() {
          gl_Position = vec4(a_position, 0.0, 1.0);
          v_texcoord = (a_position + 1.0) * 0.5;
        }
    );
    const char* f_shader_src = SHADER(
        precision mediump float;
        uniform sampler2D u_texture;
        varying vec2 v_texcoord;
        void main() {
          gl_FragColor = texture2D(u_texture, v_texcoord);
        }
    );
    return GLTestHelper::LoadProgram(v_shader_src, f_shader_src);
  }

  GLuint LoadTexture(GLenum format) {
    switch (format) {
      case GL_COMPRESSED_RGB_S3TC_DXT1_EXT: return LoadTextureDXT1(false);
      case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: return LoadTextureDXT1(true);
      case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: return LoadTextureDXT3();
      case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: return LoadTextureDXT5();
      default:
        NOTREACHED();
    }
  }

 private:
  GLManager gl_;
};

// The test draws a texture in the given format and verifies that the drawn
// pixels are of the same color as the texture.
// The texture consists of 4x4 blocks of texels (same as DXT), one for each
// color defined in kPalette.
TEST_P(CompressedTextureTest, Draw) {
  GLenum format = GetParam();

  // This test is only valid if compressed texture extension is supported.
  const char* ext = extension(format);
  if (!GLTestHelper::HasExtension(ext))
    return;

  // Load shader program.
  GLuint program = LoadProgram();
  ASSERT_NE(program, 0u);
  GLint position_loc = glGetAttribLocation(program, "a_position");
  GLint texture_loc = glGetUniformLocation(program, "u_texture");
  ASSERT_NE(position_loc, -1);
  ASSERT_NE(texture_loc, -1);
  glUseProgram(program);

  // Load geometry.
  GLuint vbo = GLTestHelper::SetupUnitQuad(position_loc);
  ASSERT_NE(vbo, 0u);

  // Load texture.
  GLuint texture = LoadTexture(format);
  ASSERT_NE(texture, 0u);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform1i(texture_loc, 0);

  // Draw.
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glFlush();

  // Verify results.
  int origin[] = {0, 0};
  uint8_t expected_rgba[] = {0, 0, 0, 255};
  for (unsigned i = 0; i < kPalette.size(); ++i) {
    origin[0] = kBlockSize * i;
    ToRGB888(kPalette[i], expected_rgba);
    EXPECT_TRUE(GLTestHelper::CheckPixels(origin[0], origin[1], kBlockSize,
                                          kBlockSize, 0, expected_rgba,
                                          nullptr));
  }
  GLTestHelper::CheckGLError("CompressedTextureTest.Draw", __LINE__);
}

static const GLenum kFormats[] = {
  GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
  GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
  GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
  GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
};
INSTANTIATE_TEST_SUITE_P(Format,
                         CompressedTextureTest,
                         ::testing::ValuesIn(kFormats));

class CompressedTextureTestES3 : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kTextureWidth, kTextureHeight);
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    GpuDriverBugWorkarounds workarounds;
    workarounds.reset_base_level_for_astc_image = GetParam();
    gl_.InitializeWithWorkarounds(options, workarounds);
  }

  void TearDown() override { gl_.Destroy(); }

  void UploadASTCLevel(GLenum target,
                       GLenum format,
                       GLint level,
                       GLsizei width,
                       GLsizei height,
                       GLsizei block_width,
                       GLsizei block_height,
                       const std::array<uint8_t, 4>& color,
                       bool use_sub_image) {
    // Void-extent block for ASTC.
    // Format: 0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // R_lo, R_hi, G_lo, G_hi, B_lo, B_hi, A_lo, A_hi
    const std::array<uint8_t, 16> block = {
        0xFC,     0xFD,     0xFF,     0xFF,     0xFF,     0xFF,
        0xFF,     0xFF,     color[0], color[0], color[1], color[1],
        color[2], color[2], color[3], color[3]};

    GLsizei num_blocks_x = (width + block_width - 1) / block_width;
    GLsizei num_blocks_y = (height + block_height - 1) / block_height;
    GLsizei num_blocks = num_blocks_x * num_blocks_y;

    std::vector<uint8_t> data;
    data.reserve(num_blocks * 16);
    for (GLsizei i = 0; i < num_blocks; ++i) {
      data.insert(data.end(), block.begin(), block.end());
    }

    if (use_sub_image) {
      glCompressedTexSubImage2D(target, level, 0, 0, width, height, format,
                                static_cast<GLsizei>(data.size()), data.data());
    } else {
      glCompressedTexImage2D(target, level, format, width, height, 0,
                             static_cast<GLsizei>(data.size()), data.data());
    }
    ASSERT_TRUE(GLTestHelper::CheckGLError("UploadASTCLevel", __LINE__));
  }

  void RunASTCCompressedWithBaseLevelTest(bool use_sub_image) {
    if (!GLTestHelper::HasExtension("GL_KHR_texture_compression_astc_ldr")) {
      return;
    }

    // Use shaders that match the proof-of-concept.
    // They don't use vertex attributes, but gl_VertexID to generate a quad.
    const char* kVS =
        "#version 300 es\n"
        "out vec2 uv;\n"
        "void main() {\n"
        "  vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1);\n"
        "  uv = p;\n"
        "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n";

    const char* kFS =
        "#version 300 es\n"
        "precision highp float;\n"
        "uniform sampler2D t;\n"
        "in vec2 uv;\n"
        "out vec4 c;\n"
        "void main() {\n"
        "  c = texture(t, uv);\n"
        "}\n";

    GLuint program = GLTestHelper::LoadProgram(kVS, kFS);
    ASSERT_NE(program, 0u);
    glUseProgram(program);
    GLint tex_location = glGetUniformLocation(program, "t");
    ASSERT_NE(tex_location, -1);
    glUniform1i(tex_location, 0);
    ASSERT_TRUE(GLTestHelper::CheckGLError("Setup program", __LINE__));

    constexpr std::array<uint8_t, 4> kBlack = {0, 0, 0, 255};
    constexpr std::array<uint8_t, 4> kRed = {255, 0, 0, 255};
    constexpr std::array<uint8_t, 4> kGreen = {0, 255, 0, 255};

    // 8x5 ASTC format.
    // This needs to be big enough, so at smallest level (=4), width >= kBlockWidth.
    // This is to work around a driver bug: https://crbug.com/562185979.
    GLenum format = GL_COMPRESSED_RGBA_ASTC_8x5_KHR;
    constexpr GLsizei kWidth = 128;
    constexpr GLsizei kHeight = 160;
    constexpr GLsizei kLevels = 5;
    constexpr GLsizei kBlockWidth = 8;
    constexpr GLsizei kBlockHeight = 5;

    // Loop multiple times to increase chances of hitting OOB write/crash if
    // workaround fails. Keep textures alive to groom the heap.
    constexpr int kIterations = 16;
    std::vector<GLuint> textures(kIterations);
    glGenTextures(kIterations, textures.data());

    for (int i = 0; i < kIterations; ++i) {
      glBindTexture(GL_TEXTURE_2D, textures[i]);

      if (use_sub_image) {
        glTexStorage2DEXT(GL_TEXTURE_2D, kLevels, format, kWidth, kHeight);
        ASSERT_TRUE(GLTestHelper::CheckGLError("glTexStorage2D", __LINE__));
      } else {
        // We need to initialize all levels to work around a driver bug.
        // https://crbug.com/556435507.
        for (GLint level = 0; level < kLevels; ++level) {
          GLsizei level_width = std::max(1, kWidth >> level);
          GLsizei level_height = std::max(1, kHeight >> level);
          UploadASTCLevel(GL_TEXTURE_2D, format, level, level_width,
                          level_height, kBlockWidth, kBlockHeight, kBlack,
                          /*use_sub_image=*/false);
        }
      }

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 4);
      ASSERT_TRUE(
          GLTestHelper::CheckGLError("glTexParameteri base level 4", __LINE__));

      // Upload Red to level 4 (8x10).
      UploadASTCLevel(GL_TEXTURE_2D, format, 4, 8, 10, kBlockWidth,
                      kBlockHeight, kRed, use_sub_image);

      // Upload Green to level 3 (16x20).
      UploadASTCLevel(GL_TEXTURE_2D, format, 3, 16, 20, kBlockWidth,
                      kBlockHeight, kGreen, use_sub_image);

      // Draw. Since BASE_LEVEL is 4, it should sample from level 4 (Red).
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glFinish();
      ASSERT_TRUE(GLTestHelper::CheckGLError("Draw level 4", __LINE__));

      EXPECT_TRUE(
          GLTestHelper::CheckPixels(0, 0, 1, 1, 1, kRed.data(), nullptr));

      // Change BASE_LEVEL to 3.
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 3);
      ASSERT_TRUE(
          GLTestHelper::CheckGLError("glTexParameteri base level 3", __LINE__));

      // Draw again. Now it should sample from level 3 (Green).
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glFinish();
      ASSERT_TRUE(GLTestHelper::CheckGLError("Draw level 3", __LINE__));

      EXPECT_TRUE(
          GLTestHelper::CheckPixels(0, 0, 1, 1, 1, kGreen.data(), nullptr));
    }

    glDeleteTextures(kIterations, textures.data());
    glDeleteProgram(program);
  }

 private:
  GLManager gl_;
};

// Test that compressed sub-image updates work when TEXTURE_BASE_LEVEL > 0.
// This is a workaround for a PowerVR driver bug where it miscomputes the
// offset.
TEST_P(CompressedTextureTestES3, ASTCCompressedSubImageWithBaseLevel) {
  RunASTCCompressedWithBaseLevelTest(/*use_sub_image=*/true);
}

// Test that compressed image updates work when TEXTURE_BASE_LEVEL > 0.
// This is a workaround for a PowerVR driver bug where it miscomputes the
// offset.
TEST_P(CompressedTextureTestES3, ASTCCompressedImageWithBaseLevel) {
  RunASTCCompressedWithBaseLevelTest(/*use_sub_image=*/false);
}

INSTANTIATE_TEST_SUITE_P(Workaround,
                         CompressedTextureTestES3,
                         ::testing::Bool());

}  // namespace gpu

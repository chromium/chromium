// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include <array>

#include "base/compiler_specific.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
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
static const unsigned kPaletteSize =
    (kPalette.size() * sizeof(decltype(kPalette)::value_type)) /
    sizeof(kPalette[0]);
static const unsigned kTextureWidth = kBlockSize * kPaletteSize;
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
  uint16_t data[kStride * kPaletteSize];
  for (unsigned i = 0; i < kPaletteSize; ++i) {
    // Each iteration defines a 4x4 block of texture.
    unsigned j = kStride * i;
    UNSAFE_TODO(data[j++]) = kPalette[i];  // color_0.
    UNSAFE_TODO(data[j++]) = kPalette[i];  // color_1.
    UNSAFE_TODO(data[j++]) = kColor0;      // color index.
    UNSAFE_TODO(data[j++]) = kColor1;      // color index.
  }
  GLenum format = alpha ?
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
  return LoadCompressedTexture(
      data, sizeof(data), format, kTextureWidth, kTextureHeight);
}

GLuint LoadTextureDXT3() {
  const unsigned kStride = 8;
  const uint16_t kOpaque = 0xFFFF;
  uint16_t data[kStride * kPaletteSize];
  for (unsigned i = 0; i < kPaletteSize; ++i) {
    // Each iteration defines a 4x4 block of texture.
    unsigned j = kStride * i;
    UNSAFE_TODO(data[j++]) = kOpaque;      // alpha row 0.
    UNSAFE_TODO(data[j++]) = kOpaque;      // alpha row 1.
    UNSAFE_TODO(data[j++]) = kOpaque;      // alpha row 2.
    UNSAFE_TODO(data[j++]) = kOpaque;      // alpha row 3.
    UNSAFE_TODO(data[j++]) = kPalette[i];  // color_0.
    UNSAFE_TODO(data[j++]) = kPalette[i];  // color_1.
    UNSAFE_TODO(data[j++]) = kColor0;      // color index.
    UNSAFE_TODO(data[j++]) = kColor1;      // color index.
  }
  return LoadCompressedTexture(data,
                               sizeof(data),
                               GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
                               kTextureWidth,
                               kTextureHeight);
}

GLuint LoadTextureDXT5() {
  const unsigned kStride = 8;
  const uint16_t kClear = 0x0000;
  const uint16_t kAlpha7 = 0xFFFF;  // Opaque alpha index.
  uint16_t data[kStride * kPaletteSize];
  for (unsigned i = 0; i < kPaletteSize; ++i) {
    // Each iteration defines a 4x4 block of texture.
    unsigned j = kStride * i;
    UNSAFE_TODO(data[j++]) = kClear;       // alpha_0 | alpha_1.
    UNSAFE_TODO(data[j++]) = kAlpha7;      // alpha index.
    UNSAFE_TODO(data[j++]) = kAlpha7;      // alpha index.
    UNSAFE_TODO(data[j++]) = kAlpha7;      // alpha index.
    UNSAFE_TODO(data[j++]) = kPalette[i];  // color_0.
    UNSAFE_TODO(data[j++]) = kPalette[i];  // color_1.
    UNSAFE_TODO(data[j++]) = kColor0;      // color index.
    UNSAFE_TODO(data[j++]) = kColor1;      // color index.
  }
  return LoadCompressedTexture(data,
                               sizeof(data),
                               GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                               kTextureWidth,
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
  for (unsigned i = 0; i < kPaletteSize; ++i) {
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

class CompressedTextureTestES3 : public ::testing::Test {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kTextureWidth, kTextureHeight);
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    gl_.Initialize(options);
  }

  void TearDown() override { gl_.Destroy(); }

 private:
  GLManager gl_;
};

// Test that compressed sub-image updates work when TEXTURE_BASE_LEVEL > 0.
// This is a workaround for a PowerVR driver bug where it miscomputes the
// offset.
TEST_F(CompressedTextureTestES3, ASTCCompressedSubImageWithBaseLevel) {
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
  GLTestHelper::CheckGLError("Setup program", __LINE__);

  // 8x5 ASTC format.
  GLenum format = GL_COMPRESSED_RGBA_ASTC_8x5_KHR;
  constexpr GLsizei kWidth = 8;
  constexpr GLsizei kHeight = 160;
  constexpr GLsizei kLevels = 5;
  std::vector<uint8_t> data(16, 0);

  // Loop multiple times to increase chances of hitting OOB write/crash if
  // workaround fails. Keep textures alive to groom the heap.
  constexpr int kIterations = 16;
  std::vector<GLuint> textures(kIterations);
  glGenTextures(kIterations, textures.data());

  for (int i = 0; i < kIterations; ++i) {
    glBindTexture(GL_TEXTURE_2D, textures[i]);

    glTexStorage2DEXT(GL_TEXTURE_2D, kLevels, format, kWidth, kHeight);
    GLTestHelper::CheckGLError("glTexStorage2D", __LINE__);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 4);
    GLTestHelper::CheckGLError("glTexParameteri", __LINE__);

    // Draw using the program that samples the texture.
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFinish();
    GLTestHelper::CheckGLError("Draw", __LINE__);

    // Perform sub-image update to level 3.
    // 1x5 pixels is 1 block for 8x5 ASTC, which is 16 bytes.
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 3, 0, 0, 1, 5, format,
                              static_cast<GLsizei>(data.size()), data.data());
    GLTestHelper::CheckGLError("glCompressedTexSubImage2D level 3", __LINE__);

    // Also try level 4 (which is the base level)
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 4, 0, 0, 1, 5, format,
                              static_cast<GLsizei>(data.size()), data.data());
    GLTestHelper::CheckGLError("glCompressedTexSubImage2D level 4", __LINE__);
  }

  glDeleteTextures(kIterations, textures.data());
  glDeleteProgram(program);
}

}  // namespace gpu

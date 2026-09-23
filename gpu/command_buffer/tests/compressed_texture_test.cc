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

// Per OpenGL ES 3.2, "All pixel storage modes are ignored when decoding a
// compressed texture image" (sec. 8.7). These tests upload the same data
// with and without UNPACK_ROW_LENGTH / UNPACK_IMAGE_HEIGHT set while a
// pixel unpack buffer is bound, and require byte-identical readbacks of
// every layer either way (crbug.com/562279351).
class CompressedTexturePixelUnpackStateTest : public CompressedTextureTestES3 {
 protected:
  static constexpr GLsizei kTexSize = 4;
  static constexpr GLsizei kTexDepth = 4;
  // GL_COMPRESSED_R11_EAC: one 8-byte block per 4x4 region.
  static constexpr GLsizei kEacBlockSize = 8;
  static constexpr GLsizei kImageSize = kEacBlockSize * kTexDepth;
  static constexpr GLsizei kBufferSize = 16384;
  // Large enough that a driver consuming these against the spec computes
  // strides whose products wrap 32 bits.
  static constexpr GLint kLargeRowLength = 524292;
  static constexpr GLint kLargeImageHeight = 16384;

  using LayerPixels = std::array<uint8_t, kTexSize * kTexSize * 4>;
  using AllLayerPixels = std::array<LayerPixels, kTexDepth>;

  void SetUp() override {
    CompressedTextureTestES3::SetUp();

    // Renders one selected layer of a 4x4 TEXTURE_2D_ARRAY via texelFetch.
    const char* kVS =
        "#version 300 es\n"
        "void main() {\n"
        "  vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1);\n"
        "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n";
    const char* kFS =
        "#version 300 es\n"
        "precision highp float;\n"
        "precision highp int;\n"
        "uniform highp sampler2DArray t;\n"
        "uniform int layer;\n"
        "out vec4 c;\n"
        "void main() {\n"
        "  c = texelFetch(t, ivec3(ivec2(gl_FragCoord.xy), layer), 0);\n"
        "}\n";
    program_ = GLTestHelper::LoadProgram(kVS, kFS);
    ASSERT_NE(program_, 0u) << "failed to build the layer-sampling program";
    glUseProgram(program_);
    GLint tex_location = glGetUniformLocation(program_, "t");
    ASSERT_NE(tex_location, -1) << "sampler uniform 't' not found";
    glUniform1i(tex_location, 0);
    layer_location_ = glGetUniformLocation(program_, "layer");
    ASSERT_NE(layer_location_, -1) << "uniform 'layer' not found";
  }

  void TearDown() override {
    if (program_) {
      glDeleteProgram(program_);
    }
    CompressedTextureTestES3::TearDown();
  }

  // Creates and leaves bound a pixel unpack buffer holding the probe data.
  // Each 8-byte EAC block gets a distinct byte pattern -- a per-block base
  // value (0x11 * (block + 1)) perturbed per byte (^ (i * 7)) -- so the four
  // layers decode to four distinguishable images; the rest of the buffer is
  // filler that a conformant upload never reads.
  GLuint MakeProbePixelUnpackBuffer() {
    std::vector<uint8_t> bytes(kBufferSize, 0xA5);
    for (GLsizei block = 0; block < kTexDepth; ++block) {
      for (GLsizei i = 0; i < kEacBlockSize; ++i) {
        bytes[block * kEacBlockSize + i] =
            static_cast<uint8_t>((0x11 * (block + 1)) ^ (i * 7));
      }
    }
    GLuint pbo = 0;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, bytes.size(), bytes.data(),
                 GL_STATIC_DRAW);
    return pbo;
  }

  void SetLargeUnpackGeometry(bool enable) {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, enable ? kLargeRowLength : 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, enable ? kLargeImageHeight : 0);
  }

  void ReadBackAllLayers(AllLayerPixels* out) {
    glViewport(0, 0, kTexSize, kTexSize);
    for (GLsizei layer = 0; layer < kTexDepth; ++layer) {
      glUniform1i(layer_location_, layer);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glReadPixels(0, 0, kTexSize, kTexSize, GL_RGBA, GL_UNSIGNED_BYTE,
                   (*out)[layer].data());
    }
  }

  void ExpectLayersEqual(const AllLayerPixels& with_state,
                         const AllLayerPixels& without_state) {
    for (GLsizei layer = 0; layer < kTexDepth; ++layer) {
      EXPECT_TRUE(with_state[layer] == without_state[layer])
          << "layer " << layer
          << " differs when UNPACK_ROW_LENGTH/UNPACK_IMAGE_HEIGHT are set";
    }
  }

  GLuint program_ = 0;
  GLint layer_location_ = -1;
};

INSTANTIATE_TEST_SUITE_P(Workaround,
                         CompressedTexturePixelUnpackStateTest,
                         ::testing::Bool());

TEST_P(CompressedTexturePixelUnpackStateTest, IgnoredForCompressedTexImage3D) {
  std::array<AllLayerPixels, 2> readback;
  for (int with_state = 0; with_state < 2; ++with_state) {
    GLuint pbo = MakeProbePixelUnpackBuffer();
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    SetLargeUnpackGeometry(with_state != 0);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, /*level=*/0,
                           GL_COMPRESSED_R11_EAC, kTexSize, kTexSize, kTexDepth,
                           /*border=*/0, kImageSize, nullptr);
    SetLargeUnpackGeometry(false);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
    ASSERT_TRUE(GLTestHelper::CheckGLError("CompressedTexImage3D", __LINE__));

    ReadBackAllLayers(&readback[with_state]);
    ASSERT_TRUE(GLTestHelper::CheckGLError("ReadBackAllLayers", __LINE__));
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &pbo);
  }
  ExpectLayersEqual(readback[1], readback[0]);
}

TEST_P(CompressedTexturePixelUnpackStateTest,
       IgnoredForCompressedTexSubImage3D) {
  std::array<AllLayerPixels, 2> readback;
  for (int with_state = 0; with_state < 2; ++with_state) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    // Define the level from client memory first; the sub-image update below
    // is the upload under test.
    const std::vector<uint8_t> zeros(kImageSize, 0);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, /*level=*/0,
                           GL_COMPRESSED_R11_EAC, kTexSize, kTexSize, kTexDepth,
                           /*border=*/0, kImageSize, zeros.data());
    GLuint pbo = MakeProbePixelUnpackBuffer();
    SetLargeUnpackGeometry(with_state != 0);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, /*level=*/0, /*xoffset=*/0,
                              /*yoffset=*/0, /*zoffset=*/0, kTexSize, kTexSize,
                              kTexDepth, GL_COMPRESSED_R11_EAC, kImageSize,
                              nullptr);
    SetLargeUnpackGeometry(false);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
    ASSERT_TRUE(
        GLTestHelper::CheckGLError("CompressedTexSubImage3D", __LINE__));

    ReadBackAllLayers(&readback[with_state]);
    ASSERT_TRUE(GLTestHelper::CheckGLError("ReadBackAllLayers", __LINE__));
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &pbo);
  }
  ExpectLayersEqual(readback[1], readback[0]);
}

// A partial sub-image into a fresh immutable texture makes the service
// zero-fill the uncleared level first, a CPU-sourced compressed upload that
// runs while the client's unpack state is still resident in the driver. The
// never-written layers must decode identically with and without the unpack
// state set.
TEST_P(CompressedTexturePixelUnpackStateTest,
       IgnoredWhenClearingCompressedLevels) {
  std::array<AllLayerPixels, 2> readback;

  // TODO(crbug.com/411230292): Remove this block once the android-x86-rel
  // trybot is moved off of swiftshader_indirect to a GPU backend with more
  // complete ES 3.0 conformance.
  // Immutable compressed storage of ETC2/EAC is not supported everywhere
  // this suite runs: several bot configurations accept compressed uploads of
  // these formats (via decoder-side decompression, or an emulated stack
  // underneath) but reject glTexStorage3D with them -- and the extension
  // string GL_ANGLE_compressed_texture_etc does not discriminate, since the
  // service advertises it for emulated support too (see
  // FeatureInfo::EnableWebGLCompressedTextureETC and its "we assume"
  // comment). The clear path under test needs an uncleared immutable
  // compressed level, so probe the exact operation on a throwaway texture
  // and skip where it is unsupported. On hardware with native ETC2 support
  // (where this test discriminates) the probe succeeds and any failure in
  // the test body below is a real failure; that hardware is also covered by
  // the on-device falsification legs.
  {
    GLuint probe_tex = 0;
    glGenTextures(1, &probe_tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, probe_tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, /*levels=*/1, GL_COMPRESSED_R11_EAC,
                   kTexSize, kTexSize, kTexDepth);
    const GLenum storage_error = glGetError();
    glDeleteTextures(1, &probe_tex);
    if (storage_error != GL_NO_ERROR) {
      const char* renderer =
          reinterpret_cast<const char*>(glGetString(GL_RENDERER));
      const char* version =
          reinterpret_cast<const char*>(glGetString(GL_VERSION));
      GTEST_SKIP() << "TexStorage3D with GL_COMPRESSED_R11_EAC unsupported "
                      "(error 0x"
                   << std::hex << storage_error << std::dec << ", "
                   << "GL_ANGLE_compressed_texture_etc "
                   << (GLTestHelper::HasExtension(
                           "GL_ANGLE_compressed_texture_etc")
                           ? "advertised"
                           : "absent")
                   << ", renderer: " << (renderer ? renderer : "null")
                   << ", version: " << (version ? version : "null") << ")";
    }
  }

  for (int with_state = 0; with_state < 2; ++with_state) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, /*levels=*/1, GL_COMPRESSED_R11_EAC,
                   kTexSize, kTexSize, kTexDepth);
    GLuint pbo = MakeProbePixelUnpackBuffer();
    SetLargeUnpackGeometry(with_state != 0);
    // Depth 1 of kTexDepth: forces the zero-fill of the whole level before
    // this upload lands in layer 0.
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, /*level=*/0, /*xoffset=*/0,
                              /*yoffset=*/0, /*zoffset=*/0, kTexSize, kTexSize,
                              /*depth=*/1, GL_COMPRESSED_R11_EAC, kEacBlockSize,
                              nullptr);
    SetLargeUnpackGeometry(false);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_TRUE(GLTestHelper::CheckGLError("partial CompressedTexSubImage3D",
                                           __LINE__));

    ReadBackAllLayers(&readback[with_state]);
    ASSERT_TRUE(GLTestHelper::CheckGLError("ReadBackAllLayers", __LINE__));
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &pbo);
  }
  ExpectLayersEqual(readback[1], readback[0]);
}

}  // namespace gpu

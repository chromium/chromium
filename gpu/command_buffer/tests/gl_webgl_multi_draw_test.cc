// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <math.h>
#include <stdint.h>
#include <vector>

#include "base/containers/contains.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLWebGLMultiDrawTest : public ::testing::Test {
 protected:
  GLWebGLMultiDrawTest() : max_transfer_buffer_size_(16 * 1024) {
    size_t vertex_bytes_per_pixel = sizeof(GLint) + sizeof(GLsizei);
    uint32_t pixel_count = max_transfer_buffer_size_ / vertex_bytes_per_pixel;
    canvas_size_ = 2 * static_cast<uint32_t>(std::sqrt(pixel_count));
  }

  void SetUp() override {
    GLManager::Options options;
    options.shared_memory_limits.min_transfer_buffer_size =
        max_transfer_buffer_size_ / 2;
    options.shared_memory_limits.max_transfer_buffer_size =
        max_transfer_buffer_size_;
    options.shared_memory_limits.start_transfer_buffer_size =
        max_transfer_buffer_size_ / 2;
    options.context_type = CONTEXT_TYPE_WEBGL1;
    options.size = gfx::Size(canvas_size_, canvas_size_);
    gl_.Initialize(options);
  }

  void TearDown() override { gl_.Destroy(); }

  uint32_t canvas_size() const { return canvas_size_; }

  uint32_t max_transfer_buffer_size() const {
    return max_transfer_buffer_size_;
  }

  gles2::GLES2Implementation* gles2_implementation() const {
    return gl_.gles2_implementation();
  }

 private:
  GLManager gl_;
  uint32_t canvas_size_;
  uint32_t max_transfer_buffer_size_;
};

// This test issues a MultiDrawArrays that requires more command space than
// the maximum size of the transfer buffer. To check that every draw is issued,
// each gl_DrawID is transformed to a unique pixel and colored a unique value.
// The pixels are read back and checked that every pixel is correct.
TEST_F(GLWebGLMultiDrawTest, MultiDrawLargerThanTransferBuffer) {
  std::string requestable_extensions_string =
      reinterpret_cast<const char*>(glGetRequestableExtensionsCHROMIUM());

  // This test is only valid if the multi draw extension is supported
  if (!GLTestHelper::HasExtension("GL_ANGLE_multi_draw")) {
    if (!base::Contains(requestable_extensions_string,
                        "GL_ANGLE_multi_draw ")) {
      return;
    }
    glRequestExtensionCHROMIUM("GL_ANGLE_multi_draw");
  }

  if (!GLTestHelper::HasExtension("GL_WEBGL_multi_draw")) {
    if (!base::Contains(requestable_extensions_string,
                        "GL_WEBGL_multi_draw ")) {
      return;
    }
    glRequestExtensionCHROMIUM("GL_WEBGL_multi_draw");
  }

  std::string vertex_source =
      "#define SIZE " + std::to_string(canvas_size()) + "\n";
  vertex_source += "#extension GL_ANGLE_multi_draw : require\n";
  vertex_source += R"(
    attribute vec2 a_position;
    varying vec4 v_color;

    int mod(int x, int y) {
      int q = x / y;
      return x - q * y;
    }

    int rshift8(int x) {
      int result = x;
      for (int i = 0; i < 8; ++i) {
        result = result / 2;
      }
      return result;
    }

    int rshift16(int x) {
      int result = x;
      for (int i = 0; i < 16; ++i) {
        result = result / 2;
      }
      return result;
    }

    int rshift24(int x) {
      int result = x;
      for (int i = 0; i < 24; ++i) {
        result = result / 2;
      }
      return result;
    }

    void main() {
      int x_int = mod(gl_DrawID, SIZE);
      int y_int = gl_DrawID / SIZE;

      float s = 1.0 / float(SIZE);
      float x = float(x_int) / float(SIZE);
      float y = float(y_int) / float(SIZE);
      float z = 0.0;
      mat4 m = mat4(s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, x, y, z, 1);
      vec2 position01 = a_position * 0.5 + 0.5;
      gl_Position = (m * vec4(position01, 0.0, 1.0)) * 2.0 - 1.0;
      int r = mod(rshift24(gl_DrawID), 256);
      int g = mod(rshift16(gl_DrawID), 256);
      int b = mod(rshift8(gl_DrawID), 256);
      int a = mod(gl_DrawID, 256);
      float denom = 1.0 / 255.0;
      v_color = vec4(r, g, b, a) * denom;
    })";

  GLuint program = GLTestHelper::LoadProgram(vertex_source.c_str(), R"(
      precision mediump float;
      varying vec4 v_color;
      void main() { gl_FragColor = v_color; }
)");
  ASSERT_NE(program, 0u);
  GLint position_loc = glGetAttribLocation(program, "a_position");
  ASSERT_NE(position_loc, -1);
  glUseProgram(program);

  GLuint vbo = GLTestHelper::SetupUnitQuad(position_loc);
  ASSERT_NE(vbo, 0u);

  std::vector<GLint> firsts(canvas_size() * canvas_size(), 0);
  std::vector<GLsizei> counts(canvas_size() * canvas_size(), 6);

  ASSERT_GT(firsts.size() * sizeof(GLint) + counts.size() * sizeof(GLsizei),
            max_transfer_buffer_size());

  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gles2_implementation()->MultiDrawArraysWEBGL(GL_TRIANGLES, firsts.data(),
                                               counts.data(),
                                               canvas_size() * canvas_size());
  glFlush();

  std::vector<std::array<uint8_t, 4>> pixels(canvas_size() * canvas_size());
  glReadPixels(0, 0, canvas_size(), canvas_size(), GL_RGBA, GL_UNSIGNED_BYTE,
               pixels.data());
  unsigned int expected = 0;
  for (const auto& pixel : pixels) {
    unsigned int id =
        ((pixel[0] << 24) + (pixel[1] << 16) + (pixel[2] << 8) + pixel[3]);
    EXPECT_EQ(id, expected);
    expected++;
  }

  GLTestHelper::CheckGLError(
      "GLWebGLMultiDrawTest.MultiDrawLargerThanTransferBuffer", __LINE__);
}

}  // namespace gpu

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>

#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/gl/gl_image_native_pixmap.h"
#endif

#define SKIP_TEST_IF(cmd)                        \
  do {                                           \
    if (cmd) {                                   \
      LOG(INFO) << "Skip test because " << #cmd; \
      return;                                    \
    }                                            \
  } while (false)

namespace gpu {
namespace {

static const int kImageWidth = 64;
static const int kImageHeight = 64;

}  // namespace

class GpuOESEGLImageTest : public testing::Test,
                           public gpu::GpuCommandBufferTestEGL {
 protected:
  void SetUp() override {
    egl_initialized_ = InitializeEGL(kImageWidth, kImageHeight);
  }

  void TearDown() override { RestoreGLDefault(); }

  bool egl_initialized_{false};
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/835072): re-enable this test on ASAN once bugs are fixed.
#if !defined(ADDRESS_SANITIZER)

#define SHADER(Src) #Src

// clang-format off
const char kVertexShader[] =
SHADER(
  attribute vec4 a_position;
  varying vec2 v_texCoord;
  void main() {
    gl_Position = a_position;
    v_texCoord = vec2((a_position.x + 1.0) * 0.5, (a_position.y + 1.0) * 0.5);
  }
);

const char* kFragmentShader =
SHADER(
  precision mediump float;
  uniform sampler2D a_texture;
  varying vec2 v_texCoord;
  void main() {
    gl_FragColor = texture2D(a_texture, v_texCoord);
  }
);
// clang-format on

// The test verifies that the content of an EGLImage can be drawn. Indeed the
// test upload some colored pixels into a GL texture. Then it creates an
// EGLImage from this texture and binds this image to draw it into another
// GL texture. At the end the test downloads the pixels from the final GL
// texture and verifies that the colors match with the pixels uploaded into
// the initial GL texture.
TEST_F(GpuOESEGLImageTest, EGLImageToTexture) {
  SKIP_TEST_IF(!egl_initialized_);

  // This extension is required for creating an EGLImage from a GL texture.
  SKIP_TEST_IF(!HasEGLExtension("EGL_KHR_image_base"));

  // This extension is required to render an EGLImage into a GL texture.
  SKIP_TEST_IF(!HasGLExtension("GL_OES_EGL_image"));

  gfx::BufferFormat format = gfx::BufferFormat::RGBX_8888;
  gfx::Size size(kImageWidth, kImageHeight);
  size_t buffer_size = gfx::BufferSizeForBufferFormat(size, format);
  uint8_t pixel[] = {128u, 92u, 45u, 255u};
  size_t plane = 0;
  uint32_t stride = gfx::RowSizeForBufferFormat(size.width(), format, plane);

  std::unique_ptr<uint8_t[]> pixels(new uint8_t[buffer_size]);

  // Assign a value to each pixel.
  for (int y = 0; y < size.height(); ++y) {
    uint8_t* line = static_cast<uint8_t*>(pixels.get()) + y * stride;
    for (int x = 0; x < size.width() * 4; x += 4) {
      line[x + 0] = pixel[0];
      line[x + 1] = pixel[1];
      line[x + 2] = pixel[2];
      line[x + 3] = pixel[3];
    }
  }

  // Create an EGLImage from a GL texture.
  scoped_refptr<gl::GLImageNativePixmap> image =
      CreateGLImageNativePixmap(format, size, pixels.get());
  EXPECT_TRUE(image);
  EXPECT_EQ(size, image->GetSize());

  // Need a texture to bind the image.
  GLuint texture_id = 0;
  glGenTextures(1, &texture_id);
  ASSERT_NE(0u, texture_id);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  // Make sure the texture exists in the service side.
  glFinish();

  // Bind the image.
  EXPECT_TRUE(image->BindTexImage(GL_TEXTURE_2D));
  gl_.decoder()->SetLevelInfo(texture_id, 0 /* level */, GL_RGB, size.width(),
                              size.height(), 1 /* depth */, GL_RGB,
                              GL_UNSIGNED_BYTE, gfx::Rect(size));

  // TODO(crbug.com/1323341): This call is likely unnecessary, but it's not
  // currently possible to actually run the test to completion to verify. See
  // https://chromium-review.googlesource.com/c/chromium/src/+/4055269/comment/e8eed809_ffcdfc9c/.
  gl_.decoder()->AttachImageToTextureWithClientBinding(
      texture_id, GL_TEXTURE_2D, image.get());

  // Build program, buffers and draw the texture.
  GLuint vertex_shader =
      gpu::GLTestHelper::LoadShader(GL_VERTEX_SHADER, kVertexShader);
  GLuint fragment_shader =
      gpu::GLTestHelper::LoadShader(GL_FRAGMENT_SHADER, kFragmentShader);
  GLuint program =
      gpu::GLTestHelper::SetupProgram(vertex_shader, fragment_shader);
  ASSERT_NE(0u, program);
  glUseProgram(program);

  GLint sampler_location = glGetUniformLocation(program, "a_texture");
  ASSERT_NE(-1, sampler_location);
  glUniform1i(sampler_location, 0);

  GLuint vbo = gpu::GLTestHelper::SetupUnitQuad(
      glGetAttribLocation(program, "a_position"));
  ASSERT_NE(0u, vbo);
  glViewport(0, 0, kImageWidth, kImageHeight);

  // Render the EGLImage into the GL texture.
  glDrawArrays(GL_TRIANGLES, 0, 6);
  ASSERT_TRUE(glGetError() == GL_NO_ERROR);

  // Check if pixels match the values that were assigned to the mapped buffer.
  gpu::GLTestHelper::CheckPixels(0, 0, kImageWidth, kImageHeight, 0, pixel,
                                 nullptr);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // TODO(crbug.com/1323341): This call is likely unnecessary, but it's not
  // currently possible to actually run the test to completion to verify. See
  // https://chromium-review.googlesource.com/c/chromium/src/+/4055269/comment/e8eed809_ffcdfc9c/.
  gl_.decoder()->AttachImageToTextureWithClientBinding(
      texture_id, GL_TEXTURE_2D, image.get());

  // Clean up.
  glDeleteProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteBuffers(1, &vbo);
  glDeleteTextures(1, &texture_id);
}
#endif  // !defined(ADDRESS_SANITIZER)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace gpu

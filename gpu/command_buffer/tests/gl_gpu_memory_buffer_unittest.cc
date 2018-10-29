// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2chromium.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/half_float.h"
#include "ui/gl/gl_image.h"

#if defined(OS_LINUX)
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"
#endif

#define SKIP_TEST_IF(cmd)                        \
  do {                                           \
    if (cmd) {                                   \
      LOG(INFO) << "Skip test because " << #cmd; \
      return;                                    \
    }                                            \
  } while (false)

using testing::_;
using testing::IgnoreResult;
using testing::InvokeWithoutArgs;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace gpu {
namespace gles2 {

static const int kImageWidth = 32;
static const int kImageHeight = 32;

class GpuMemoryBufferTest : public testing::TestWithParam<gfx::BufferFormat> {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kImageWidth, kImageHeight);
    gl_.Initialize(options);
    gl_.MakeCurrent();
  }

  void TearDown() override {
    gl_.Destroy();
  }

  GLManager gl_;
};

#if defined(OS_LINUX)
class GpuMemoryBufferTestEGL : public testing::Test,
                               public gpu::GpuCommandBufferTestEGL {
 public:
  GpuMemoryBufferTestEGL()
      : egl_gles2_initialized_(false),
        native_pixmap_factory_(gfx::CreateClientNativePixmapFactoryDmabuf()) {}

 protected:
  void SetUp() override {
    egl_gles2_initialized_ = InitializeEGLGLES2(kImageWidth, kImageHeight);
    gl_.set_use_native_pixmap_memory_buffers(true);
  }

  void TearDown() override { RestoreGLDefault(); }

  bool egl_gles2_initialized_;
  std::unique_ptr<gfx::ClientNativePixmapFactory> native_pixmap_factory_;
};
#endif  // defined(OS_LINUX)

namespace {

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

void SetRow(gfx::BufferFormat format,
            uint8_t* buffer,
            int width,
            uint8_t pixel[4]) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      for (int i = 0; i < width; ++i)
        buffer[i] = pixel[0];
      return;
    case gfx::BufferFormat::BGR_565:
      for (int i = 0; i < width * 2; i += 2) {
        *reinterpret_cast<uint16_t*>(&buffer[i]) =
            ((pixel[2] >> 3) << 11) | ((pixel[1] >> 2) << 5) | (pixel[0] >> 3);
      }
      return;
    case gfx::BufferFormat::RGBA_4444:
      for (int i = 0; i < width * 2; i += 2) {
        buffer[i + 0] = (pixel[1] << 4) | (pixel[0] & 0xf);
        buffer[i + 1] = (pixel[3] << 4) | (pixel[2] & 0xf);
      }
      return;
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      for (int i = 0; i < width * 4; i += 4) {
        buffer[i + 0] = pixel[0];
        buffer[i + 1] = pixel[1];
        buffer[i + 2] = pixel[2];
        buffer[i + 3] = pixel[3];
      }
      return;
    case gfx::BufferFormat::BGRA_8888:
      for (int i = 0; i < width * 4; i += 4) {
        buffer[i + 0] = pixel[2];
        buffer[i + 1] = pixel[1];
        buffer[i + 2] = pixel[0];
        buffer[i + 3] = pixel[3];
      }
      return;
    case gfx::BufferFormat::RGBA_F16: {
      float float_pixel[4] = {
          pixel[0] / 255.f, pixel[1] / 255.f, pixel[2] / 255.f,
          pixel[3] / 255.f,
      };
      uint16_t half_float_pixel[4];
      gfx::FloatToHalfFloat(float_pixel, half_float_pixel, 4);
      uint16_t* half_float_buffer = reinterpret_cast<uint16_t*>(buffer);
      for (int i = 0; i < width * 4; i += 4) {
        half_float_buffer[i + 0] = half_float_pixel[0];
        half_float_buffer[i + 1] = half_float_pixel[1];
        half_float_buffer[i + 2] = half_float_pixel[2];
        half_float_buffer[i + 3] = half_float_pixel[3];
      }
      return;
    }
    case gfx::BufferFormat::RGBX_1010102:
      for (int x = 0; x < width; ++x) {
        *reinterpret_cast<uint32_t*>(&buffer[x * 4]) =
            0x3 << 30 |  // Alpha channel is unused
            ((pixel[2] << 2) | (pixel[2] >> 6)) << 20 |  // B
            ((pixel[1] << 2) | (pixel[1] >> 6)) << 10 |  // G
            ((pixel[0] << 2) | (pixel[0] >> 6));         // R
      }
      return;
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::UYVY_422:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

GLenum InternalFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED;
    case gfx::BufferFormat::R_16:
      return GL_R16_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG;
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBX_1010102:
      return GL_RGB;
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
      return GL_RGBA;
    case gfx::BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::RGBA_F16:
      return GL_RGBA;
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::UYVY_422:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

// An end to end test that tests the whole GpuMemoryBuffer lifecycle.
TEST_P(GpuMemoryBufferTest, Lifecycle) {
  if (GetParam() == gfx::BufferFormat::R_8 &&
      !gl_.GetCapabilities().texture_rg) {
    LOG(WARNING) << "texture_rg not supported. Skipping test.";
    return;
  }

  if (GetParam() == gfx::BufferFormat::RGBA_F16 &&
      !gl_.GetCapabilities().texture_half_float_linear) {
    LOG(WARNING) << "texture_half_float_linear not supported. Skipping test.";
    return;
  }

  if (GetParam() == gfx::BufferFormat::RGBX_1010102 &&
      !gl_.GetCapabilities().image_xb30) {
    LOG(WARNING) << "image_xb30 not supported. Skipping test.";
    return;
  }

  GLuint texture_id = 0;
  glGenTextures(1, &texture_id);
  ASSERT_NE(0u, texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  // Create the gpu memory buffer.
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(gl_.CreateGpuMemoryBuffer(
      gfx::Size(kImageWidth, kImageHeight), GetParam()));

  // Map buffer for writing.
  ASSERT_TRUE(buffer->Map());
  ASSERT_NE(nullptr, buffer->memory(0));
  ASSERT_NE(0, buffer->stride(0));
  uint8_t pixel[] = {255u, 0u, 0u, 255u};

  // Assign a value to each pixel.
  for (int y = 0; y < kImageHeight; ++y) {
    SetRow(GetParam(),
           static_cast<uint8_t*>(buffer->memory(0)) + y * buffer->stride(0),
           kImageWidth, pixel);
  }
  // Unmap the buffer.
  buffer->Unmap();

  // Create the image. This should add the image ID to the ImageManager.
  GLuint image_id =
      glCreateImageCHROMIUM(buffer->AsClientBuffer(), kImageWidth, kImageHeight,
                            InternalFormat(GetParam()));
  ASSERT_NE(0u, image_id);
  ASSERT_TRUE(gl_.decoder()->GetImageManagerForTest()->LookupImage(image_id) !=
              nullptr);

  // Bind the image.
  glBindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);

  // Build program, buffers and draw the texture.
  GLuint vertex_shader =
      GLTestHelper::LoadShader(GL_VERTEX_SHADER, kVertexShader);
  GLuint fragment_shader =
      GLTestHelper::LoadShader(GL_FRAGMENT_SHADER, kFragmentShader);
  GLuint program = GLTestHelper::SetupProgram(vertex_shader, fragment_shader);
  ASSERT_NE(0u, program);
  glUseProgram(program);

  GLint sampler_location = glGetUniformLocation(program, "a_texture");
  ASSERT_NE(-1, sampler_location);
  glUniform1i(sampler_location, 0);

  GLuint vbo =
      GLTestHelper::SetupUnitQuad(glGetAttribLocation(program, "a_position"));
  ASSERT_NE(0u, vbo);
  glViewport(0, 0, kImageWidth, kImageHeight);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  ASSERT_TRUE(glGetError() == GL_NO_ERROR);

  // Check if pixels match the values that were assigned to the mapped buffer.
  GLTestHelper::CheckPixels(0, 0, kImageWidth, kImageHeight, 0, pixel, nullptr);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // Release the image.
  glReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);

  // Clean up.
  glDeleteProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteBuffers(1, &vbo);
  glDestroyImageCHROMIUM(image_id);
  glDeleteTextures(1, &texture_id);
}

#if defined(OS_LINUX)
// Test glCreateImageCHROMIUM with gfx::NATIVE_PIXMAP. Basically the test
// reproduces the situation where some dmabuf fds are available outside the
// gpu process and the user wants to import them using glCreateImageCHROMIUM.
// It can be the case when vaapi is setup in a media service hosted in a
// dedicated process, i.e. not the gpu process.
TEST_F(GpuMemoryBufferTestEGL, GLCreateImageCHROMIUMFromNativePixmap) {
  SKIP_TEST_IF(!egl_gles2_initialized_);

  // This extension is required for glCreateImageCHROMIUM on Linux.
  SKIP_TEST_IF(!HasEGLExtension("EGL_EXT_image_dma_buf_import"));

  // This extension is required for the test to work but not for the real
  // world, see CreateNativePixmapHandle.
  SKIP_TEST_IF(!HasEGLExtension("EGL_MESA_image_dma_buf_export"));

  // This extension is required for glCreateImageCHROMIUM on Linux.
  SKIP_TEST_IF(!HasGLExtension("GL_OES_EGL_image"));

  gfx::BufferFormat format = gfx::BufferFormat::RGBX_8888;
  gfx::Size size(kImageWidth, kImageHeight);
  size_t buffer_size = gfx::BufferSizeForBufferFormat(size, format);
  uint8_t pixel[] = {255u, 0u, 0u, 255u};
  size_t plane = 0;
  uint32_t stride = gfx::RowSizeForBufferFormat(size.width(), format, plane);

  std::unique_ptr<uint8_t[]> pixels(new uint8_t[buffer_size]);

  // Assign a value to each pixel.
  for (int y = 0; y < size.height(); ++y) {
    SetRow(format, pixels.get() + y * stride, size.width(), pixel);
  }

  // A real use case would be to export a VAAPI surface as dmabuf fds. But for
  // simplicity the test gets them from a GL texture.
  gfx::NativePixmapHandle native_pixmap_handle =
      CreateNativePixmapHandle(format, size, pixels.get());
  EXPECT_EQ(1u, native_pixmap_handle.fds.size());

  // Initialize a GpuMemoryBufferHandle to wrap a native pixmap.
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  handle.native_pixmap_handle = native_pixmap_handle;
  EXPECT_TRUE(handle.id.is_valid());

  // Create a GMB to pass to glCreateImageCHROMIUM.
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer =
      gpu::GpuMemoryBufferImplNativePixmap::CreateFromHandle(
          native_pixmap_factory_.get(), handle, size, format,
          gfx::BufferUsage::SCANOUT,
          base::RepeatingCallback<void(const gpu::SyncToken&)>());
  EXPECT_NE(nullptr, buffer.get());
  EXPECT_TRUE(buffer->GetId().is_valid());

  // Create the image. This should add the image ID to the ImageManager.
  GLuint image_id = glCreateImageCHROMIUM(buffer->AsClientBuffer(),
                                          size.width(), size.height(), GL_RGB);
  EXPECT_NE(0u, image_id);
  // In the tests the gl::GLImage is added into the ImageManager when calling
  // GLManager::CreateImage. In real cases the GpuControl would not own the
  // ImageManager. I.e. for the tests the ImageManager lives in the client side
  // so there is no need to call glShallowFinishCHROMIUM().
  EXPECT_TRUE(gl_.decoder()->GetImageManagerForTest()->LookupImage(image_id) !=
              nullptr);
  ASSERT_TRUE(glGetError() == GL_NO_ERROR);

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

  // Bind the image.
  glBindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);

  // Build program, buffers and draw the texture.
  GLuint vertex_shader =
      GLTestHelper::LoadShader(GL_VERTEX_SHADER, kVertexShader);
  GLuint fragment_shader =
      GLTestHelper::LoadShader(GL_FRAGMENT_SHADER, kFragmentShader);
  GLuint program = GLTestHelper::SetupProgram(vertex_shader, fragment_shader);
  ASSERT_NE(0u, program);
  glUseProgram(program);

  GLint sampler_location = glGetUniformLocation(program, "a_texture");
  ASSERT_NE(-1, sampler_location);
  glUniform1i(sampler_location, 0);

  GLuint vbo =
      GLTestHelper::SetupUnitQuad(glGetAttribLocation(program, "a_position"));
  ASSERT_NE(0u, vbo);
  glViewport(0, 0, kImageWidth, kImageHeight);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  ASSERT_TRUE(glGetError() == GL_NO_ERROR);

  // Check if pixels match the values that were assigned to the mapped buffer.
  GLTestHelper::CheckPixels(0, 0, kImageWidth, kImageHeight, 0, pixel, nullptr);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // Release the image.
  glReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);

  // Clean up.
  glDeleteProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteBuffers(1, &vbo);
  glDestroyImageCHROMIUM(image_id);
  glDeleteTextures(1, &texture_id);
}
#endif  // defined(OS_LINUX)

INSTANTIATE_TEST_CASE_P(GpuMemoryBufferTests,
                        GpuMemoryBufferTest,
                        ::testing::Values(gfx::BufferFormat::R_8,
                                          gfx::BufferFormat::BGR_565,
                                          gfx::BufferFormat::RGBA_4444,
                                          gfx::BufferFormat::RGBA_8888,
                                          gfx::BufferFormat::RGBX_1010102,
                                          gfx::BufferFormat::BGRA_8888,
                                          gfx::BufferFormat::RGBA_F16));

}  // namespace gles2
}  // namespace gpu

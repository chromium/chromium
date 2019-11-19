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
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/half_float.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/test/gl_image_test_support.h"

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
    case gfx::BufferFormat::BGRX_1010102:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::RGBA_F16:
      return GL_RGBA;
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::P010:
      NOTREACHED() << gfx::BufferFormatToString(format);
      return 0;
  }

  NOTREACHED();
  return 0;
}

uint32_t BufferFormatToFourCC(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
      return libyuv::FOURCC_ANY;  // libyuv::FOURCC_RGBP has wrong endianness.
    case gfx::BufferFormat::RGBA_4444:
      return libyuv::FOURCC_ANY;  // libyuv::FOURCC_R444 has wrong endianness.
    case gfx::BufferFormat::RGBA_8888:
      return libyuv::FOURCC_ABGR;
    case gfx::BufferFormat::BGRA_8888:
      return libyuv::FOURCC_ARGB;
    case gfx::BufferFormat::RGBX_1010102:
      return libyuv::FOURCC_AB30;
    case gfx::BufferFormat::BGRX_1010102:
      return libyuv::FOURCC_AR30;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return libyuv::FOURCC_NV12;
    case gfx::BufferFormat::YVU_420:
      return libyuv::FOURCC_YV12;
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::P010:
      return libyuv::FOURCC_ANY;
  }
  NOTREACHED();
  return libyuv::FOURCC_ANY;
}

}  // namespace

// Verifies that the read-back colour after map-write-unmap is the original.
TEST_P(GpuMemoryBufferTest, MapUnmap) {
  const gfx::BufferFormat buffer_format = GetParam();
  const uint32_t libyuv_fourcc = BufferFormatToFourCC(buffer_format);
  if (libyuv_fourcc == static_cast<uint32_t>(libyuv::FOURCC_ANY)) {
    LOG(WARNING) << gfx::BufferFormatToString(buffer_format)
                 << " not supported, skipping test";
    return;
  }

  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(gl_.CreateGpuMemoryBuffer(
      gfx::Size(kImageWidth, kImageHeight), buffer_format));

  ASSERT_TRUE(buffer->Map());
  ASSERT_NE(nullptr, buffer->memory(0));
  ASSERT_NE(0, buffer->stride(0));
  constexpr uint8_t color_rgba[] = {127u, 0u, 0u, 255u};
  constexpr uint8_t color_bgra[] = {0u, 0u, 127u, 255u};

  const size_t num_planes = NumberOfPlanesForLinearBufferFormat(buffer_format);
  for (size_t plane = 0; plane < num_planes; ++plane) {
    gl::GLImageTestSupport::SetBufferDataToColor(
        kImageWidth, kImageHeight, buffer->stride(plane), plane, buffer_format,
        color_rgba, static_cast<uint8_t*>(buffer->memory(plane)));
  }
  buffer->Unmap();

  ASSERT_TRUE(buffer->Map());
  ASSERT_NE(nullptr, buffer->memory(0));
  ASSERT_NE(0, buffer->stride(0));
  const uint8_t* data = static_cast<uint8_t*>(buffer->memory(0));
  const int stride = buffer->stride(0);
  // libyuv defines the formats as word-order.
  uint8_t argb[kImageWidth * kImageHeight * 4] = {};
  const int result = libyuv::ConvertToARGB(
      data, stride * kImageWidth, argb, kImageWidth /* dst_stride_argb */,
      0 /* crop_x */, 0 /* crop_y */, kImageWidth, kImageHeight,
      kImageWidth /* rop_width */, kImageHeight /* crop_height */,
      libyuv::kRotate0, libyuv_fourcc);

  constexpr int max_error = 2;
  ASSERT_EQ(result, 0) << gfx::BufferFormatToString(buffer_format);
  int bad_count = 0;
  for (int y = 0; y < kImageHeight; ++y) {
    for (int x = 0; x < kImageWidth; ++x) {
      int offset = y * kImageWidth + x * 4;
      for (int c = 0; c < 4; ++c) {
        // |argb| in word order is read as B, G, R, A on little endian .
        const uint8_t actual = argb[offset + c];
        const uint8_t expected = color_bgra[c];
        EXPECT_NEAR(expected, actual, max_error)
            << " at " << x << ", " << y << " channel " << c;
        bad_count += std::abs(actual - expected) > max_error;
        // Exit early just so we don't spam the log but we print enough to
        // hopefully make it easy to diagnose the issue.
        ASSERT_LE(bad_count, 4);
      }
    }
  }
  buffer->Unmap();
}

// An end to end test that tests the whole GpuMemoryBuffer lifecycle.
TEST_P(GpuMemoryBufferTest, Lifecycle) {
  const gfx::BufferFormat buffer_format = GetParam();

  if (buffer_format == gfx::BufferFormat::R_8 &&
      !gl_.GetCapabilities().texture_rg) {
    LOG(WARNING) << "texture_rg not supported. Skipping test.";
    return;
  }

  if (buffer_format == gfx::BufferFormat::RGBA_F16 &&
      !gl_.GetCapabilities().texture_half_float_linear) {
    LOG(WARNING) << "texture_half_float_linear not supported. Skipping test.";
    return;
  }

  if (buffer_format == gfx::BufferFormat::RGBX_1010102 &&
      !gl_.GetCapabilities().image_xb30) {
    LOG(WARNING) << "image_xb30 not supported. Skipping test.";
    return;
  }

  if (buffer_format == gfx::BufferFormat::BGRX_1010102 &&
      !gl_.GetCapabilities().image_xr30) {
    LOG(WARNING) << "image_xr30 not supported. Skipping test.";
    return;
  }

  if (buffer_format == gfx::BufferFormat::YVU_420 ||
      buffer_format == gfx::BufferFormat::YUV_420_BIPLANAR) {
    LOG(WARNING) << "GLImageMemory doesn't support YUV formats, skipping test.";
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
      gfx::Size(kImageWidth, kImageHeight), buffer_format));

  // Map buffer for writing.
  ASSERT_TRUE(buffer->Map());
  ASSERT_NE(nullptr, buffer->memory(0));
  ASSERT_NE(0, buffer->stride(0));
  constexpr uint8_t pixel[] = {255u, 0u, 0u, 255u};

  const size_t num_planes = NumberOfPlanesForLinearBufferFormat(buffer_format);
  for (size_t plane = 0; plane < num_planes; ++plane) {
    gl::GLImageTestSupport::SetBufferDataToColor(
        kImageWidth, kImageHeight, buffer->stride(plane), plane, buffer_format,
        pixel, static_cast<uint8_t*>(buffer->memory(0)));
  }
  buffer->Unmap();

  // Create the image. This should add the image ID to the ImageManager.
  GLuint image_id =
      glCreateImageCHROMIUM(buffer->AsClientBuffer(), kImageWidth, kImageHeight,
                            InternalFormat(buffer_format));
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

  gl::GLImageTestSupport::SetBufferDataToColor(kImageWidth, kImageHeight,
                                               stride, 0 /* plane */, format,
                                               pixel, pixels.get());

  // A real use case would be to export a VAAPI surface as dmabuf fds. But for
  // simplicity the test gets them from a GL texture.
  gfx::NativePixmapHandle native_pixmap_handle =
      CreateNativePixmapHandle(format, size, pixels.get());
  EXPECT_EQ(1u, native_pixmap_handle.planes.size());

  // Initialize a GpuMemoryBufferHandle to wrap a native pixmap.
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  handle.native_pixmap_handle = std::move(native_pixmap_handle);
  EXPECT_TRUE(handle.id.is_valid());

  // Create a GMB to pass to glCreateImageCHROMIUM.
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer =
      gpu::GpuMemoryBufferImplNativePixmap::CreateFromHandle(
          native_pixmap_factory_.get(), std::move(handle), size, format,
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

INSTANTIATE_TEST_SUITE_P(
    GpuMemoryBufferTests,
    GpuMemoryBufferTest,
    ::testing::Values(gfx::BufferFormat::R_8,
                      gfx::BufferFormat::BGR_565,
                      gfx::BufferFormat::RGBA_4444,
                      gfx::BufferFormat::RGBA_8888,
                      gfx::BufferFormat::RGBX_1010102,
                      gfx::BufferFormat::BGRX_1010102,
                      gfx::BufferFormat::BGRA_8888,
                      gfx::BufferFormat::RGBA_F16,
                      gfx::BufferFormat::YVU_420,
                      gfx::BufferFormat::YUV_420_BIPLANAR));

}  // namespace gles2
}  // namespace gpu

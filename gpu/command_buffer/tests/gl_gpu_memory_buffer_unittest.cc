// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2chromium.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/half_float.h"
#include "ui/gl/test/gl_test_support.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
class GpuMemoryBufferTestEGL : public testing::Test,
                               public gpu::GpuCommandBufferTestEGL {
 public:
  GpuMemoryBufferTestEGL()
      : native_pixmap_factory_(gfx::CreateClientNativePixmapFactoryDmabuf()) {}

 protected:
  void SetUp() override {
    egl_initialized_ = InitializeEGL(kImageWidth, kImageHeight);
    gl_.set_use_native_pixmap_memory_buffers(true);
  }

  void TearDown() override { RestoreGLDefault(); }

  bool egl_initialized_{false};
  std::unique_ptr<gfx::ClientNativePixmapFactory> native_pixmap_factory_;
};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace {

#define SHADER(Src) #Src

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
    case gfx::BufferFormat::RGBA_1010102:
      return libyuv::FOURCC_AB30;
    case gfx::BufferFormat::BGRA_1010102:
      return libyuv::FOURCC_AR30;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return libyuv::FOURCC_NV12;
    case gfx::BufferFormat::YVU_420:
      return libyuv::FOURCC_YV12;
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RG_1616:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
    case gfx::BufferFormat::P010:
      return libyuv::FOURCC_ANY;
  }
  NOTREACHED_IN_MIGRATION();
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
    gl::GLTestSupport::SetBufferDataToColor(
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

INSTANTIATE_TEST_SUITE_P(
    GpuMemoryBufferTests,
    GpuMemoryBufferTest,
    ::testing::Values(gfx::BufferFormat::R_8,
                      gfx::BufferFormat::BGR_565,
                      gfx::BufferFormat::RGBA_4444,
                      gfx::BufferFormat::RGBA_8888,
                      gfx::BufferFormat::RGBA_1010102,
                      gfx::BufferFormat::BGRA_1010102,
                      gfx::BufferFormat::BGRA_8888,
                      gfx::BufferFormat::RGBA_F16,
                      gfx::BufferFormat::YVU_420,
                      gfx::BufferFormat::YUV_420_BIPLANAR));

}  // namespace gles2
}  // namespace gpu

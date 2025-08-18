// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_gl_surface_renderer.h"

#include <android/native_window.h>
#include <media/NdkImageReader.h>

#include "base/android/build_info.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_converter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace media {

namespace {
constexpr gfx::Size kSurfaceSize(100, 100);
}  // namespace

class VideoFrameGLSurfaceRendererTest
    : public testing::TestWithParam<VideoPixelFormat> {
 public:
  VideoFrameGLSurfaceRendererTest() = default;
  ~VideoFrameGLSurfaceRendererTest() override = default;

 protected:
  void SetUp() override {
    if (__builtin_available(android 35, *)) {
      // Negation of __builtin_available results in compiler warning.
    } else {
      GTEST_SKIP() << "Not supported Android version. "
                   << "This test needs Android 15 and up.";
    }

    ASSERT_TRUE(gl::init::InitializeGLOneOff(gl::GpuPreference::kDefault));

    AImageReader* image_reader_ptr = nullptr;
    media_status_t ret = AImageReader_new(
        kSurfaceSize.width(), kSurfaceSize.height(), AIMAGE_FORMAT_RGBA_8888,
        /*maxImages=*/1, &image_reader_ptr);
    image_reader_ = image_reader_ptr;
    ASSERT_EQ(ret, AMEDIA_OK);

    ANativeWindow* native_window = nullptr;
    ret = AImageReader_getWindow(image_reader_, &native_window);
    ASSERT_EQ(ret, AMEDIA_OK);

    image_listener_ = {
        .context = this,
        .onImageAvailable = &VideoFrameGLSurfaceRendererTest::OnImageAvailable,
    };
    ret = AImageReader_setImageListener(image_reader_, &image_listener_);
    ASSERT_EQ(ret, AMEDIA_OK);

    renderer_ = std::make_unique<VideoFrameGLSurfaceRenderer>(
        gl::ScopedANativeWindow::Wrap(native_window));
    ASSERT_TRUE(renderer_->Initialize().is_ok());
  }

  void TearDown() override {
    renderer_.reset();
    if (image_reader_) {
      AImageReader_delete(image_reader_);
      image_reader_ = nullptr;
    }
    gl::init::ShutdownGL(nullptr, false);
  }

  scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat format,
                                        const gfx::Size& size,
                                        uint32_t color = 0xFFFFFF) {
    auto frame = VideoFrame::CreateFrame(format, size, gfx::Rect(size), size,
                                         base::TimeDelta());
    EXPECT_TRUE(frame);
    frame->set_color_space(gfx::ColorSpace::CreateREC601());
    FillFourColors(*frame, color);
    return frame;
  }

  static void OnImageAvailable(void* context, AImageReader* reader) {
    auto* test_instance =
        static_cast<VideoFrameGLSurfaceRendererTest*>(context);
    if (test_instance->image_available_closure_) {
      std::move(test_instance->image_available_closure_).Run();
    }
  }

  base::OnceClosure image_available_closure_;
  base::test::TaskEnvironment task_environment_;
  raw_ptr<AImageReader> image_reader_ = nullptr;
  std::unique_ptr<VideoFrameGLSurfaceRenderer> renderer_;
  VideoFrameConverter frame_converter_;
  AImageReader_ImageListener image_listener_{};
};

TEST_P(VideoFrameGLSurfaceRendererTest, RenderFrame) {
  auto yuv_frame = CreateFrame(GetParam(), kSurfaceSize);
  ASSERT_TRUE(yuv_frame);

  // Rendering to a surface is asynchronous. We need to wait until the frame is
  // available in the AImageReader before we can acquire it. The
  // OnImageAvailable callback will signal us when it's ready.
  base::RunLoop run_loop;
  image_available_closure_ = run_loop.QuitClosure();
  EXPECT_TRUE(
      renderer_->RenderVideoFrame(yuv_frame, base::TimeTicks::Now()).is_ok());
  run_loop.Run();

  AImage* image = nullptr;
  media_status_t ret = AImageReader_acquireLatestImage(image_reader_, &image);
  ASSERT_EQ(ret, AMEDIA_OK);
  ASSERT_TRUE(image);

  int32_t width, height, format;
  AImage_getWidth(image, &width);
  AImage_getHeight(image, &height);
  AImage_getFormat(image, &format);
  EXPECT_EQ(width, kSurfaceSize.width());
  EXPECT_EQ(height, kSurfaceSize.height());
  EXPECT_EQ(format, AIMAGE_FORMAT_RGBA_8888);

  int32_t row_stride = 0;
  AImage_getPlaneRowStride(image, 0, &row_stride);
  ASSERT_GT(row_stride, 0);

  uint8_t* data = nullptr;
  int len = 0;
  AImage_getPlaneData(image, 0, &data, &len);
  EXPECT_TRUE(data);
  ASSERT_GE(len, kSurfaceSize.height() * row_stride);

  // SAFETY: The pointer and the size are provided by ImageReader API,
  // we have to assume they are correct.
  base::span<const uint8_t> image_data =
      UNSAFE_BUFFERS(base::span(data, static_cast<size_t>(len)));

  const auto layout = VideoFrameLayout::CreateWithStrides(
      PIXEL_FORMAT_XBGR, kSurfaceSize, {static_cast<size_t>(row_stride)});
  ASSERT_TRUE(layout);
  auto rendered_rgb_frame = VideoFrame::WrapExternalDataWithLayout(
      *layout, gfx::Rect(kSurfaceSize), kSurfaceSize, image_data,
      base::TimeDelta());
  ASSERT_TRUE(rendered_rgb_frame);

  auto converted_yuv_frame =
      VideoFrame::CreateFrame(GetParam(), kSurfaceSize, gfx::Rect(kSurfaceSize),
                              kSurfaceSize, base::TimeDelta());
  ASSERT_TRUE(converted_yuv_frame);
  ASSERT_TRUE(frame_converter_
                  .ConvertAndScale(*rendered_rgb_frame, *converted_yuv_frame)
                  .is_ok());

  int different_pixels =
      CountDifferentPixels(*yuv_frame, *converted_yuv_frame, 10);
  EXPECT_LT(different_pixels, width * 2);

  AImage_delete(image);
}

INSTANTIATE_TEST_SUITE_P(VideoFrameGLSurfaceRendererTest,
                         VideoFrameGLSurfaceRendererTest,
                         ::testing::Values(PIXEL_FORMAT_I420,
                                           PIXEL_FORMAT_NV12));

}  // namespace media

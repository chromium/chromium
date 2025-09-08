// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_gl_surface_renderer.h"

#include <android/native_window.h>
#include <media/NdkImageReader.h>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/ahardwarebuffer_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_converter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
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
    SetupSharedImages();

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

    auto status = renderer_->Initialize();
    ASSERT_TRUE(status.is_ok())
        << EncoderStatusCodeToString(status.code()) << " " << status.message();
    renderer_->SetSharedImageManager(&shared_image_manager_);
  }

  void SetupSharedImages() {
    gpu::GpuPreferences gpu_preferences;
    gpu::GpuDriverBugWorkarounds gpu_workarounds;
    gpu::GpuFeatureInfo gpu_feature_info;
    gl_surface_ = gl::init::CreateOffscreenGLSurface(
        gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size());
    ASSERT_TRUE(gl_surface_);
    gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                            gl::GLContextAttribs());
    ASSERT_TRUE(gl_context_);
    ASSERT_TRUE(gl_context_->MakeCurrent(gl_surface_.get()));

    context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
        base::MakeRefCounted<gl::GLShareGroup>(), gl_surface_, gl_context_,
        /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
        gpu::GrContextType::kGL);
    ASSERT_TRUE(context_state_->InitializeGL(
        gpu_preferences, base::MakeRefCounted<gpu::gles2::FeatureInfo>(
                             gpu_workarounds, gpu_feature_info)));

    backing_factory_ =
        std::make_unique<gpu::AHardwareBufferImageBackingFactory>(
            context_state_->feature_info(), gpu_preferences,
            context_state_->vk_context_provider());
  }

  void TearDown() override {
    renderer_.reset();
    if (image_reader_) {
      AImageReader_delete(image_reader_);
      image_reader_ = nullptr;
    }

    if (context_state_) {
      context_state_->MakeCurrent(gl_surface_.get(), true);
    }
    context_state_.reset();
    gl_context_.reset();
    gl_surface_.reset();

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

  void RenderAndVerifyFrame(
      scoped_refptr<VideoFrame> input_frame,
      scoped_refptr<VideoFrame> expected_frame = nullptr) {
    ASSERT_TRUE(input_frame);
    VideoPixelFormat format = input_frame->format();

    // Rendering to a surface is asynchronous. We need to wait until the frame
    // is available in the AImageReader before we can acquire it. The
    // OnImageAvailable callback will signal us when it's ready.
    base::RunLoop run_loop;
    image_available_closure_ = run_loop.QuitClosure();
    auto render_status =
        renderer_->RenderVideoFrame(input_frame, base::TimeTicks::Now());
    ASSERT_TRUE(render_status.is_ok())
        << EncoderStatusCodeToString(render_status.code()) << " "
        << render_status.message();

    run_loop.Run();

    AImage* image = nullptr;
    media_status_t ret = AImageReader_acquireLatestImage(image_reader_, &image);
    ASSERT_EQ(ret, AMEDIA_OK);
    ASSERT_TRUE(image);

    int32_t width, height, image_format;
    AImage_getWidth(image, &width);
    AImage_getHeight(image, &height);
    AImage_getFormat(image, &image_format);
    EXPECT_EQ(width, kSurfaceSize.width());
    EXPECT_EQ(height, kSurfaceSize.height());
    EXPECT_EQ(image_format, AIMAGE_FORMAT_RGBA_8888);

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

    scoped_refptr<VideoFrame> frame_to_compare;
    if (IsYuvPlanar(format)) {
      auto converted_frame =
          VideoFrame::CreateFrame(format, kSurfaceSize, gfx::Rect(kSurfaceSize),
                                  kSurfaceSize, base::TimeDelta());
      ASSERT_TRUE(converted_frame);
      ASSERT_TRUE(frame_converter_
                      .ConvertAndScale(*rendered_rgb_frame, *converted_frame)
                      .is_ok());
      frame_to_compare = converted_frame;
    } else if (format == PIXEL_FORMAT_XRGB) {
      auto converted_frame = VideoFrame::CreateFrame(
          PIXEL_FORMAT_XRGB, kSurfaceSize, gfx::Rect(kSurfaceSize),
          kSurfaceSize, base::TimeDelta());
      ASSERT_TRUE(converted_frame);
      libyuv::ABGRToARGB(
          rendered_rgb_frame->visible_data(0), rendered_rgb_frame->stride(0),
          converted_frame->writable_data(0), converted_frame->stride(0),
          kSurfaceSize.width(), kSurfaceSize.height());
      frame_to_compare = converted_frame;
    } else {
      frame_to_compare = rendered_rgb_frame;
    }

    expected_frame = expected_frame ? expected_frame : input_frame;
    int different_pixels =
        CountDifferentPixels(*expected_frame, *frame_to_compare, 10);
    EXPECT_LT(different_pixels, width * 3)
        << " pixel format: " << VideoPixelFormatToString(format);

    AImage_delete(image);
  }

  base::OnceClosure image_available_closure_;
  base::test::TaskEnvironment task_environment_;
  raw_ptr<AImageReader> image_reader_ = nullptr;
  std::unique_ptr<VideoFrameGLSurfaceRenderer> renderer_;
  VideoFrameConverter frame_converter_;
  AImageReader_ImageListener image_listener_{};

  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;
  scoped_refptr<gpu::SharedContextState> context_state_;
  gpu::SharedImageManager shared_image_manager_{/*thread_safe=*/false};
  std::unique_ptr<gpu::SharedImageBackingFactory> backing_factory_;
  scoped_refptr<gpu::MemoryTracker> memory_tracker_ =
      base::MakeRefCounted<gpu::MemoryTracker>();
  gpu::MemoryTypeTracker memory_type_tracker_{memory_tracker_.get()};
};

TEST_P(VideoFrameGLSurfaceRendererTest, RenderFrame) {
  RenderAndVerifyFrame(CreateFrame(GetParam(), kSurfaceSize, 0xFFFFFF));
}

TEST_F(VideoFrameGLSurfaceRendererTest, RenderMixedFrameFormats) {
  RenderAndVerifyFrame(CreateFrame(PIXEL_FORMAT_I420, kSurfaceSize, 0xFF0000));
  RenderAndVerifyFrame(CreateFrame(PIXEL_FORMAT_XRGB, kSurfaceSize, 0x00FF00));
  RenderAndVerifyFrame(CreateFrame(PIXEL_FORMAT_NV12, kSurfaceSize, 0x0000FF));
  RenderAndVerifyFrame(CreateFrame(PIXEL_FORMAT_XBGR, kSurfaceSize, 0xFFFF00));
  RenderAndVerifyFrame(CreateFrame(PIXEL_FORMAT_I420, kSurfaceSize, 0x00FFFF));
  RenderAndVerifyFrame(CreateFrame(PIXEL_FORMAT_NV12, kSurfaceSize, 0xFF00FF));
}

TEST_F(VideoFrameGLSurfaceRendererTest, RenderSharedImageVideoFrame) {
  auto mailbox = gpu::Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  auto sync_token = gpu::SyncToken();
  gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto expected_frame = CreateFrame(PIXEL_FORMAT_XBGR, kSurfaceSize, 0xFF00FF);
  ASSERT_TRUE(expected_frame);

  // Create a SharedImage-backed frame from the software frame's data.
  // AHardwareBufferImageBackingFactory expects tightly packed data. Create a
  // buffer and copy the frame data into it.
  const size_t row_bytes = kSurfaceSize.width() * 4;
  const size_t pixel_data_size = row_bytes * kSurfaceSize.height();
  std::vector<uint8_t> pixel_data(pixel_data_size);
  libyuv::CopyPlane(expected_frame->data(VideoFrame::Plane::kARGB),
                    expected_frame->stride(VideoFrame::Plane::kARGB),
                    pixel_data.data(), row_bytes, row_bytes,
                    kSurfaceSize.height());

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, kSurfaceSize, color_space, surface_origin, alpha_type,
      usage, "TestLabel", /*is_thread_safe=*/false, pixel_data);
  ASSERT_TRUE(backing);

  auto factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  ASSERT_TRUE(factory_ref);

  gpu::SharedImageInfo si_info(format, kSurfaceSize, color_space,
                               surface_origin, alpha_type, usage, "TestLabel");

  auto test_ssi = base::MakeRefCounted<gpu::TestSharedImageInterface>();
  auto client_shared_image = base::MakeRefCounted<gpu::ClientSharedImage>(
      mailbox, si_info, sync_token,
      base::MakeRefCounted<gpu::SharedImageInterfaceHolder>(test_ssi.get()),
      gfx::EMPTY_BUFFER);

  auto si_video_frame = VideoFrame::WrapSharedImage(
      PIXEL_FORMAT_XBGR, client_shared_image, sync_token, base::DoNothing(),
      kSurfaceSize, gfx::Rect(kSurfaceSize), kSurfaceSize, base::TimeDelta());
  ASSERT_TRUE(si_video_frame);

  RenderAndVerifyFrame(si_video_frame, expected_frame);
  RenderAndVerifyFrame(expected_frame, expected_frame);
  RenderAndVerifyFrame(si_video_frame, expected_frame);
}

std::string PrintTestParams(
    const testing::TestParamInfo<VideoPixelFormat>& info) {
  return VideoPixelFormatToString(info.param);
}

INSTANTIATE_TEST_SUITE_P(VideoFrameGLSurfaceRendererTest,
                         VideoFrameGLSurfaceRendererTest,
                         ::testing::Values(PIXEL_FORMAT_I420,
                                           PIXEL_FORMAT_NV12,
                                           PIXEL_FORMAT_XRGB,
                                           PIXEL_FORMAT_XBGR),
                         PrintTestParams);

}  // namespace media

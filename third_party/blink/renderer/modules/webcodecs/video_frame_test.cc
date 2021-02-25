// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_handle.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

ImageBitmap* ToImageBitmap(V8TestingScope* v8_scope, ScriptValue value) {
  return NativeValueTraits<ImageBitmap>::NativeValue(
      v8_scope->GetIsolate(), value.V8Value(), v8_scope->GetExceptionState());
}

class VideoFrameTest : public testing::Test {
 public:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::Create();
    InitializeSharedGpuContext(test_context_provider_.get());
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }

  VideoFrame* CreateBlinkVideoFrame(
      scoped_refptr<media::VideoFrame> media_frame,
      ExecutionContext* context) {
    return MakeGarbageCollected<VideoFrame>(std::move(media_frame), context);
  }
  VideoFrame* CreateBlinkVideoFrameFromHandle(
      scoped_refptr<VideoFrameHandle> handle) {
    return MakeGarbageCollected<VideoFrame>(std::move(handle));
  }
  scoped_refptr<media::VideoFrame> CreateDefaultBlackMediaVideoFrame() {
    return CreateBlackMediaVideoFrame(base::TimeDelta::FromMicroseconds(1000),
                                      media::PIXEL_FORMAT_I420,
                                      gfx::Size(112, 208) /* coded_size */,
                                      gfx::Size(100, 200) /* visible_size */);
  }

  scoped_refptr<media::VideoFrame> CreateBlackMediaVideoFrame(
      base::TimeDelta timestamp,
      media::VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Size& visible_size) {
    scoped_refptr<media::VideoFrame> media_frame =
        media::VideoFrame::WrapVideoFrame(
            media::VideoFrame::CreateBlackFrame(coded_size), format,
            gfx::Rect(visible_size) /* visible_rect */,
            visible_size /* natural_size */);
    media_frame->set_timestamp(timestamp);
    return media_frame;
  }

 private:
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
};

TEST_F(VideoFrameTest, ConstructorAndAttributes) {
  V8TestingScope scope;

  scoped_refptr<media::VideoFrame> media_frame = CreateBlackMediaVideoFrame(
      base::TimeDelta::FromMicroseconds(1000), media::PIXEL_FORMAT_I420,
      gfx::Size(112, 208) /* coded_size */,
      gfx::Size(100, 200) /* visible_size */);
  VideoFrame* blink_frame =
      CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());

  EXPECT_EQ(1000u, blink_frame->timestamp().value());
  EXPECT_EQ(112u, blink_frame->codedWidth());
  EXPECT_EQ(208u, blink_frame->codedHeight());
  EXPECT_EQ(100u, blink_frame->cropWidth());
  EXPECT_EQ(200u, blink_frame->cropHeight());
  EXPECT_EQ(media_frame, blink_frame->frame());

  blink_frame->close();

  EXPECT_FALSE(blink_frame->timestamp().has_value());
  EXPECT_EQ(0u, blink_frame->codedWidth());
  EXPECT_EQ(0u, blink_frame->codedHeight());
  EXPECT_EQ(0u, blink_frame->cropWidth());
  EXPECT_EQ(0u, blink_frame->cropHeight());
  EXPECT_EQ(nullptr, blink_frame->frame());
}

TEST_F(VideoFrameTest, FramesSharingHandleClose) {
  V8TestingScope scope;

  scoped_refptr<media::VideoFrame> media_frame =
      CreateDefaultBlackMediaVideoFrame();
  VideoFrame* blink_frame =
      CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());

  VideoFrame* frame_with_shared_handle =
      CreateBlinkVideoFrameFromHandle(blink_frame->handle());

  // A blink::VideoFrame created from a handle should share the same
  // media::VideoFrame reference.
  EXPECT_EQ(media_frame, frame_with_shared_handle->frame());

  // Closing a frame should invalidate all frames sharing the same handle.
  blink_frame->close();
  EXPECT_EQ(nullptr, frame_with_shared_handle->frame());
}

TEST_F(VideoFrameTest, FramesNotSharingHandleClose) {
  V8TestingScope scope;

  scoped_refptr<media::VideoFrame> media_frame =
      CreateDefaultBlackMediaVideoFrame();
  VideoFrame* blink_frame =
      CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());

  auto new_handle = base::MakeRefCounted<VideoFrameHandle>(
      blink_frame->frame(), scope.GetExecutionContext());

  VideoFrame* frame_with_new_handle =
      CreateBlinkVideoFrameFromHandle(std::move(new_handle));

  EXPECT_EQ(media_frame, frame_with_new_handle->frame());

  // If a frame was created a new handle reference the same media::VideoFrame,
  // one frame's closure should not affect the other.
  blink_frame->close();
  EXPECT_EQ(media_frame, frame_with_new_handle->frame());
}

TEST_F(VideoFrameTest, ClonedFrame) {
  V8TestingScope scope;

  scoped_refptr<media::VideoFrame> media_frame =
      CreateDefaultBlackMediaVideoFrame();
  VideoFrame* blink_frame =
      CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());

  VideoFrame* cloned_frame =
      blink_frame->clone(scope.GetScriptState(), scope.GetExceptionState());

  // The cloned frame should be referencing the same media::VideoFrame.
  EXPECT_EQ(blink_frame->frame(), cloned_frame->frame());
  EXPECT_EQ(media_frame, cloned_frame->frame());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  blink_frame->close();

  // Closing the original frame should not affect the cloned frame.
  EXPECT_EQ(media_frame, cloned_frame->frame());
}

TEST_F(VideoFrameTest, CloningClosedFrame) {
  V8TestingScope scope;

  scoped_refptr<media::VideoFrame> media_frame =
      CreateDefaultBlackMediaVideoFrame();
  VideoFrame* blink_frame =
      CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());

  blink_frame->close();

  VideoFrame* cloned_frame =
      blink_frame->clone(scope.GetScriptState(), scope.GetExceptionState());

  // No frame should have been created, and there should be an exception.
  EXPECT_EQ(nullptr, cloned_frame);
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST_F(VideoFrameTest, LeakedHandlesReportLeaks) {
  V8TestingScope scope;

  // Create a handle directly instead of a video frame, to avoid dealing with
  // the GarbageCollector.
  scoped_refptr<media::VideoFrame> media_frame =
      CreateDefaultBlackMediaVideoFrame();
  auto handle = base::MakeRefCounted<VideoFrameHandle>(
      media_frame, scope.GetExecutionContext());

  // Remove the last reference to the handle without calling Invalidate().
  handle.reset();

  auto& logger = VideoFrameLogger::From(*scope.GetExecutionContext());

  EXPECT_TRUE(logger.GetCloseAuditor()->were_frames_not_closed());
}

TEST_F(VideoFrameTest, InvalidatedHandlesDontReportLeaks) {
  V8TestingScope scope;

  // Create a handle directly instead of a video frame, to avoid dealing with
  // the GarbageCollector.
  scoped_refptr<media::VideoFrame> media_frame =
      CreateDefaultBlackMediaVideoFrame();
  auto handle = base::MakeRefCounted<VideoFrameHandle>(
      media_frame, scope.GetExecutionContext());

  handle->Invalidate();
  handle.reset();

  auto& logger = VideoFrameLogger::From(*scope.GetExecutionContext());

  EXPECT_FALSE(logger.GetCloseAuditor()->were_frames_not_closed());
}

TEST_F(VideoFrameTest, ImageBitmapCreationAndZeroCopyRoundTrip) {
  V8TestingScope scope;

  auto* init = VideoFrameInit::Create();
  init->setTimestamp(0);

  sk_sp<SkSurface> surface(SkSurface::MakeRaster(
      SkImageInfo::MakeN32Premul(5, 5, SkColorSpace::MakeSRGB())));
  sk_sp<SkImage> original_image = surface->makeImageSnapshot();

  const auto* default_options = ImageBitmapOptions::Create();
  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(original_image), base::nullopt,
      default_options);
  CanvasImageSourceUnion source;
  source.SetImageBitmap(image_bitmap);
  auto* video_frame = VideoFrame::Create(scope.GetScriptState(), source, init,
                                         scope.GetExceptionState());

  EXPECT_EQ(video_frame->handle()->sk_image(), original_image);

  {
    auto promise = video_frame->createImageBitmap(
        scope.GetScriptState(), default_options, scope.GetExceptionState());
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* new_bitmap = ToImageBitmap(&scope, tester.Value());
    ASSERT_TRUE(new_bitmap);

    auto bitmap_image =
        new_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSwSkImage();
    EXPECT_EQ(bitmap_image, original_image);
  }

  auto* clone =
      video_frame->clone(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_EQ(clone->handle()->sk_image(), original_image);
}

TEST_F(VideoFrameTest, VideoFrameFromGPUImageBitmap) {
  V8TestingScope scope;

  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  CanvasResourceParams resource_params;
  auto resource_provider = CanvasResourceProvider::CreateSharedImageProvider(
      IntSize(100, 100), kLow_SkFilterQuality, resource_params,
      CanvasResourceProvider::ShouldInitialize::kNo, context_provider_wrapper,
      RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);

  scoped_refptr<StaticBitmapImage> bitmap = resource_provider->Snapshot();
  ASSERT_TRUE(bitmap->IsTextureBacked());

  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(bitmap);
  EXPECT_TRUE(image_bitmap);
  EXPECT_TRUE(image_bitmap->BitmapImage()->IsTextureBacked());

  auto* init = VideoFrameInit::Create();
  init->setTimestamp(0);

  CanvasImageSourceUnion source;
  source.SetImageBitmap(image_bitmap);
  auto* video_frame = VideoFrame::Create(scope.GetScriptState(), source, init,
                                         scope.GetExceptionState());
  ASSERT_TRUE(video_frame);
}

}  // namespace

}  // namespace blink

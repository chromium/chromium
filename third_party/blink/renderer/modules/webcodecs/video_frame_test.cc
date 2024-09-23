// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include "components/viz/test/test_context_provider.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_copy_to_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_handle.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_monitor.h"
#include "third_party/blink/renderer/modules/webcodecs/webcodecs_logger.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
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
    InitializeSharedGpuContextGLES2(test_context_provider_.get());
  }

  void TearDown() override { SharedGpuContext::Reset(); }

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
    return CreateBlackMediaVideoFrame(base::Microseconds(1000),
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
  test::TaskEnvironment task_environment_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
};

TEST_F(VideoFrameTest, ConstructorAndAttributes) {
  V8TestingScope scope;

  scoped_refptr<media::VideoFrame> media_frame = CreateBlackMediaVideoFrame(
      base::Microseconds(1000), media::PIXEL_FORMAT_I420,
      gfx::Size(112, 208) /* coded_size */,
      gfx::Size(100, 200) /* visible_size */);
  VideoFrame* blink_frame =
      CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());

  EXPECT_EQ(1000u, blink_frame->timestamp());
  EXPECT_EQ(112u, blink_frame->codedWidth());
  EXPECT_EQ(208u, blink_frame->codedHeight());
  EXPECT_EQ(media_frame, blink_frame->frame());

  blink_frame->close();

  EXPECT_EQ(1000u, blink_frame->timestamp());
  EXPECT_EQ(0u, blink_frame->codedWidth());
  EXPECT_EQ(0u, blink_frame->codedHeight());
  EXPECT_EQ(nullptr, blink_frame->frame());
}

TEST_F(VideoFrameTest, CopyToRGB) {
  V8TestingScope scope;

  ScopedWebCodecsCopyToRGBForTest feature(true);
  scoped_refptr<media::VideoFrame> media_frame = CreateBlackMediaVideoFrame(
      base::Microseconds(1000), media::PIXEL_FORMAT_I420,
      /* coded_size= */ gfx::Size(64, 48),
      /* visible_size= */ gfx::Size(64, 48));
  VideoFrame* blink_frame =
      CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());
  VideoFrameCopyToOptions* options = VideoFrameCopyToOptions::Create();
  options->setFormat(V8VideoPixelFormat::Enum::kRGBA);

  uint32_t buffer_size =
      blink_frame->allocationSize(options, scope.GetExceptionState());
  auto* buffer = DOMArrayBuffer::Create(buffer_size, 1);
  uint8_t* data = static_cast<uint8_t*>(buffer->Data());

  // Set buffer to white pixels.
  memset(data, 0xff, buffer_size);
  AllowSharedBufferSource* destination =
      MakeGarbageCollected<AllowSharedBufferSource>(buffer);

  auto promise = blink_frame->copyTo(scope.GetScriptState(), destination,
                                     options, scope.GetExceptionState());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  // Check that after copyTo() all the pixels are black.
  for (int y = 0; y < media_frame->coded_size().height(); y++) {
    for (int x = 0; x < media_frame->coded_size().width(); x++) {
      uint8_t* addr = &data[y * media_frame->stride(0) + x * 4];
      ASSERT_EQ(addr[0], 0) << " R x: " << x << " y: " << y;
      ASSERT_EQ(addr[1], 0) << " G x: " << x << " y: " << y;
      ASSERT_EQ(addr[2], 0) << " B x: " << x << " y: " << y;
    }
  }

  blink_frame->close();
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

  VideoFrame* cloned_frame = blink_frame->clone(scope.GetExceptionState());

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

  VideoFrame* cloned_frame = blink_frame->clone(scope.GetExceptionState());

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

  auto& logger = WebCodecsLogger::From(*scope.GetExecutionContext());

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

  auto& logger = WebCodecsLogger::From(*scope.GetExecutionContext());

  EXPECT_FALSE(logger.GetCloseAuditor()->were_frames_not_closed());
}

TEST_F(VideoFrameTest, ImageBitmapCreationAndZeroCopyRoundTrip) {
  V8TestingScope scope;

  auto* init = VideoFrameInit::Create();
  init->setTimestamp(0);

  sk_sp<SkSurface> surface(SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(5, 5, SkColorSpace::MakeSRGB())));
  sk_sp<SkImage> original_image = surface->makeImageSnapshot();

  const auto* default_options = ImageBitmapOptions::Create();
  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(original_image), std::nullopt,
      default_options);
  auto* source = MakeGarbageCollected<V8CanvasImageSource>(image_bitmap);
  auto* video_frame = VideoFrame::Create(scope.GetScriptState(), source, init,
                                         scope.GetExceptionState());

  EXPECT_EQ(video_frame->handle()->sk_image(), original_image);

  {
    auto* ibs_source = MakeGarbageCollected<V8ImageBitmapSource>(video_frame);
    auto promise = ImageBitmapFactories::CreateImageBitmap(
        scope.GetScriptState(), ibs_source, default_options,
        scope.GetExceptionState());
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* new_bitmap = ToImageBitmap(&scope, tester.Value());
    ASSERT_TRUE(new_bitmap);

    auto bitmap_image =
        new_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSwSkImage();
    EXPECT_EQ(bitmap_image, original_image);
  }

  auto* clone = video_frame->clone(scope.GetExceptionState());
  EXPECT_EQ(clone->handle()->sk_image(), original_image);
}

// Wraps |source| in a VideoFrame and checks for SkImage re-use where feasible.
void TestWrappedVideoFrameImageReuse(V8TestingScope& scope,
                                     const sk_sp<SkImage> orig_image,
                                     const V8CanvasImageSource* source) {
  // Wrapping image in a VideoFrame without changing any metadata should reuse
  // the original image.
  auto* init = VideoFrameInit::Create();
  init->setTimestamp(0);  // Timestamp is required since ImageBitmap lacks.
  auto* video_frame = VideoFrame::Create(scope.GetScriptState(), source, init,
                                         scope.GetExceptionState());
  EXPECT_EQ(video_frame->handle()->sk_image(), orig_image);

  // Duration metadata doesn't impact drawing so VideoFrame should still reuse
  // the original image.
  init->setDuration(1000);
  video_frame = VideoFrame::Create(scope.GetScriptState(), source, init,
                                   scope.GetExceptionState());
  EXPECT_EQ(video_frame->handle()->sk_image(), orig_image);

  // VisibleRect change does impact drawing, so VideoFrame should NOT re-use the
  // original image.
  DOMRectInit* visible_rect = DOMRectInit::Create();
  visible_rect->setX(1);
  visible_rect->setY(1);
  visible_rect->setWidth(2);
  visible_rect->setHeight(2);
  init->setVisibleRect(visible_rect);
  video_frame = VideoFrame::Create(scope.GetScriptState(), source, init,
                                   scope.GetExceptionState());
  EXPECT_NE(video_frame->handle()->sk_image(), orig_image);
}

// Wraps an ImageBitmap in a VideoFrame and checks for SkImage re-use where
// feasible.
TEST_F(VideoFrameTest, ImageReuse_VideoFrameFromImage) {
  V8TestingScope scope;

  sk_sp<SkSurface> surface(SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(5, 5, SkColorSpace::MakeSRGB())));
  sk_sp<SkImage> original_image = surface->makeImageSnapshot();

  const auto* default_options = ImageBitmapOptions::Create();
  auto* image_bitmap_layer = MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(original_image), std::nullopt,
      default_options);

  TestWrappedVideoFrameImageReuse(
      scope, original_image,
      MakeGarbageCollected<V8CanvasImageSource>(image_bitmap_layer));
}

// Like ImageReuse_VideoFrameFromImage, but adds an intermediate VideoFrame
// to the sandwich (which triggers distinct code paths).
TEST_F(VideoFrameTest, ImageReuse_VideoFrameFromVideoFrameFromImage) {
  V8TestingScope scope;

  sk_sp<SkSurface> surface(SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(5, 5, SkColorSpace::MakeSRGB())));
  sk_sp<SkImage> original_image = surface->makeImageSnapshot();

  const auto* default_options = ImageBitmapOptions::Create();
  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(original_image), std::nullopt,
      default_options);

  auto* init = VideoFrameInit::Create();
  init->setTimestamp(0);  // Timestamp is required since ImageBitmap lacks.
  auto* video_frame = VideoFrame::Create(
      scope.GetScriptState(),
      MakeGarbageCollected<V8CanvasImageSource>(image_bitmap), init,
      scope.GetExceptionState());

  TestWrappedVideoFrameImageReuse(
      scope, original_image,
      MakeGarbageCollected<V8CanvasImageSource>(video_frame));
}

TEST_F(VideoFrameTest, VideoFrameFromGPUImageBitmap) {
  V8TestingScope scope;

  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  auto resource_provider = CanvasResourceProvider::CreateSharedImageProvider(
      SkImageInfo::MakeN32Premul(100, 100), cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kNo, context_provider_wrapper,
      RasterMode::kGPU, gpu::SharedImageUsageSet());

  scoped_refptr<StaticBitmapImage> bitmap =
      resource_provider->Snapshot(FlushReason::kTesting);
  ASSERT_TRUE(bitmap->IsTextureBacked());

  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(bitmap);
  EXPECT_TRUE(image_bitmap);
  EXPECT_TRUE(image_bitmap->BitmapImage()->IsTextureBacked());

  auto* init = VideoFrameInit::Create();
  init->setTimestamp(0);

  auto* source = MakeGarbageCollected<V8CanvasImageSource>(image_bitmap);
  auto* video_frame = VideoFrame::Create(scope.GetScriptState(), source, init,
                                         scope.GetExceptionState());
  ASSERT_TRUE(video_frame);
}

TEST_F(VideoFrameTest, HandleMonitoring) {
  V8TestingScope scope;
  VideoFrameMonitor& monitor = VideoFrameMonitor::Instance();
  const std::string source1 = "source1";
  const std::string source2 = "source2";
  EXPECT_TRUE(monitor.IsEmpty());

  // Test all constructors.
  scoped_refptr<media::VideoFrame> media_frame1 =
      CreateDefaultBlackMediaVideoFrame();
  scoped_refptr<media::VideoFrame> media_frame2 =
      CreateDefaultBlackMediaVideoFrame();

  auto verify_expectations =
      [&](wtf_size_t num_frames_source1, int num_refs_frame1_source1,
          int num_refs_frame2_source1, wtf_size_t num_frames_source2,
          int num_refs_frame1_source2, int num_refs_frame2_source2) {
        EXPECT_EQ(monitor.NumFrames(source1), num_frames_source1);
        EXPECT_EQ(monitor.NumRefs(source1, media_frame1->unique_id()),
                  num_refs_frame1_source1);
        EXPECT_EQ(monitor.NumRefs(source1, media_frame2->unique_id()),
                  num_refs_frame2_source1);
        EXPECT_EQ(monitor.NumFrames(source2), num_frames_source2);
        EXPECT_EQ(monitor.NumRefs(source2, media_frame1->unique_id()),
                  num_refs_frame1_source2);
        EXPECT_EQ(monitor.NumRefs(source2, media_frame2->unique_id()),
                  num_refs_frame2_source2);
      };

  auto handle_1_1 = base::MakeRefCounted<VideoFrameHandle>(
      media_frame1, scope.GetExecutionContext(), source1);
  verify_expectations(/* source1 */ 1, 1, 0, /* source2 */ 0, 0, 0);

  sk_sp<SkSurface> surface(SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(5, 5, SkColorSpace::MakeSRGB())));
  sk_sp<SkImage> sk_image = surface->makeImageSnapshot();
  auto handle_2_1 = base::MakeRefCounted<VideoFrameHandle>(
      media_frame2, sk_image, scope.GetExecutionContext(), source1);
  verify_expectations(/* source1 */ 2, 1, 1, /* source2 */ 0, 0, 0);

  auto& logger = WebCodecsLogger::From(*scope.GetExecutionContext());
  auto handle_1_1b = base::MakeRefCounted<VideoFrameHandle>(
      media_frame1, sk_image, logger.GetCloseAuditor(), source1);
  verify_expectations(/* source1 */ 2, 2, 1, /* source2 */ 0, 0, 0);

  auto handle_1_2 =
      base::MakeRefCounted<VideoFrameHandle>(media_frame1, sk_image, source2);
  verify_expectations(/* source1 */ 2, 2, 1, /* source2 */ 1, 1, 0);

  auto non_monitored1 = base::MakeRefCounted<VideoFrameHandle>(
      media_frame2, sk_image, scope.GetExecutionContext());
  verify_expectations(/* source1 */ 2, 2, 1, /* source2 */ 1, 1, 0);

  auto non_monitored2 =
      base::MakeRefCounted<VideoFrameHandle>(media_frame1, sk_image);
  verify_expectations(/* source1 */ 2, 2, 1, /* source2 */ 1, 1, 0);

  // Move constructor
  auto handle_1_1c = std::move(handle_1_1b);
  verify_expectations(/* source1 */ 2, 2, 1, /* source2 */ 1, 1, 0);

  // Test all clone methods.
  auto clone_1_1a = handle_1_1->Clone();
  verify_expectations(/* source1 */ 2, 3, 1, /* source2 */ 1, 1, 0);

  auto clone_1_1b = handle_1_1->CloneForInternalUse();
  verify_expectations(/* source1 */ 2, 4, 1, /* source2 */ 1, 1, 0);

  // Clone non-monitored frame
  auto non_monitored_clone = non_monitored2->CloneForInternalUse();
  verify_expectations(/* source1 */ 2, 4, 1, /* source2 */ 1, 1, 0);

  // Test invalidate
  handle_1_1->Invalidate();
  verify_expectations(/* source1 */ 2, 3, 1, /* source2 */ 1, 1, 0);

  // handle_1_1b was moved to handle_1_1c
  handle_1_1c->Invalidate();
  verify_expectations(/* source1 */ 2, 2, 1, /* source2 */ 1, 1, 0);

  handle_2_1->Invalidate();
  verify_expectations(/* source1 */ 1, 2, 0, /* source2 */ 1, 1, 0);

  non_monitored1->Invalidate();
  verify_expectations(/* source1 */ 1, 2, 0, /* source2 */ 1, 1, 0);

  non_monitored2->Invalidate();
  verify_expectations(/* source1 */ 1, 2, 0, /* source2 */ 1, 1, 0);

  clone_1_1a->Invalidate();
  verify_expectations(/* source1 */ 1, 1, 0, /* source2 */ 1, 1, 0);

  // Resetting handles instead of invalidating.
  handle_1_2.reset();
  verify_expectations(/* source1 */ 1, 1, 0, /* source2 */ 0, 0, 0);

  clone_1_1b.reset();
  EXPECT_TRUE(monitor.IsEmpty());

  // handle10 is not monitored
  non_monitored_clone.reset();
  EXPECT_TRUE(monitor.IsEmpty());
}

TEST_F(VideoFrameTest, VideoFrameMonitoring) {
  V8TestingScope scope;
  VideoFrameMonitor& monitor = VideoFrameMonitor::Instance();
  const std::string source = "source";
  scoped_refptr<media::VideoFrame> media_frame =
      CreateDefaultBlackMediaVideoFrame();
  auto verify_expectations = [&](wtf_size_t num_frames, int num_refs) {
    EXPECT_EQ(monitor.NumFrames(source), num_frames);
    EXPECT_EQ(monitor.NumRefs(source, media_frame->unique_id()), num_refs);
  };
  EXPECT_TRUE(monitor.IsEmpty());

  // Test all constructors
  auto* frame1 = MakeGarbageCollected<VideoFrame>(
      media_frame, scope.GetExecutionContext(), source);
  verify_expectations(1u, 1);

  auto* non_monitored1 = MakeGarbageCollected<VideoFrame>(
      media_frame, scope.GetExecutionContext());
  verify_expectations(1u, 1);

  auto monitored_handle = base::MakeRefCounted<VideoFrameHandle>(
      media_frame, scope.GetExecutionContext(), source);
  auto* frame2 = MakeGarbageCollected<VideoFrame>(std::move(monitored_handle));
  verify_expectations(1u, 2);

  auto non_monitored_handle = base::MakeRefCounted<VideoFrameHandle>(
      media_frame, scope.GetExecutionContext());
  auto* non_monitored2 =
      MakeGarbageCollected<VideoFrame>(std::move(non_monitored_handle));
  verify_expectations(1u, 2);

  frame1->clone(scope.GetExceptionState());
  verify_expectations(1u, 3);

  auto* non_monitored_clone = non_monitored1->clone(scope.GetExceptionState());
  verify_expectations(1u, 3);

  frame1->close();
  verify_expectations(1u, 2);

  frame2->close();
  verify_expectations(1u, 1);

  non_monitored1->close();
  non_monitored2->close();
  non_monitored_clone->close();
  verify_expectations(1u, 1);

  // Garbage-collecting a non-closed monitored frame should reclaim it and
  // update the monitor.
  blink::WebHeap::CollectAllGarbageForTesting();
  EXPECT_TRUE(monitor.IsEmpty());
}

TEST_F(VideoFrameTest, TestExternalAllocatedMemoryIsReportedCorrectlyOnClose) {
  V8TestingScope scope;

  scoped_refptr<media::VideoFrame> media_frame = CreateBlackMediaVideoFrame(
      base::Microseconds(1000), media::PIXEL_FORMAT_I420,
      gfx::Size(112, 208) /* coded_size */,
      gfx::Size(100, 200) /* visible_size */);

  int64_t initial_external_memory =
      scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0);

  VideoFrame* blink_frame =
      CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());

  EXPECT_GT(scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0),
            initial_external_memory);

  // Calling close should decrement externally allocated memory.
  blink_frame->close();

  EXPECT_EQ(scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0),
            initial_external_memory);

  // Calling close another time should not decrement external memory twice.
  blink_frame->close();

  EXPECT_EQ(scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0),
            initial_external_memory);

  blink_frame = nullptr;
  blink::WebHeap::CollectAllGarbageForTesting();

  // Check the destructor does not double decrement the external memory.
  EXPECT_EQ(scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0),
            initial_external_memory);
}

TEST_F(VideoFrameTest,
       TestExternalAllocatedMemoryIsReportedCorrectlyOnDestruction) {
  V8TestingScope scope;

  scoped_refptr<media::VideoFrame> media_frame = CreateBlackMediaVideoFrame(
      base::Microseconds(1000), media::PIXEL_FORMAT_I420,
      gfx::Size(112, 208) /* coded_size */,
      gfx::Size(100, 200) /* visible_size */);

  int64_t initial_external_memory =
      scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0);

  CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());

  EXPECT_GT(scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0),
            initial_external_memory);

  blink::WebHeap::CollectAllGarbageForTesting();

  // Check the destructor correctly decrements the reported
  // externally allocated memory  when close has not been called before.
  EXPECT_EQ(scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0),
            initial_external_memory);
}

}  // namespace

}  // namespace blink

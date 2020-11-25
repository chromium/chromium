// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_handle.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

class VideoFrameTest : public testing::Test {
 public:
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

  blink_frame->destroy();

  EXPECT_FALSE(blink_frame->timestamp().has_value());
  EXPECT_EQ(0u, blink_frame->codedWidth());
  EXPECT_EQ(0u, blink_frame->codedHeight());
  EXPECT_EQ(0u, blink_frame->cropWidth());
  EXPECT_EQ(0u, blink_frame->cropHeight());
  EXPECT_EQ(nullptr, blink_frame->frame());
}

TEST_F(VideoFrameTest, FramesSharingHandleDestruction) {
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

  // Destroying a frame should invalidate all frames sharing the same handle.
  blink_frame->destroy();
  EXPECT_EQ(nullptr, frame_with_shared_handle->frame());
}

TEST_F(VideoFrameTest, FramesNotSharingHandleDestruction) {
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
  // one frame's destruction should not affect the other.
  blink_frame->destroy();
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

  blink_frame->destroy();

  // Destroying the original frame should not affect the cloned frame.
  EXPECT_EQ(media_frame, cloned_frame->frame());
}

TEST_F(VideoFrameTest, CloningDestroyedFrame) {
  V8TestingScope scope;

  scoped_refptr<media::VideoFrame> media_frame =
      CreateDefaultBlackMediaVideoFrame();
  VideoFrame* blink_frame =
      CreateBlinkVideoFrame(media_frame, scope.GetExecutionContext());

  blink_frame->destroy();

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

  EXPECT_TRUE(logger.GetDestructionAuditor()->were_frames_not_destroyed());
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

  EXPECT_FALSE(logger.GetDestructionAuditor()->were_frames_not_destroyed());
}

}  // namespace

}  // namespace blink

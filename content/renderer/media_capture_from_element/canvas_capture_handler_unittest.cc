// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_capture_from_element/canvas_capture_handler.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/renderer/media/stream/media_stream_video_capturer_source.h"
#include "media/base/limits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::TestWithParam;

namespace content {

namespace {

static const int kTestCanvasCaptureWidth = 320;
static const int kTestCanvasCaptureHeight = 240;
static const double kTestCanvasCaptureFramesPerSecond = 55.5;

static const int kTestCanvasCaptureFrameEvenSize = 2;
static const int kTestCanvasCaptureFrameOddSize = 3;
static const int kTestCanvasCaptureFrameColorErrorTolerance = 2;
static const int kTestAlphaValue = 175;

ACTION_P(RunClosure, closure) {
  closure.Run();
}

}  // namespace

class CanvasCaptureHandlerTest
    : public TestWithParam<testing::tuple<bool, int, int>> {
 public:
  CanvasCaptureHandlerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    canvas_capture_handler_ = CanvasCaptureHandler::CreateCanvasCaptureHandler(
        blink::WebSize(kTestCanvasCaptureWidth, kTestCanvasCaptureHeight),
        kTestCanvasCaptureFramesPerSecond,
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(), &track_);
  }

  void TearDown() override {
    track_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();
    canvas_capture_handler_.reset();

    // Let the message loop run to finish destroying the capturer.
    base::RunLoop().RunUntilIdle();
  }

  // Necessary callbacks and MOCK_METHODS for VideoCapturerSource.
  MOCK_METHOD2(DoOnDeliverFrame,
               void(const scoped_refptr<media::VideoFrame>&, base::TimeTicks));
  void OnDeliverFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                      base::TimeTicks estimated_capture_time) {
    DoOnDeliverFrame(video_frame, estimated_capture_time);
  }

  MOCK_METHOD1(DoOnRunning, void(bool));
  void OnRunning(bool state) { DoOnRunning(state); }

  // Verify returned frames.
  static sk_sp<SkImage> GenerateTestImage(bool opaque, int width, int height) {
    SkBitmap testBitmap;
    testBitmap.allocN32Pixels(width, height, opaque);
    testBitmap.eraseARGB(opaque ? 255 : kTestAlphaValue, 30, 60, 200);
    return SkImage::MakeFromBitmap(testBitmap);
  }

  void OnVerifyDeliveredFrame(
      bool opaque,
      int expected_width,
      int expected_height,
      const scoped_refptr<media::VideoFrame>& video_frame,
      base::TimeTicks estimated_capture_time) {
    if (opaque)
      EXPECT_EQ(media::PIXEL_FORMAT_I420, video_frame->format());
    else
      EXPECT_EQ(media::PIXEL_FORMAT_I420A, video_frame->format());

    const gfx::Size& size = video_frame->visible_rect().size();
    EXPECT_EQ(expected_width, size.width());
    EXPECT_EQ(expected_height, size.height());
    const uint8_t* y_plane =
        video_frame->visible_data(media::VideoFrame::kYPlane);
    EXPECT_NEAR(74, y_plane[0], kTestCanvasCaptureFrameColorErrorTolerance);
    const uint8_t* u_plane =
        video_frame->visible_data(media::VideoFrame::kUPlane);
    EXPECT_NEAR(193, u_plane[0], kTestCanvasCaptureFrameColorErrorTolerance);
    const uint8_t* v_plane =
        video_frame->visible_data(media::VideoFrame::kVPlane);
    EXPECT_NEAR(105, v_plane[0], kTestCanvasCaptureFrameColorErrorTolerance);
    if (!opaque) {
      const uint8_t* a_plane =
          video_frame->visible_data(media::VideoFrame::kAPlane);
      EXPECT_EQ(kTestAlphaValue, a_plane[0]);
    }
  }

  blink::WebMediaStreamTrack track_;
  // The Class under test. Needs to be scoped_ptr to force its destruction.
  std::unique_ptr<CanvasCaptureHandler> canvas_capture_handler_;

 protected:
  media::VideoCapturerSource* GetVideoCapturerSource(
      MediaStreamVideoCapturerSource* ms_source) {
    return ms_source->source_.get();
  }

  // A ChildProcess is needed to fool the Tracks and Sources believing they are
  // on the right threads. A ScopedTaskEnvironment must be instantiated before
  // ChildProcess to prevent it from leaking a TaskScheduler.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ChildProcess child_process_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandlerTest);
};

// Checks that the initialization-destruction sequence works fine.
TEST_F(CanvasCaptureHandlerTest, ConstructAndDestruct) {
  EXPECT_TRUE(canvas_capture_handler_->NeedsNewFrame());
  base::RunLoop().RunUntilIdle();
}

// Checks that the destruction sequence works fine.
TEST_F(CanvasCaptureHandlerTest, DestructTrack) {
  EXPECT_TRUE(canvas_capture_handler_->NeedsNewFrame());
  track_.Reset();
  base::RunLoop().RunUntilIdle();
}

// Checks that the destruction sequence works fine.
TEST_F(CanvasCaptureHandlerTest, DestructHandler) {
  EXPECT_TRUE(canvas_capture_handler_->NeedsNewFrame());
  canvas_capture_handler_.reset();
  base::RunLoop().RunUntilIdle();
}

// Checks that VideoCapturerSource call sequence works fine.
TEST_P(CanvasCaptureHandlerTest, GetFormatsStartAndStop) {
  InSequence s;
  const blink::WebMediaStreamSource& web_media_stream_source = track_.Source();
  EXPECT_FALSE(web_media_stream_source.IsNull());
  MediaStreamVideoCapturerSource* const ms_source =
      static_cast<MediaStreamVideoCapturerSource*>(
          web_media_stream_source.GetExtraData());
  EXPECT_TRUE(ms_source != nullptr);
  media::VideoCapturerSource* source = GetVideoCapturerSource(ms_source);
  EXPECT_TRUE(source != nullptr);

  media::VideoCaptureFormats formats = source->GetPreferredFormats();
  ASSERT_EQ(2u, formats.size());
  EXPECT_EQ(kTestCanvasCaptureWidth, formats[0].frame_size.width());
  EXPECT_EQ(kTestCanvasCaptureHeight, formats[0].frame_size.height());
  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
      .Times(1)
      .WillOnce(RunClosure(std::move(quit_closure)));
  source->StartCapture(
      params, base::Bind(&CanvasCaptureHandlerTest::OnDeliverFrame,
                         base::Unretained(this)),
      base::Bind(&CanvasCaptureHandlerTest::OnRunning, base::Unretained(this)));
  canvas_capture_handler_->SendNewFrame(
      GenerateTestImage(testing::get<0>(GetParam()),
                        testing::get<1>(GetParam()),
                        testing::get<2>(GetParam())),
      nullptr);
  run_loop.Run();

  source->StopCapture();
}

// Verifies that SkImage is processed and produces VideoFrame as expected.
TEST_P(CanvasCaptureHandlerTest, VerifyFrame) {
  const bool opaque_frame = testing::get<0>(GetParam());
  const bool width = testing::get<1>(GetParam());
  const bool height = testing::get<1>(GetParam());
  InSequence s;
  media::VideoCapturerSource* const source =
      GetVideoCapturerSource(static_cast<MediaStreamVideoCapturerSource*>(
          track_.Source().GetExtraData()));
  EXPECT_TRUE(source != nullptr);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);
  media::VideoCaptureParams params;
  source->StartCapture(
      params,
      base::Bind(&CanvasCaptureHandlerTest::OnVerifyDeliveredFrame,
                 base::Unretained(this), opaque_frame, width, height),
      base::Bind(&CanvasCaptureHandlerTest::OnRunning, base::Unretained(this)));
  canvas_capture_handler_->SendNewFrame(
      GenerateTestImage(opaque_frame, width, height), nullptr);
  run_loop.RunUntilIdle();
}

// Checks that needsNewFrame() works as expected.
TEST_F(CanvasCaptureHandlerTest, CheckNeedsNewFrame) {
  InSequence s;
  media::VideoCapturerSource* source =
      GetVideoCapturerSource(static_cast<MediaStreamVideoCapturerSource*>(
          track_.Source().GetExtraData()));
  EXPECT_TRUE(source != nullptr);
  EXPECT_TRUE(canvas_capture_handler_->NeedsNewFrame());
  source->StopCapture();
  EXPECT_FALSE(canvas_capture_handler_->NeedsNewFrame());
}

INSTANTIATE_TEST_CASE_P(
    ,
    CanvasCaptureHandlerTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(kTestCanvasCaptureFrameEvenSize,
                                         kTestCanvasCaptureFrameOddSize),
                       ::testing::Values(kTestCanvasCaptureFrameEvenSize,
                                         kTestCanvasCaptureFrameOddSize)));

}  // namespace content

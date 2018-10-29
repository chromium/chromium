// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_capture_from_element/html_video_element_capturer_source.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/limits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_string.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

// An almost empty WebMediaPlayer to override paint() method.
class MockWebMediaPlayer : public blink::WebMediaPlayer,
                           public base::SupportsWeakPtr<MockWebMediaPlayer> {
 public:
  MockWebMediaPlayer()  = default;
  ~MockWebMediaPlayer() override = default;

  LoadTiming Load(LoadType,
                  const blink::WebMediaPlayerSource&,
                  CORSMode) override {
    return LoadTiming::kImmediate;
  }
  void Play() override {}
  void Pause() override {}
  void Seek(double seconds) override {}
  void SetRate(double) override {}
  void SetVolume(double) override {}
  void EnterPictureInPicture(PipWindowOpenedCallback) override {}
  void ExitPictureInPicture(PipWindowClosedCallback) override {}
  void SetPictureInPictureCustomControls(
      const std::vector<blink::PictureInPictureControlInfo>&) override {}
  void RegisterPictureInPictureWindowResizeCallback(
      PipWindowResizedCallback) override {}
  blink::WebTimeRanges Buffered() const override {
    return blink::WebTimeRanges();
  }
  blink::WebTimeRanges Seekable() const override {
    return blink::WebTimeRanges();
  }
  void SetSinkId(const blink::WebString& sinkId,
                 std::unique_ptr<blink::WebSetSinkIdCallbacks>) override {}
  bool HasVideo() const override { return true; }
  bool HasAudio() const override { return false; }
  blink::WebSize NaturalSize() const override { return blink::WebSize(16, 10); }
  blink::WebSize VisibleRect() const override { return blink::WebSize(16, 10); }
  bool Paused() const override { return false; }
  bool Seeking() const override { return false; }
  double Duration() const override { return 0.0; }
  double CurrentTime() const override { return 0.0; }
  NetworkState GetNetworkState() const override { return kNetworkStateEmpty; }
  ReadyState GetReadyState() const override { return kReadyStateHaveNothing; }
  SurfaceLayerMode GetVideoSurfaceLayerMode() const override {
    return SurfaceLayerMode::kNever;
  }
  blink::WebString GetErrorMessage() const override {
    return blink::WebString();
  }

  bool DidLoadingProgress() override { return true; }
  bool WouldTaintOrigin() const override { return false; }
  double MediaTimeForTimeValue(double timeValue) const override { return 0.0; }
  unsigned DecodedFrameCount() const override { return 0; }
  unsigned DroppedFrameCount() const override { return 0; }
  unsigned CorruptedFrameCount() const override { return 0; }
  uint64_t AudioDecodedByteCount() const override { return 0; }
  uint64_t VideoDecodedByteCount() const override { return 0; }

  void Paint(cc::PaintCanvas* canvas,
             const blink::WebRect& paint_rectangle,
             cc::PaintFlags&,
             int already_uploaded_id,
             VideoFrameUploadMetadata* out_metadata) override {
    // We could fill in |canvas| with a meaningful pattern in ARGB and verify
    // that is correctly captured (as I420) by HTMLVideoElementCapturerSource
    // but I don't think that'll be easy/useful/robust, so just let go here.
    return;
  }
};

class HTMLVideoElementCapturerSourceTest : public testing::Test {
 public:
  HTMLVideoElementCapturerSourceTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        web_media_player_(new MockWebMediaPlayer()),
        html_video_capturer_(new HtmlVideoElementCapturerSource(
            web_media_player_->AsWeakPtr(),
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            blink::scheduler::GetSingleThreadTaskRunnerForTesting())) {}

  // Necessary callbacks and MOCK_METHODS for them.
  MOCK_METHOD2(DoOnDeliverFrame,
               void(const scoped_refptr<media::VideoFrame>&, base::TimeTicks));
  void OnDeliverFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                    base::TimeTicks estimated_capture_time) {
    DoOnDeliverFrame(video_frame, estimated_capture_time);
  }

  MOCK_METHOD1(DoOnRunning, void(bool));
  void OnRunning(bool state) { DoOnRunning(state); }

 protected:
  // We need some kind of message loop to allow |html_video_capturer_| to
  // schedule capture events.
  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<MockWebMediaPlayer> web_media_player_;
  std::unique_ptr<HtmlVideoElementCapturerSource> html_video_capturer_;
};

// Constructs and destructs all objects, in particular |html_video_capturer_|
// and its inner object(s). This is a non trivial sequence.
TEST_F(HTMLVideoElementCapturerSourceTest, ConstructAndDestruct) {}

// Checks that the usual sequence of GetPreferredFormats() ->
// StartCapture() -> StopCapture() works as expected and let it capture two
// frames.
TEST_F(HTMLVideoElementCapturerSourceTest, GetFormatsAndStartAndStop) {
  InSequence s;
  media::VideoCaptureFormats formats =
      html_video_capturer_->GetPreferredFormats();
  ASSERT_EQ(1u, formats.size());
  EXPECT_EQ(web_media_player_->NaturalSize().width,
            formats[0].frame_size.width());
  EXPECT_EQ(web_media_player_->NaturalSize().height,
            formats[0].frame_size.height());

  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  scoped_refptr<media::VideoFrame> first_frame;
  scoped_refptr<media::VideoFrame> second_frame;
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _)).WillOnce(SaveArg<0>(&first_frame));
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&second_frame),
                      RunClosure(std::move(quit_closure))));

  html_video_capturer_->StartCapture(
      params, base::Bind(&HTMLVideoElementCapturerSourceTest::OnDeliverFrame,
                         base::Unretained(this)),
      base::Bind(&HTMLVideoElementCapturerSourceTest::OnRunning,
                 base::Unretained(this)));

  run_loop.Run();

  EXPECT_EQ(0u, first_frame->timestamp().InMilliseconds());
  EXPECT_GT(second_frame->timestamp().InMilliseconds(), 30u);
  html_video_capturer_->StopCapture();
  Mock::VerifyAndClearExpectations(this);
}

// When a new source is created and started, it is stopped in the same task
// when cross-origin data is detected. This test checks that no data is
// delivered in this case.
TEST_F(HTMLVideoElementCapturerSourceTest,
       StartAndStopInSameTaskCaptureZeroFrames) {
  InSequence s;
  media::VideoCaptureFormats formats =
      html_video_capturer_->GetPreferredFormats();
  ASSERT_EQ(1u, formats.size());
  EXPECT_EQ(web_media_player_->NaturalSize().width,
            formats[0].frame_size.width());
  EXPECT_EQ(web_media_player_->NaturalSize().height,
            formats[0].frame_size.height());

  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  EXPECT_CALL(*this, DoOnRunning(true));
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _)).Times(0);

  html_video_capturer_->StartCapture(
      params,
      base::Bind(&HTMLVideoElementCapturerSourceTest::OnDeliverFrame,
                 base::Unretained(this)),
      base::Bind(&HTMLVideoElementCapturerSourceTest::OnRunning,
                 base::Unretained(this)));
  html_video_capturer_->StopCapture();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);
}

}  // namespace content

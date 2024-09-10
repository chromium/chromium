// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/html_video_element_capturer_source.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using base::test::RunOnceClosure;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;

namespace blink {

namespace {

// An almost empty WebMediaPlayer to override paint() method.
class MockWebMediaPlayer : public WebMediaPlayer {
 public:
  MockWebMediaPlayer() = default;
  ~MockWebMediaPlayer() override = default;

  LoadTiming Load(LoadType,
                  const WebMediaPlayerSource&,
                  CorsMode,
                  bool is_cache_disabled) override {
    return LoadTiming::kImmediate;
  }
  void Play() override {}
  void Pause() override {}
  void Seek(double seconds) override {}
  void SetRate(double) override {}
  void SetVolume(double) override {}
  void SetLatencyHint(double) override {}
  void SetPreservesPitch(bool) override {}
  void SetWasPlayedWithUserActivationAndHighMediaEngagement(bool) override {}
  void SetShouldPauseWhenFrameIsHidden(bool) override {}
  void OnRequestPictureInPicture() override {}
  WebTimeRanges Buffered() const override { return WebTimeRanges(); }
  WebTimeRanges Seekable() const override { return WebTimeRanges(); }
  void OnFrozen() override {}
  bool SetSinkId(const WebString& sinkId,
                 WebSetSinkIdCompleteCallback) override {
    return false;
  }
  bool HasVideo() const override { return true; }
  bool HasAudio() const override { return false; }
  gfx::Size NaturalSize() const override { return size_; }
  gfx::Size VisibleSize() const override { return size_; }
  bool Paused() const override { return false; }
  bool Seeking() const override { return false; }
  double Duration() const override { return 0.0; }
  double CurrentTime() const override { return 0.0; }
  bool IsEnded() const override { return false; }
  NetworkState GetNetworkState() const override { return kNetworkStateEmpty; }
  ReadyState GetReadyState() const override { return kReadyStateHaveNothing; }
  WebString GetErrorMessage() const override { return WebString(); }

  bool DidLoadingProgress() override { return true; }
  bool WouldTaintOrigin() const override { return would_taint_origin_; }
  double MediaTimeForTimeValue(double timeValue) const override { return 0.0; }
  unsigned DecodedFrameCount() const override { return 0; }
  unsigned DroppedFrameCount() const override { return 0; }
  unsigned CorruptedFrameCount() const override { return 0; }
  uint64_t AudioDecodedByteCount() const override { return 0; }
  uint64_t VideoDecodedByteCount() const override { return 0; }
  void SetVolumeMultiplier(double multiplier) override {}
  void SuspendForFrameClosed() override {}

  void SetWouldTaintOrigin(bool taint) { would_taint_origin_ = taint; }
  bool PassedTimingAllowOriginCheck() const override { return true; }

  void Paint(cc::PaintCanvas* canvas,
             const gfx::Rect& rect,
             cc::PaintFlags&) override {
    return;
  }

  scoped_refptr<media::VideoFrame> GetCurrentFrameThenUpdate() override {
    // We could fill in |canvas| with a meaningful pattern in ARGB and verify
    // that is correctly captured (as I420) by HTMLVideoElementCapturerSource
    // but I don't think that'll be easy/useful/robust, so just let go here.
    return is_video_opaque_ ? media::VideoFrame::CreateBlackFrame(size_)
                            : media::VideoFrame::CreateTransparentFrame(size_);
  }

  std::optional<media::VideoFrame::ID> CurrentFrameId() const override {
    return std::nullopt;
  }

  bool IsOpaque() const override { return is_video_opaque_; }
  bool HasAvailableVideoFrame() const override { return true; }
  bool HasReadableVideoFrame() const override { return true; }

  base::WeakPtr<WebMediaPlayer> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  bool is_video_opaque_ = true;
  gfx::Size size_ = gfx::Size(16, 10);
  bool would_taint_origin_ = false;

  base::WeakPtrFactory<MockWebMediaPlayer> weak_factory_{this};
};

}  // namespace

class HTMLVideoElementCapturerSourceTest : public testing::TestWithParam<bool> {
 public:
  HTMLVideoElementCapturerSourceTest()
      : web_media_player_(new MockWebMediaPlayer()),
        html_video_capturer_(new HtmlVideoElementCapturerSource(
            web_media_player_->AsWeakPtr(),
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            scheduler::GetSingleThreadTaskRunnerForTesting())) {}

  // Necessary callbacks and MOCK_METHODS for them.
  MOCK_METHOD2(DoOnDeliverFrame,
               void(scoped_refptr<media::VideoFrame>, base::TimeTicks));
  void OnDeliverFrame(
      scoped_refptr<media::VideoFrame> video_frame,
      base::TimeTicks estimated_capture_time) {
    DoOnDeliverFrame(std::move(video_frame), estimated_capture_time);
  }

  MOCK_METHOD1(DoOnRunning, void(bool));
  void OnRunning(blink::RunState run_state) {
    bool state = (run_state == blink::RunState::kRunning) ? true : false;
    DoOnRunning(state);
  }

  void SetVideoPlayerOpacity(bool opacity) {
    web_media_player_->is_video_opaque_ = opacity;
  }

  void SetVideoPlayerSize(const gfx::Size& size) {
    web_media_player_->size_ = size;
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockWebMediaPlayer> web_media_player_;
  std::unique_ptr<HtmlVideoElementCapturerSource> html_video_capturer_;
};

// Constructs and destructs all objects, in particular |html_video_capturer_|
// and its inner object(s). This is a non trivial sequence.
TEST_F(HTMLVideoElementCapturerSourceTest, ConstructAndDestruct) {}

TEST_F(HTMLVideoElementCapturerSourceTest, EmptyWebMediaPlayerFailsCapture) {
  web_media_player_.reset();
  EXPECT_CALL(*this, DoOnRunning(false)).Times(1);

  html_video_capturer_->StartCapture(
      media::VideoCaptureParams(),
      WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnDeliverFrame,
                         base::Unretained(this)),
      base::DoNothing(), base::DoNothing(),
      WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnRunning,
                         base::Unretained(this)));
}

// Checks that the usual sequence of GetPreferredFormats() ->
// StartCapture() -> StopCapture() works as expected and let it capture two
// frames, that are tested for format vs the expected source opacity.
TEST_P(HTMLVideoElementCapturerSourceTest, GetFormatsAndStartAndStop) {
  InSequence s;
  media::VideoCaptureFormats formats =
      html_video_capturer_->GetPreferredFormats();
  ASSERT_EQ(1u, formats.size());
  EXPECT_EQ(web_media_player_->NaturalSize(), formats[0].frame_size);

  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);

  const bool is_video_opaque = GetParam();
  SetVideoPlayerOpacity(is_video_opaque);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  scoped_refptr<media::VideoFrame> first_frame;
  scoped_refptr<media::VideoFrame> second_frame;
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _)).WillOnce(SaveArg<0>(&first_frame));
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&second_frame),
                      RunOnceClosure(std::move(quit_closure))));

  html_video_capturer_->StartCapture(
      params,
      WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnDeliverFrame,
                         base::Unretained(this)),
      base::DoNothing(), base::DoNothing(),
      WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnRunning,
                         base::Unretained(this)));

  run_loop.Run();

  EXPECT_EQ(0u, first_frame->timestamp().InMilliseconds());
  EXPECT_GT(second_frame->timestamp().InMilliseconds(), 30u);
  if (is_video_opaque)
    EXPECT_EQ(media::PIXEL_FORMAT_I420, first_frame->format());
  else
    EXPECT_EQ(media::PIXEL_FORMAT_I420A, first_frame->format());

  html_video_capturer_->StopCapture();
  Mock::VerifyAndClearExpectations(this);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HTMLVideoElementCapturerSourceTest,
                         ::testing::Bool());

// When a new source is created and started, it is stopped in the same task
// when cross-origin data is detected. This test checks that no data is
// delivered in this case.
TEST_F(HTMLVideoElementCapturerSourceTest,
       StartAndStopInSameTaskCaptureZeroFrames) {
  InSequence s;
  media::VideoCaptureFormats formats =
      html_video_capturer_->GetPreferredFormats();
  ASSERT_EQ(1u, formats.size());
  EXPECT_EQ(web_media_player_->NaturalSize(), formats[0].frame_size);

  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  EXPECT_CALL(*this, DoOnRunning(true));
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _)).Times(0);

  html_video_capturer_->StartCapture(
      params,
      WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnDeliverFrame,
                         base::Unretained(this)),
      base::DoNothing(), base::DoNothing(),
      WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnRunning,
                         base::Unretained(this)));
  html_video_capturer_->StopCapture();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);
}

// Verify that changes in the opacicty of the source WebMediaPlayer are followed
// by corresponding changes in the format of the captured VideoFrame.
TEST_F(HTMLVideoElementCapturerSourceTest, AlphaAndNot) {
  InSequence s;
  media::VideoCaptureFormats formats =
      html_video_capturer_->GetPreferredFormats();
  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  {
    SetVideoPlayerOpacity(false);

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    scoped_refptr<media::VideoFrame> frame;
    EXPECT_CALL(*this, DoOnRunning(true)).Times(1);
    EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
        .WillOnce(
            DoAll(SaveArg<0>(&frame), RunOnceClosure(std::move(quit_closure))));
    html_video_capturer_->StartCapture(
        params,
        WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnDeliverFrame,
                           base::Unretained(this)),
        base::DoNothing(), base::DoNothing(),
        WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnRunning,
                           base::Unretained(this)));
    run_loop.Run();

    EXPECT_EQ(media::PIXEL_FORMAT_I420A, frame->format());
  }
  {
    SetVideoPlayerOpacity(true);

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    scoped_refptr<media::VideoFrame> frame;
    EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
        .WillOnce(
            DoAll(SaveArg<0>(&frame), RunOnceClosure(std::move(quit_closure))));
    run_loop.Run();

    EXPECT_EQ(media::PIXEL_FORMAT_I420, frame->format());
  }
  {
    SetVideoPlayerOpacity(false);

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    scoped_refptr<media::VideoFrame> frame;
    EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
        .WillOnce(
            DoAll(SaveArg<0>(&frame), RunOnceClosure(std::move(quit_closure))));
    run_loop.Run();

    EXPECT_EQ(media::PIXEL_FORMAT_I420A, frame->format());
  }

  html_video_capturer_->StopCapture();
  Mock::VerifyAndClearExpectations(this);
}

// Verify that changes in the natural size of the source WebMediaPlayer do not
// crash.
// TODO(crbug.com/1817203): Verify that size changes are fully supported.
TEST_F(HTMLVideoElementCapturerSourceTest, SizeChange) {
  InSequence s;
  media::VideoCaptureFormats formats =
      html_video_capturer_->GetPreferredFormats();
  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  {
    SetVideoPlayerSize(gfx::Size(16, 10));

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    scoped_refptr<media::VideoFrame> frame;
    EXPECT_CALL(*this, DoOnRunning(true)).Times(1);
    EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
        .WillOnce(
            DoAll(SaveArg<0>(&frame), RunOnceClosure(std::move(quit_closure))));
    html_video_capturer_->StartCapture(
        params,
        WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnDeliverFrame,
                           base::Unretained(this)),
        base::DoNothing(), base::DoNothing(),
        WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnRunning,
                           base::Unretained(this)));
    run_loop.Run();
  }
  {
    SetVideoPlayerSize(gfx::Size(32, 20));

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    scoped_refptr<media::VideoFrame> frame;
    EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
        .WillOnce(
            DoAll(SaveArg<0>(&frame), RunOnceClosure(std::move(quit_closure))));
    run_loop.Run();
  }

  html_video_capturer_->StopCapture();
  Mock::VerifyAndClearExpectations(this);
}

// Checks that the usual sequence of GetPreferredFormats() ->
// StartCapture() -> StopCapture() works as expected and let it capture two
// frames, that are tested for format vs the expected source opacity.
TEST_F(HTMLVideoElementCapturerSourceTest, TaintedPlayerDoesNotDeliverFrames) {
  InSequence s;
  media::VideoCaptureFormats formats =
      html_video_capturer_->GetPreferredFormats();
  ASSERT_EQ(1u, formats.size());
  EXPECT_EQ(web_media_player_->NaturalSize(), formats[0].frame_size);
  web_media_player_->SetWouldTaintOrigin(true);

  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);

  // No frames should be delivered.
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _)).Times(0);
  html_video_capturer_->StartCapture(
      params,
      WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnDeliverFrame,
                         base::Unretained(this)),
      base::DoNothing(), base::DoNothing(),
      WTF::BindRepeating(&HTMLVideoElementCapturerSourceTest::OnRunning,
                         base::Unretained(this)));

  // Wait for frames to be potentially sent in a follow-up task.
  base::RunLoop().RunUntilIdle();

  html_video_capturer_->StopCapture();
  Mock::VerifyAndClearExpectations(this);
}

}  // namespace blink

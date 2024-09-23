// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/data_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/null_video_sink.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/base/wall_clock_time_source.h"
#include "media/renderers/video_renderer_impl.h"
#include "media/video/mock_gpu_memory_buffer_video_frame_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::Values;

namespace media {

MATCHER_P(HasTimestampMatcher, ms, "") {
  *result_listener << "has timestamp " << arg->timestamp().InMilliseconds();
  return arg->timestamp().InMilliseconds() == ms;
}

class VideoRendererImplTest : public testing::Test {
 public:
  std::vector<std::unique_ptr<VideoDecoder>> CreateVideoDecodersForTest() {
    decoder_ = new NiceMock<MockVideoDecoder>();
    std::vector<std::unique_ptr<VideoDecoder>> decoders;
    decoders.push_back(base::WrapUnique(decoder_.get()));
    ON_CALL(*decoder_, Initialize_(_, _, _, _, _, _))
        .WillByDefault(
            DoAll(SaveArg<4>(&output_cb_),
                  RunOnceCallback<3>(expect_init_success_
                                         ? DecoderStatus::Codes::kOk
                                         : DecoderStatus::Codes::kFailed)));
    // Monitor decodes from the decoder.
    ON_CALL(*decoder_, Decode_(_, _))
        .WillByDefault(Invoke(this, &VideoRendererImplTest::DecodeRequested));
    ON_CALL(*decoder_, Reset_(_))
        .WillByDefault(Invoke(this, &VideoRendererImplTest::FlushRequested));
    ON_CALL(*decoder_, GetMaxDecodeRequests()).WillByDefault(Return(1));
    return decoders;
  }

  VideoRendererImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        decoder_(nullptr),
        demuxer_stream_(DemuxerStream::VIDEO),
        simulate_decode_delay_(false),
        expect_init_success_(true),
        time_source_(&tick_clock_) {
    null_video_sink_ = std::make_unique<NullVideoSink>(
        false, base::Seconds(1.0 / 60),
        base::BindRepeating(&MockCB::FrameReceived,
                            base::Unretained(&mock_cb_)),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    renderer_ = std::make_unique<VideoRendererImpl>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        null_video_sink_.get(),
        base::BindRepeating(&VideoRendererImplTest::CreateVideoDecodersForTest,
                            base::Unretained(this)),
        true, &media_log_, nullptr, 0);
    renderer_->SetTickClockForTesting(&tick_clock_);
    null_video_sink_->set_tick_clock_for_testing(&tick_clock_);

    // Start wallclock time at a non-zero value.
    AdvanceWallclockTimeInMs(12345);

    demuxer_stream_.set_video_decoder_config(TestVideoConfig::Normal());

    // We expect these to be called but we don't care how/when. Tests can
    // customize the provided buffer returned via OnDemuxerRead().
    ON_CALL(demuxer_stream_, OnRead(_))
        .WillByDefault(Invoke(this, &VideoRendererImplTest::OnDemuxerRead));
  }

  VideoRendererImplTest(const VideoRendererImplTest&) = delete;
  VideoRendererImplTest& operator=(const VideoRendererImplTest&) = delete;

  ~VideoRendererImplTest() override = default;

  void Initialize() {
    InitializeWithLowDelay(false);
  }

  void InitializeWithLowDelay(bool low_delay) {
    // Initialize, we shouldn't have any reads.
    InitializeRenderer(&demuxer_stream_, low_delay, true);
  }

  void InitializeRenderer(MockDemuxerStream* demuxer_stream,
                          bool low_delay,
                          bool expect_success) {
    SCOPED_TRACE(base::StringPrintf("InitializeRenderer(%d)", expect_success));
    expect_init_success_ = expect_success;
    WaitableMessageLoopEvent event;
    CallInitialize(demuxer_stream, event.GetPipelineStatusCB(), low_delay,
                   expect_success);
    event.RunAndWaitForStatus(expect_success ? PIPELINE_OK
                                             : DECODER_ERROR_NOT_SUPPORTED);
  }

  void CallInitialize(MockDemuxerStream* demuxer_stream,
                      PipelineStatusCallback status_cb,
                      bool low_delay,
                      bool expect_success) {
    if (low_delay)
      demuxer_stream->set_liveness(StreamLiveness::kLive);
    EXPECT_CALL(mock_cb_, OnWaiting(_)).Times(0);
    EXPECT_CALL(mock_cb_, OnAudioConfigChange(_)).Times(0);
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
    renderer_->Initialize(
        demuxer_stream, nullptr, &mock_cb_,
        base::BindRepeating(&WallClockTimeSource::GetWallClockTimes,
                            base::Unretained(&time_source_)),
        std::move(status_cb));
  }

  void StartPlayingFrom(int milliseconds) {
    SCOPED_TRACE(base::StringPrintf("StartPlayingFrom(%d)", milliseconds));
    const base::TimeDelta media_time = base::Milliseconds(milliseconds);
    time_source_.SetMediaTime(media_time);
    renderer_->StartPlayingFrom(media_time);
    base::RunLoop().RunUntilIdle();
  }

  void Flush() {
    SCOPED_TRACE("Flush()");
    WaitableMessageLoopEvent event;
    renderer_->Flush(event.GetClosure());
    event.RunAndWait();
  }

  void Destroy() {
    SCOPED_TRACE("Destroy()");
    renderer_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void OnDemuxerRead(DemuxerStream::ReadCB& read_cb) {
    if (simulate_demuxer_stall_after_n_reads_ >= 0) {
      if (simulate_demuxer_stall_after_n_reads_-- == 0) {
        stalled_demixer_read_cb_ = std::move(read_cb);
        return;
      }
    }

    scoped_refptr<DecoderBuffer> decoder_buffer(new DecoderBuffer(0));

    // Set |decoder_buffer| timestamp such that it won't match any of the
    // times provided to QueueFrames(). Otherwise the default timestamp of 0 may
    // match some frames and not others, which causes non-uniform handling in
    // DecoderStreamTraits.
    decoder_buffer->set_timestamp(kNoTimestamp);

    // Test hook for to specify a custom buffer duration.
    decoder_buffer->set_duration(buffer_duration_);

    std::move(read_cb).Run(DemuxerStream::kOk, {std::move(decoder_buffer)});
  }

  bool IsDemuxerStalled() { return !!stalled_demixer_read_cb_; }

  void UnstallDemuxer() {
    EXPECT_TRUE(IsDemuxerStalled());
    OnDemuxerRead(stalled_demixer_read_cb_);
  }

  // Parses a string representation of video frames and generates corresponding
  // VideoFrame objects in |decode_results_|.
  //
  // Syntax:
  //   nn - Queue a decoder buffer with timestamp nn * 1000us
  //   nndmm - Queue a decoder buffer with timestamp nn * 1000us
  //           and mm * 1000us duration
  //   abort - Queue an aborted read
  //   error - Queue a decoder error
  //
  // Examples:
  //   A clip that is four frames long: "0 10 20 30"
  //   A clip that has a decode error: "60 70 error"
  void QueueFrames(const std::string& str) {
    for (std::string_view token : base::SplitString(
             str, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      if (token == "abort") {
        scoped_refptr<VideoFrame> null_frame;
        QueueFrame(DecoderStatus::Codes::kAborted, null_frame);
        continue;
      }

      if (token == "error") {
        scoped_refptr<VideoFrame> null_frame;
        QueueFrame(DecoderStatus::Codes::kFailed, null_frame);
        continue;
      }

      auto ts_tokens = base::SplitStringPiece(token, "d", base::TRIM_WHITESPACE,
                                              base::SPLIT_WANT_ALL);
      if (ts_tokens.size() > 1) {
        token = ts_tokens[0];
      }

      int timestamp_in_ms = 0;
      if (base::StringToInt(token, &timestamp_in_ms)) {
        gfx::Size natural_size = TestVideoConfig::NormalCodedSize();
        scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
            PIXEL_FORMAT_I420, natural_size, gfx::Rect(natural_size),
            natural_size, base::Milliseconds(timestamp_in_ms));

        int duration_in_ms = 0;
        if (ts_tokens.size() > 1 &&
            base::StringToInt(ts_tokens[1], &duration_in_ms)) {
          frame->metadata().frame_duration = base::Milliseconds(duration_in_ms);
        }

        QueueFrame(DecoderStatus::Codes::kOk, frame);
        continue;
      }

      CHECK(false) << "Unrecognized decoder buffer token: " << token;
    }
  }

  // Queues video frames to be served by the decoder during rendering.
  void QueueFrame(DecoderStatus status, scoped_refptr<VideoFrame> frame) {
    decode_results_.push_back(std::make_pair(status, frame));
  }

  bool IsDecodePending() { return !!decode_cb_; }

  void WaitForError(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("WaitForError(%d)", expected.code()));

    WaitableMessageLoopEvent event;
    PipelineStatusCallback error_cb = event.GetPipelineStatusCB();
    EXPECT_CALL(mock_cb_, OnError(_))
        .WillOnce(Invoke([cb = &error_cb](PipelineStatus status) {
          std::move(*cb).Run(status);
        }));
    event.RunAndWaitForStatus(expected);
  }

  void WaitForEnded() {
    SCOPED_TRACE("WaitForEnded()");

    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnEnded())
        .WillOnce(RunOnceClosure(event.GetClosure()));
    event.RunAndWait();
  }

  void WaitForPendingDecode() {
    SCOPED_TRACE("WaitForPendingDecode()");
    if (decode_cb_)
      return;

    DCHECK(!wait_for_pending_decode_cb_);

    WaitableMessageLoopEvent event;
    wait_for_pending_decode_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(decode_cb_);
    DCHECK(!wait_for_pending_decode_cb_);
  }

  void SatisfyPendingDecode() {
    CHECK(decode_cb_);
    CHECK(!decode_results_.empty());

    // Post tasks for OutputCB and DecodeCB.
    scoped_refptr<VideoFrame> frame = decode_results_.front().second;
    if (frame.get())
      task_environment_.GetMainThreadTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(output_cb_, frame));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb_), decode_results_.front().first));
    decode_results_.pop_front();
  }

  void SatisfyPendingDecodeWithEndOfStream() {
    DCHECK(decode_cb_);

    // Return EOS buffer to trigger EOS frame.
    DemuxerStream::DecoderBufferVector buffers;
    buffers.emplace_back(DecoderBuffer::CreateEOSBuffer());
    EXPECT_CALL(demuxer_stream_, OnRead(_))
        .WillOnce(RunOnceCallback<0>(DemuxerStream::kOk, buffers));

    // Satisfy pending |decode_cb_| to trigger a new DemuxerStream::Read().
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb_), DecoderStatus::Codes::kOk));

    WaitForPendingDecode();

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb_), DecoderStatus::Codes::kOk));
  }

  bool HasQueuedFrames() const { return decode_results_.size() > 0; }

  void AdvanceWallclockTimeInMs(int time_ms) {
    EXPECT_TRUE(
        task_environment_.GetMainThreadTaskRunner()->BelongsToCurrentThread());
    base::AutoLock l(lock_);
    tick_clock_.Advance(base::Milliseconds(time_ms));
  }

  void AdvanceTimeInMs(int time_ms) {
    EXPECT_TRUE(
        task_environment_.GetMainThreadTaskRunner()->BelongsToCurrentThread());
    base::AutoLock l(lock_);
    time_ += base::Milliseconds(time_ms);
    time_source_.StopTicking();
    time_source_.SetMediaTime(time_);
    time_source_.StartTicking();
  }

  MOCK_METHOD0(OnSimulateDecodeDelay, base::TimeDelta(void));

 protected:
  base::test::TaskEnvironment task_environment_;
  NullMediaLog media_log_;

  // Fixture members.
  std::unique_ptr<VideoRendererImpl> renderer_;
  base::SimpleTestTickClock tick_clock_;
  raw_ptr<NiceMock<MockVideoDecoder>, DanglingUntriaged>
      decoder_;  // Owned by |renderer_|.
  NiceMock<MockDemuxerStream> demuxer_stream_;
  bool simulate_decode_delay_;

  bool expect_init_success_;

  // Specifies how many reads should complete before demuxer stalls.
  int simulate_demuxer_stall_after_n_reads_ = -1;
  DemuxerStream::ReadCB stalled_demixer_read_cb_;

  // Use StrictMock<T> to catch missing/extra callbacks.
  class MockCB : public MockRendererClient {
   public:
    MOCK_METHOD1(FrameReceived, void(scoped_refptr<VideoFrame>));
  };
  StrictMock<MockCB> mock_cb_;

  // Must be destroyed before |renderer_| since they share |tick_clock_|.
  std::unique_ptr<NullVideoSink> null_video_sink_;

  WallClockTimeSource time_source_;

  // Duration set on DecoderBuffers. See OnDemuxerRead().
  base::TimeDelta buffer_duration_;

 private:
  void DecodeRequested(scoped_refptr<DecoderBuffer> buffer,
                       VideoDecoder::DecodeCB& decode_cb) {
    EXPECT_TRUE(
        task_environment_.GetMainThreadTaskRunner()->BelongsToCurrentThread());
    CHECK(!decode_cb_);
    decode_cb_ = std::move(decode_cb);

    // Wake up WaitForPendingDecode() if needed.
    if (wait_for_pending_decode_cb_)
      std::move(wait_for_pending_decode_cb_).Run();

    if (decode_results_.empty())
      return;

    if (simulate_decode_delay_)
      tick_clock_.Advance(OnSimulateDecodeDelay());

    SatisfyPendingDecode();
  }

  void FlushRequested(base::OnceClosure& callback) {
    EXPECT_TRUE(
        task_environment_.GetMainThreadTaskRunner()->BelongsToCurrentThread());
    decode_results_.clear();
    if (decode_cb_) {
      QueueFrames("abort");
      SatisfyPendingDecode();
    }

    task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                          std::move(callback));
  }

  // Used to protect |time_|.
  base::Lock lock_;
  base::TimeDelta time_;

  // Used for satisfying reads.
  VideoDecoder::OutputCB output_cb_;
  VideoDecoder::DecodeCB decode_cb_;
  base::TimeDelta next_frame_timestamp_;

  // Run during DecodeRequested() to unblock WaitForPendingDecode().
  base::OnceClosure wait_for_pending_decode_cb_;

  base::circular_deque<std::pair<DecoderStatus, scoped_refptr<VideoFrame>>>
      decode_results_;
};

TEST_F(VideoRendererImplTest, DoNothing) {
  // Test that creation and deletion doesn't depend on calls to Initialize()
  // and/or Destroy().
}

TEST_F(VideoRendererImplTest, DestroyWithoutInitialize) {
  Destroy();
}

TEST_F(VideoRendererImplTest, Initialize) {
  Initialize();
  Destroy();
}

TEST_F(VideoRendererImplTest, InitializeAndStartPlayingFrom) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  Destroy();
}

TEST_F(VideoRendererImplTest, InitializeAndStartPlayingFromWithDuration) {
  Initialize();
  QueueFrames("0d10 10d10 20d10 30d10 40d10");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(10)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(std::optional<int>(100)));
  StartPlayingFrom(10);
  Destroy();
}

TEST_F(VideoRendererImplTest, InitializeAndEndOfStream) {
  Initialize();
  StartPlayingFrom(0);
  WaitForPendingDecode();
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    {
      // Buffering state changes must happen before end of stream.
      testing::InSequence in_sequence;
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
          .WillOnce(RunOnceClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, OnEnded());
    }
    SatisfyPendingDecodeWithEndOfStream();
    event.RunAndWait();
  }
  // Firing a time state changed to true should be ignored...
  renderer_->OnTimeProgressing();
  EXPECT_FALSE(null_video_sink_->is_started());
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingAfterEndOfStream) {
  Initialize();
  QueueFrames("0d10 10d10 20d10 30d10 40d10");
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(40)));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(40);
  WaitForPendingDecode();
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    {
      // Buffering state changes must happen before end of stream.
      testing::InSequence in_sequence;
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
          .WillOnce(RunOnceClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, OnEnded());
    }
    SatisfyPendingDecodeWithEndOfStream();
    event.RunAndWait();
  }
  // Firing a time state changed to true should be ignored...
  renderer_->OnTimeProgressing();
  EXPECT_FALSE(null_video_sink_->is_started());
  Destroy();
}

TEST_F(VideoRendererImplTest, InitializeAndEndOfStreamOneStaleFrame) {
  Initialize();
  StartPlayingFrom(10000);
  QueueFrames("0");
  QueueFrame(DecoderStatus::Codes::kOk, VideoFrame::CreateEOSFrame());
  WaitForPendingDecode();
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;

    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);

    {
      // Buffering state changes must happen before end of stream.
      testing::InSequence in_sequence;
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
          .WillOnce(RunOnceClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, OnEnded());
    }
    SatisfyPendingDecode();
    event.RunAndWait();
  }
  // Firing a time state changed to true should be ignored...
  renderer_->OnTimeProgressing();
  EXPECT_FALSE(null_video_sink_->is_started());
  Destroy();
}

TEST_F(VideoRendererImplTest, ReinitializeForAnotherStream) {
  Initialize();
  StartPlayingFrom(0);
  Flush();
  NiceMock<MockDemuxerStream> new_stream(DemuxerStream::VIDEO);
  new_stream.set_video_decoder_config(TestVideoConfig::Normal());
  InitializeRenderer(&new_stream, false, true);
}

TEST_F(VideoRendererImplTest, DestroyWhileInitializing) {
  CallInitialize(&demuxer_stream_, NewExpectedStatusCB(PIPELINE_ERROR_ABORT),
                 false, PIPELINE_OK);
  Destroy();
}

TEST_F(VideoRendererImplTest, DestroyWhileFlushing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  renderer_->Flush(NewExpectedClosure());
  Destroy();
}

TEST_F(VideoRendererImplTest, Play) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  Destroy();
}

TEST_F(VideoRendererImplTest, FlushWithNothingBuffered) {
  Initialize();
  StartPlayingFrom(0);

  // We shouldn't expect a buffering state change since we never reached
  // BUFFERING_HAVE_ENOUGH.
  Flush();
  Destroy();
}

// Verify that the flush callback is invoked outside of VideoRenderer lock, so
// we should be able to call other renderer methods from the Flush callback.
static void VideoRendererImplTest_FlushDoneCB(VideoRendererImplTest* test,
                                              VideoRenderer* renderer,
                                              base::OnceClosure success_cb) {
  test->QueueFrames("0 10 20 30");
  renderer->StartPlayingFrom(base::Seconds(0));
  std::move(success_cb).Run();
}

TEST_F(VideoRendererImplTest, FlushCallbackNoLock) {
  Initialize();
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  WaitableMessageLoopEvent event;
  renderer_->Flush(
      base::BindOnce(&VideoRendererImplTest_FlushDoneCB, base::Unretained(this),
                     base::Unretained(renderer_.get()), event.GetClosure()));
  event.RunAndWait();
  Destroy();
}

TEST_F(VideoRendererImplTest, DecodeError_Playing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(_)).Times(testing::AtLeast(1));

  // Consider the case that rendering is faster than we setup the test event.
  // In that case, when we run out of the frames, BUFFERING_HAVE_NOTHING will
  // be called.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _))
      .Times(testing::AtMost(1));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);

  StartPlayingFrom(0);
  renderer_->OnTimeProgressing();
  time_source_.StartTicking();
  AdvanceTimeInMs(10);

  QueueFrames("error");
  SatisfyPendingDecode();
  WaitForError(PIPELINE_ERROR_DECODE);
  Destroy();
}

TEST_F(VideoRendererImplTest, DecodeError_DuringStartPlayingFrom) {
  Initialize();
  QueueFrames("error");
  EXPECT_CALL(mock_cb_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)));
  EXPECT_CALL(mock_cb_, OnFallback(HasStatusCode(PIPELINE_ERROR_DECODE)));
  StartPlayingFrom(0);
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFrom_Exact) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(60)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(60);
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFrom_RightBefore) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(50)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(std::optional<int>(100)));
  StartPlayingFrom(59);
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFrom_RightAfter) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(60)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(std::optional<int>(100)));
  StartPlayingFrom(61);
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFrom_LowDelay) {
  // In low-delay mode only one frame is required to finish preroll. But frames
  // prior to the start time will not be used.
  InitializeWithLowDelay(true);
  QueueFrames("0 10");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(10)));
  // Expect some amount of have enough/nothing due to only requiring one frame.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
      .Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _))
      .Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(10);

  QueueFrames("20");
  SatisfyPendingDecode();

  renderer_->OnTimeProgressing();
  time_source_.StartTicking();

  WaitableMessageLoopEvent event;
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(20)))
      .WillOnce(RunOnceClosure(event.GetClosure()));
  AdvanceTimeInMs(20);
  event.RunAndWait();

  Destroy();
}

// Verify that a late decoder response doesn't break invariants in the renderer.
TEST_F(VideoRendererImplTest, DestroyDuringOutstandingRead) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);

  // Check that there is an outstanding Read() request.
  EXPECT_TRUE(IsDecodePending());

  Destroy();
}

// Verifies that the first frame is painted w/o rendering being started.
TEST_F(VideoRendererImplTest, RenderingStopsAfterFirstFrame) {
  InitializeWithLowDelay(true);
  QueueFrames("0");

  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnEnded()).Times(0);

  {
    SCOPED_TRACE("Waiting for first frame to be painted.");
    WaitableMessageLoopEvent event;

    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    StartPlayingFrom(0);

    EXPECT_TRUE(IsDecodePending());
    SatisfyPendingDecodeWithEndOfStream();

    event.RunAndWait();
  }

  Destroy();
}
// Verifies that the first frame is eventually painted even if its not the best.
TEST_F(VideoRendererImplTest, PaintFirstFrameOnStall) {
  Initialize();
  QueueFrames("0d10");
  ON_CALL(*decoder_, CanReadWithoutStalling()).WillByDefault(Return(false));

  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnEnded()).Times(0);

  {
    SCOPED_TRACE("Waiting for first frame to be painted.");
    WaitableMessageLoopEvent event;

    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    StartPlayingFrom(10);

    EXPECT_TRUE(IsDecodePending());

    event.RunAndWait();
  }

  Destroy();
}

// Verifies that the sink is stopped after rendering the first frame if
// playback has started.
TEST_F(VideoRendererImplTest, RenderingStopsAfterOneFrameWithEOS) {
  InitializeWithLowDelay(true);
  QueueFrames("0");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0))).Times(1);
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);

  {
    SCOPED_TRACE("Waiting for sink to stop.");
    WaitableMessageLoopEvent event;

    null_video_sink_->set_stop_cb(event.GetClosure());
    StartPlayingFrom(0);
    renderer_->OnTimeProgressing();

    EXPECT_TRUE(IsDecodePending());
    SatisfyPendingDecodeWithEndOfStream();
    WaitForEnded();

    renderer_->OnTimeStopped();
    event.RunAndWait();
  }

  Destroy();
}

// Tests the case where the video started and received a single Render() call,
// then the video was put into the background.
TEST_F(VideoRendererImplTest, RenderingStartedThenStopped) {
  Initialize();
  QueueFrames("0 30 60 90");

  // Start the sink and wait for the first callback.  Set statistics to a non
  // zero value, once we have some decoded frames they should be overwritten.
  PipelineStatistics last_pipeline_statistics;
  last_pipeline_statistics.video_frames_dropped = 1;
  {
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_))
        .Times(5)
        .WillRepeatedly(SaveArg<0>(&last_pipeline_statistics));
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
    StartPlayingFrom(0);
    event.RunAndWait();
    Mock::VerifyAndClearExpectations(&mock_cb_);
  }

  // Four calls to update statistics should have been made, each reporting a
  // single decoded frame and one frame worth of memory usage. No dropped frames
  // should be reported later since we're in background rendering mode. These
  // calls must all have occurred before playback starts.
  EXPECT_EQ(0u, last_pipeline_statistics.video_frames_dropped);
  EXPECT_EQ(1u, last_pipeline_statistics.video_frames_decoded);

  // Note: This is not the total, but just the increase in the last call since
  // the previous call, the total should be 4 * 115200.
  EXPECT_EQ(115200, last_pipeline_statistics.video_memory_usage);

  EXPECT_EQ(renderer_->GetPreferredRenderInterval(),
            last_pipeline_statistics.video_frame_duration_average);

  // Consider the case that rendering is faster than we setup the test event.
  // In that case, when we run out of the frames, BUFFERING_HAVE_NOTHING will
  // be called. And then during SatisfyPendingDecodeWithEndOfStream,
  // BUFFER_HAVE_ENOUGH will be called again.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
      .Times(testing::AtMost(1));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _))
      .Times(testing::AtMost(1));
  renderer_->OnTimeProgressing();
  time_source_.StartTicking();

  // Suspend all future callbacks and synthetically advance the media time,
  // because this is a background render, we won't underflow by waiting until
  // a pending read is ready.
  null_video_sink_->set_background_render(true);
  AdvanceTimeInMs(91);
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(90)));
  WaitForPendingDecode();

  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_))
      .WillOnce(SaveArg<0>(&last_pipeline_statistics));
  SatisfyPendingDecodeWithEndOfStream();

  AdvanceTimeInMs(30);
  WaitForEnded();

  EXPECT_EQ(0u, last_pipeline_statistics.video_frames_dropped);
  EXPECT_EQ(0u, last_pipeline_statistics.video_frames_decoded);

  // Memory usage is relative, so the prior lines increased memory usage to
  // 4 * 115200, so this last one should show we only have 1 frame left.
  EXPECT_EQ(-3 * 115200, last_pipeline_statistics.video_memory_usage);

  Destroy();
}

// Tests the case where underflow evicts all frames before EOS.
TEST_F(VideoRendererImplTest, UnderflowEvictionBeforeEOS) {
  Initialize();
  QueueFrames("0 30 60 90 100");

  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    EXPECT_CALL(mock_cb_, FrameReceived(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
    StartPlayingFrom(0);
    event.RunAndWait();
  }

  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_NOTHING");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    renderer_->OnTimeProgressing();
    time_source_.StartTicking();
    // Jump time far enough forward that no frames are valid.
    AdvanceTimeInMs(1000);
    event.RunAndWait();
  }

  WaitForPendingDecode();

  renderer_->OnTimeStopped();
  time_source_.StopTicking();

  // Providing the end of stream packet should remove all frames and exit.
  SatisfyPendingDecodeWithEndOfStream();
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  WaitForEnded();
  Destroy();
}

// Tests the case where underflow evicts all frames in the HAVE_ENOUGH state.
TEST_F(VideoRendererImplTest, UnderflowEvictionWhileHaveEnough) {
  Initialize();
  QueueFrames("0 30 60 90 100");

  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    EXPECT_CALL(mock_cb_, FrameReceived(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
    StartPlayingFrom(0);
    event.RunAndWait();
  }

  // Now wait until we have no effective frames.
  {
    SCOPED_TRACE("Waiting for zero effective frames.");
    WaitableMessageLoopEvent event;
    null_video_sink_->set_background_render(true);
    time_source_.StartTicking();
    AdvanceTimeInMs(1000);
    renderer_->OnTimeProgressing();
    EXPECT_CALL(mock_cb_, FrameReceived(_))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    event.RunAndWait();
    ASSERT_EQ(renderer_->effective_frames_queued_for_testing(), 0u);
  }

  // When OnTimeStopped() is called it should transition to HAVE_NOTHING.
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_NOTHING");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    renderer_->OnTimeStopped();
    event.RunAndWait();
  }

  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFromThenFlushThenEOS) {
  Initialize();
  QueueFrames("0 30 60 90");

  WaitableMessageLoopEvent event;
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
      .WillOnce(RunOnceClosure(event.GetClosure()));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  event.RunAndWait();

  // Cycle ticking so that we get a non-null reference time.
  time_source_.StartTicking();
  time_source_.StopTicking();

  // Flush and simulate a seek past EOS, where some error prevents the decoder
  // from returning any frames.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  Flush();

  StartPlayingFrom(200);
  WaitForPendingDecode();
  SatisfyPendingDecodeWithEndOfStream();
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  WaitForEnded();
  Destroy();
}

TEST_F(VideoRendererImplTest, FramesAreNotExpiredDuringPreroll) {
  Initialize();
  // !CanReadWithoutStalling() puts the renderer in state BUFFERING_HAVE_ENOUGH
  // after the first frame.
  ON_CALL(*decoder_, CanReadWithoutStalling()).WillByDefault(Return(false));
  // Set background rendering to simulate the first couple of Render() calls
  // by VFC.
  null_video_sink_->set_background_render(true);
  QueueFrames("0 10 20");
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
      .Times(testing::AtMost(1));
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);

  renderer_->OnTimeProgressing();
  time_source_.StartTicking();

  WaitableMessageLoopEvent event;
  // Frame "10" should not have been expired.
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(10)))
      .WillOnce(RunOnceClosure(event.GetClosure()));
  AdvanceTimeInMs(10);
  event.RunAndWait();

  Destroy();
}

TEST_F(VideoRendererImplTest, VideoConfigChange) {
  Initialize();

  // Configure demuxer stream to allow config changes.
  EXPECT_CALL(demuxer_stream_, SupportsConfigChanges())
      .WillRepeatedly(Return(true));

  // Signal a config change at the next DemuxerStream::Read().
  DemuxerStream::DecoderBufferVector buffers;
  EXPECT_CALL(demuxer_stream_, OnRead(_))
      .WillOnce(RunOnceCallback<0>(DemuxerStream::kConfigChanged, buffers));

  // Use LargeEncrypted config (non-default) to ensure its plumbed through to
  // callback.
  demuxer_stream_.set_video_decoder_config(TestVideoConfig::LargeEncrypted());

  EXPECT_CALL(mock_cb_, OnVideoConfigChange(
                            DecoderConfigEq(TestVideoConfig::LargeEncrypted())))
      .Times(1);

  // Start plyaing to trigger DemuxerStream::Read(), surfacing the config change
  StartPlayingFrom(0);

  Destroy();
}

TEST_F(VideoRendererImplTest, NaturalSizeChange) {
  Initialize();

  gfx::Size initial_size(8, 8);
  gfx::Size larger_size(16, 16);

  QueueFrame(DecoderStatus::Codes::kOk,
             VideoFrame::CreateFrame(PIXEL_FORMAT_I420, initial_size,
                                     gfx::Rect(initial_size), initial_size,
                                     base::Milliseconds(0)));
  QueueFrame(DecoderStatus::Codes::kOk,
             VideoFrame::CreateFrame(PIXEL_FORMAT_I420, larger_size,
                                     gfx::Rect(larger_size), larger_size,
                                     base::Milliseconds(10)));
  QueueFrame(DecoderStatus::Codes::kOk,
             VideoFrame::CreateFrame(PIXEL_FORMAT_I420, larger_size,
                                     gfx::Rect(larger_size), larger_size,
                                     base::Milliseconds(20)));
  QueueFrame(DecoderStatus::Codes::kOk,
             VideoFrame::CreateFrame(PIXEL_FORMAT_I420, initial_size,
                                     gfx::Rect(initial_size), initial_size,
                                     base::Milliseconds(30)));

  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);

  {
    // Callback is fired for the first frame.
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(initial_size));
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
    StartPlayingFrom(0);
    renderer_->OnTimeProgressing();
    time_source_.StartTicking();
  }
  {
    // Callback should be fired once when switching to the larger size.
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(larger_size));
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(10)))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }
  {
    // Called is not fired because frame size does not change.
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(20)))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }
  {
    // Callback is fired once when switching to the larger size.
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(initial_size));
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(30)))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }

  Destroy();
}

TEST_F(VideoRendererImplTest, OpacityChange) {
  Initialize();

  gfx::Size frame_size(8, 8);
  VideoPixelFormat opaque_format = PIXEL_FORMAT_I420;
  VideoPixelFormat non_opaque_format = PIXEL_FORMAT_I420A;

  QueueFrame(DecoderStatus::Codes::kOk,
             VideoFrame::CreateFrame(non_opaque_format, frame_size,
                                     gfx::Rect(frame_size), frame_size,
                                     base::Milliseconds(0)));
  QueueFrame(DecoderStatus::Codes::kOk,
             VideoFrame::CreateFrame(non_opaque_format, frame_size,
                                     gfx::Rect(frame_size), frame_size,
                                     base::Milliseconds(10)));
  QueueFrame(
      DecoderStatus::Codes::kOk,
      VideoFrame::CreateFrame(opaque_format, frame_size, gfx::Rect(frame_size),
                              frame_size, base::Milliseconds(20)));
  QueueFrame(
      DecoderStatus::Codes::kOk,
      VideoFrame::CreateFrame(opaque_format, frame_size, gfx::Rect(frame_size),
                              frame_size, base::Milliseconds(30)));

  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(frame_size)).Times(1);

  {
    // Callback is fired for the first frame.
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(false));
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
    StartPlayingFrom(0);
    renderer_->OnTimeProgressing();
    time_source_.StartTicking();
  }
  {
    // Callback is not fired because opacity does not change.
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(10)))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }
  {
    // Called is fired when opacity changes.
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(true));
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(20)))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }
  {
    // Callback is not fired because opacity does not change.
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(30)))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }

  Destroy();
}

TEST_F(VideoRendererImplTest, VideoFrameRateChange) {
  Initialize();

  EXPECT_CALL(mock_cb_, FrameReceived(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(_, _)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);

  // Send 50fps frames first.
  EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(std::optional<int>(50)));
  QueueFrames("0 20 40 60 80 100 120 140 160 180 200");
  QueueFrames("220 240 260 280 300 320 340 360 380 400");

  // Also queue some frames that aren't at 50fps, so that we get an unknown fps.
  EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(std::optional<int>()));
  QueueFrames("500 600");

  // Drain everything.
  StartPlayingFrom(0);
  renderer_->OnTimeProgressing();
  time_source_.StartTicking();
  // Send in all the frames we queued.
  while (HasQueuedFrames()) {
    AdvanceTimeInMs(20);
    AdvanceWallclockTimeInMs(20);
    // This runs the sink callbacks to consume frames.
    task_environment_.FastForwardBy(base::Milliseconds(20));
    base::RunLoop().RunUntilIdle();
  }

  Destroy();
}

class VideoRendererImplAsyncAddFrameReadyTest : public VideoRendererImplTest {
 public:
  void InitializeWithMockGpuMemoryBufferVideoFramePool() {
    renderer_ = std::make_unique<VideoRendererImpl>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        null_video_sink_.get(),
        base::BindRepeating(&VideoRendererImplAsyncAddFrameReadyTest::
                                CreateVideoDecodersForTest,
                            base::Unretained(this)),
        true, &media_log_,
        std::make_unique<MockGpuMemoryBufferVideoFramePool>(&frame_ready_cbs_),
        0);
    VideoRendererImplTest::Initialize();
  }

 protected:
  std::vector<base::OnceClosure> frame_ready_cbs_;
};

TEST_F(VideoRendererImplAsyncAddFrameReadyTest, InitializeAndStartPlayingFrom) {
  InitializeWithMockGpuMemoryBufferVideoFramePool();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  ASSERT_EQ(1u, frame_ready_cbs_.size());

  uint32_t frame_ready_index = 0;
  while (frame_ready_index < frame_ready_cbs_.size()) {
    std::move(frame_ready_cbs_[frame_ready_index++]).Run();
    base::RunLoop().RunUntilIdle();
  }
  Destroy();
}

TEST_F(VideoRendererImplAsyncAddFrameReadyTest, WeakFactoryDiscardsOneFrame) {
  InitializeWithMockGpuMemoryBufferVideoFramePool();
  QueueFrames("0 10 20 30");
  StartPlayingFrom(0);
  Flush();
  ASSERT_EQ(1u, frame_ready_cbs_.size());
  // This frame will be discarded.
  std::move(frame_ready_cbs_.front()).Run();
  Destroy();
}

enum class UnderflowTestType {
  // Renderer will require a default amount of buffering to reach HAVE_ENOUGH.
  NORMAL,
  // Both of these require only a single frame to reach HAVE_ENOUGH.
  LOW_DELAY,
  CANT_READ_WITHOUT_STALLING
};

class UnderflowTest
    : public VideoRendererImplTest,
      public testing::WithParamInterface<
          ::std::tuple<UnderflowTestType, BufferingStateChangeReason>> {
 protected:
  void SetUp() override { std::tie(test_type, underflow_type) = GetParam(); }

  void TestBufferToHaveEnoughThenUnderflow() {
    InitializeWithLowDelay(test_type == UnderflowTestType::LOW_DELAY);

    if (test_type == UnderflowTestType::CANT_READ_WITHOUT_STALLING)
      ON_CALL(*decoder_, CanReadWithoutStalling()).WillByDefault(Return(false));

    if (underflow_type == DEMUXER_UNDERFLOW) {
      simulate_demuxer_stall_after_n_reads_ = 4;
    }
    QueueFrames("0 20 40 60");

    {
      WaitableMessageLoopEvent event;
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
      EXPECT_CALL(mock_cb_,
                  OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                         BUFFERING_CHANGE_REASON_UNKNOWN))
          .WillOnce(RunOnceClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
      EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
      EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
      StartPlayingFrom(0);
      event.RunAndWait();
      Mock::VerifyAndClearExpectations(&mock_cb_);
    }

    // Start playing.
    time_source_.StartTicking();
    renderer_->OnTimeProgressing();

    // Advance time slightly, but enough to exceed the duration of the last
    // frame.
    // Frames should be dropped and we should NOT signal having nothing.
    {
      SCOPED_TRACE("Waiting for frame drops");
      WaitableMessageLoopEvent event;
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(20))).Times(0);

      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(40))).Times(0);
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(60)))
          .WillOnce(RunOnceClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
      AdvanceTimeInMs(61);

      event.RunAndWait();
      Mock::VerifyAndClearExpectations(&mock_cb_);
    }

    // Advance time more. Now we should signal having nothing. And put
    // the last frame up for display.
    {
      SCOPED_TRACE("Waiting for BUFFERING_HAVE_NOTHING");
      WaitableMessageLoopEvent event;
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                                   underflow_type))
          .WillOnce(RunOnceClosure(event.GetClosure()));
      AdvanceTimeInMs(18);
      event.RunAndWait();
      Mock::VerifyAndClearExpectations(&mock_cb_);
    }

    EXPECT_EQ(1u, renderer_->frames_queued_for_testing());
    time_source_.StopTicking();
    renderer_->OnTimeStopped();
    EXPECT_EQ(0u, renderer_->frames_queued_for_testing());
    ASSERT_EQ(underflow_type == DEMUXER_UNDERFLOW, IsDemuxerStalled());
    ASSERT_EQ(underflow_type == DECODER_UNDERFLOW, IsDecodePending());

    // Stopping time signals a confirmed underflow to VRI. Verify updates to
    // buffering limits.
    switch (test_type) {
      // In the normal and cant_read modes, min and max buffered frames should
      // always be equal, and both should increase upon underflow.
      case UnderflowTestType::NORMAL:
      case UnderflowTestType::CANT_READ_WITHOUT_STALLING:
        EXPECT_EQ(renderer_->min_buffered_frames_for_testing(),
                  limits::kMaxVideoFrames + 1);
        EXPECT_EQ(renderer_->max_buffered_frames_for_testing(),
                  limits::kMaxVideoFrames + 1);
        break;
      // In low_delay mode only the max should increase while min remains 1.
      case UnderflowTestType::LOW_DELAY:
        EXPECT_EQ(renderer_->min_buffered_frames_for_testing(), 1);
        EXPECT_EQ(renderer_->max_buffered_frames_for_testing(),
                  limits::kMaxVideoFrames + 1);
        break;
    }
  }

  UnderflowTestType test_type;
  BufferingStateChangeReason underflow_type;
};

TEST_P(UnderflowTest, UnderflowAndEosTest) {
  TestBufferToHaveEnoughThenUnderflow();

  if (IsDemuxerStalled())
    UnstallDemuxer();

  // Receiving end of stream should signal having enough.
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    EXPECT_CALL(mock_cb_, OnEnded());
    SatisfyPendingDecodeWithEndOfStream();
    event.RunAndWait();
  }

  Destroy();
}

TEST_P(UnderflowTest, UnderflowAndRecoverTest) {
  TestBufferToHaveEnoughThenUnderflow();

  if (IsDemuxerStalled())
    UnstallDemuxer();

  // Queue some frames, satisfy reads, and make sure expired frames are gone
  // when the renderer paints the first frame.
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(80))).Times(1);
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN))
        .WillOnce(RunOnceClosure(event.GetClosure()));

    switch (test_type) {
      // In the normal underflow case we queue 5 frames here instead of four
      // since the underflow increases the number of required frames to reach
      // the have enough state.
      case UnderflowTestType::NORMAL:
        QueueFrames("80 100 120 140 160");
        EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(std::optional<int>(50)));
        break;
      // In either of these modes the HAVE_ENOUGH transition should still
      // occur with a single frame.
      case UnderflowTestType::LOW_DELAY:
      case UnderflowTestType::CANT_READ_WITHOUT_STALLING:
        QueueFrames("80");
        break;
    }
    SatisfyPendingDecode();
    event.RunAndWait();
  }

  Destroy();
}

INSTANTIATE_TEST_SUITE_P(
    ChrisTest,
    UnderflowTest,
    Combine(Values(UnderflowTestType::NORMAL,
                   UnderflowTestType::LOW_DELAY,
                   UnderflowTestType::CANT_READ_WITHOUT_STALLING),
            Values(DEMUXER_UNDERFLOW, DECODER_UNDERFLOW)));

class VideoRendererLatencyHintTest : public VideoRendererImplTest {
 public:
  void VerifyDefaultRebufferingBehavior(int start_playing_from) {
    // Keep it simple. Only call this if you're starting from empty.
    DCHECK_EQ(renderer_->effective_frames_queued_for_testing(), 0u);

    // Initial frames should trigger various callbacks.
    EXPECT_CALL(mock_cb_,
                FrameReceived(HasTimestampMatcher(start_playing_from)));
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(_)).Times(AnyNumber());

    // Queue 3 frames, 20 msec apart. Stop 1 shy of min_buffered_frames_.
    ASSERT_EQ(renderer_->min_buffered_frames_for_testing(), 4);
    int frame_time = start_playing_from;
    for (int i = 0; i < 3; i++) {
      QueueFrames(base::NumberToString(frame_time));
      frame_time += 20;
    }

    // Verify no transition to HAVE_ENOUGH since 3 < |min_buffered_frames_|
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
        .Times(0);

    StartPlayingFrom(start_playing_from);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(renderer_->effective_frames_queued_for_testing(), 3u);
    Mock::VerifyAndClearExpectations(&mock_cb_);

    // Queuing one extra frame should trigger the transition.
    QueueFrames(base::NumberToString(frame_time));
    SatisfyPendingDecode();
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&mock_cb_);
  }
};

// Test default HaveEnough transition when no latency hint is set.
TEST_F(VideoRendererLatencyHintTest, HaveEnough_NoLatencyHint) {
  Initialize();
  VerifyDefaultRebufferingBehavior(0);
  Destroy();
}

// Test early HaveEnough transition when low latency hint is set.
TEST_F(VideoRendererLatencyHintTest, HaveEnough_LowLatencyHint) {
  Initialize();

  // Set latencyHint to bare minimum.
  renderer_->SetLatencyHint(base::TimeDelta());

  // Initial frames should trigger various callbacks.
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(2);
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));

  // Only 1 frame should be needed to trigger have enough.
  ASSERT_EQ(renderer_->min_buffered_frames_for_testing(), 1);
  QueueFrames("0");

  StartPlayingFrom(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(renderer_->effective_frames_queued_for_testing(), 1u);
  Mock::VerifyAndClearExpectations(&mock_cb_);

  // Verify latency hint doesn't reduce our ability to buffer beyond the
  // 1-frame HAVE_ENOUGH (i.e. don't throttle decoding in the name of latency).
  EXPECT_EQ(renderer_->max_buffered_frames_for_testing(), 4);
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(3);
  QueueFrames("10 20 30");
  WaitForPendingDecode();
  SatisfyPendingDecode();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(renderer_->frames_queued_for_testing(), 4u);

  // Unset latencyHint, to verify default behavior.
  renderer_->SetLatencyHint(std::nullopt);

  // Flush to return to clean slate.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  Flush();

  VerifyDefaultRebufferingBehavior(1000);

  Destroy();
}

// Test late HaveEnough transition when high latency hint is set.
TEST_F(VideoRendererLatencyHintTest, HaveEnough_HighLatencyHint) {
  // We must provide a |buffer_duration_| for the latencyHint to take effect
  // immediately. The VideoRendererAlgorithm will eventually provide a PTS-delta
  // duration, but not until after we've started rendering.
  buffer_duration_ = base::Milliseconds(30);

  // Set latencyHint to a large value.
  renderer_->SetLatencyHint(base::Milliseconds(400));

  // NOTE: other tests will SetLatencyHint after Initialize(). Either way should
  // work. Initializing later is especially interesting for "high" hints because
  // the renderer will try to set buffering caps based on stream state that
  // isn't yet available.
  Initialize();

  // Initial frames should trigger various callbacks.
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());

  // Queue 12 frames, each 30 ms apart. At this framerate, 400ms rounds to 13
  // frames, so 12 frames should be 1 shy of the HaveEnough threshold.
  EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(std::optional<int>(33)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
      .Times(0);
  QueueFrames("0 30 60 90 120 150 180 210 240 270 300 330");

  StartPlayingFrom(0);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(renderer_->min_buffered_frames_for_testing(), 13);
  EXPECT_EQ(renderer_->effective_frames_queued_for_testing(), 12u);
  Mock::VerifyAndClearExpectations(&mock_cb_);

  // Queue 1 additional frame and verify HaveEnough threshold is reached.
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  QueueFrames("360");
  SatisfyPendingDecode();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_cb_);

  // Unset latencyHint, to verify default behavior.
  renderer_->SetLatencyHint(std::nullopt);

  // Flush to return to clean slate.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  Flush();
  Mock::VerifyAndClearExpectations(&mock_cb_);

  VerifyDefaultRebufferingBehavior(1000);

  Destroy();
}

// Test updates to buffering limits upon underflow when latency hint set.
TEST_F(VideoRendererLatencyHintTest,
       LatencyHintUnderflowUpdatesMaxBufferingLimit) {
  // Enable low delay mode. Low delay mode is tested separately.
  InitializeWithLowDelay(true);
  EXPECT_EQ(renderer_->min_buffered_frames_for_testing(), 1);

  // We must provide a |buffer_duration_| for the latencyHint to take effect
  // immediately. The VideoRendererAlgorithm will eventually provide a PTS-delta
  // duration, but not until after we've started rendering.
  buffer_duration_ = base::Milliseconds(30);

  // Set latency hint to a medium value.
  renderer_->SetLatencyHint(base::Milliseconds(200));

  // Stall the demuxer after 7 frames.
  simulate_demuxer_stall_after_n_reads_ = 7;

  // Queue up enough frames to trigger HAVE_ENOUGH. Each frame is 30 ms apart.
  // At this spacing, 200ms rounds to 7 frames.
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(std::optional<int>(33)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  QueueFrames("0 30 60 90 120 150 180");
  StartPlayingFrom(0);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(renderer_->min_buffered_frames_for_testing(), 7);
  EXPECT_EQ(renderer_->effective_frames_queued_for_testing(), 7u);
  Mock::VerifyAndClearExpectations(&mock_cb_);

  // Advance time to trigger HAVE_NOTHING (underflow).
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_NOTHING");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(180)));
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                                 DEMUXER_UNDERFLOW))
        .WillOnce(RunOnceClosure(event.GetClosure()));
    renderer_->OnTimeProgressing();
    time_source_.StartTicking();
    AdvanceTimeInMs(300);
    event.RunAndWait();
    Mock::VerifyAndClearExpectations(&mock_cb_);
  }

  // Simulate delayed buffering state callbacks.
  time_source_.StopTicking();
  renderer_->OnTimeStopped();

  // When latency hint set the max should increase while min remains steady
  // (user controls the min via hint).
  EXPECT_EQ(renderer_->min_buffered_frames_for_testing(), 7);
  EXPECT_EQ(renderer_->max_buffered_frames_for_testing(), 7 + 1);

  Destroy();
}

// Test that latency hint overrides low delay mode.
TEST_F(VideoRendererLatencyHintTest, LatencyHintOverridesLowDelay) {
  // Enable low delay mode. Low delay mode is tested separately.
  InitializeWithLowDelay(true);
  EXPECT_EQ(renderer_->min_buffered_frames_for_testing(), 1);

  // We must provide a |buffer_duration_| for the latencyHint to take effect
  // immediately. The VideoRendererAlgorithm will eventually provide a PTS-delta
  // duration, but not until after we've started rendering.
  buffer_duration_ = base::Milliseconds(30);

  // Set latency hint to a medium value.
  renderer_->SetLatencyHint(base::Milliseconds(200));

  // Initial frames should trigger various callbacks.
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());

  // Queue 6 frames, each 30 ms apart. At this spacing, 200ms rounds to
  // 7 frames, so 6 frames should be 1 shy of the HaveEnough threshold. Verify
  // that HAVE_ENOUGH is not triggered in spite of being initialized with low
  // delay mode.
  EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(std::optional<int>(33)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
      .Times(0);
  QueueFrames("0 30 60 90 120 150");
  StartPlayingFrom(0);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(renderer_->min_buffered_frames_for_testing(), 7);
  EXPECT_EQ(renderer_->effective_frames_queued_for_testing(), 6u);
  Mock::VerifyAndClearExpectations(&mock_cb_);

  // Queue 1 additional frame and verify HaveEnough threshold is reached.
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  QueueFrames("180");
  SatisfyPendingDecode();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_cb_);

  // Unset latencyHint, to verify default behavior. NOTE: low delay mode is not
  // restored when latency hint unset.
  renderer_->SetLatencyHint(std::nullopt);

  // Flush to return to clean slate.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  Flush();
  Mock::VerifyAndClearExpectations(&mock_cb_);

  VerifyDefaultRebufferingBehavior(1000);

  Destroy();
}

// Test that !CanReadWithoutStalling() overrides latency hint.
TEST_F(VideoRendererLatencyHintTest,
       CantReadWithoutStallingOverridesLatencyHint) {
  Initialize();

  // Let decoder indicate that it CAN'T read without stalling, meaning we should
  // enter HAVE_ENOUGH with just one effective frame (waiting for more frames
  // will stall the decoder).
  ON_CALL(*decoder_, CanReadWithoutStalling()).WillByDefault(Return(false));

  // We must provide a |buffer_duration_| for the latencyHint to take effect
  // immediately. The VideoRendererAlgorithm will eventually provide a PTS-delta
  // duration, but not until after we've started rendering.
  buffer_duration_ = base::Milliseconds(30);

  // Set latency hint to a medium value. At a spacing of 30ms this would set
  // the HAVE_ENOUGH threshold to 4 frames.
  renderer_->SetLatencyHint(base::Milliseconds(200));

  // Initial frames should trigger various callbacks.
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(0)));
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());

  // Queue 1 frame. This is well short of what the latency hint would require,
  // but we CAN'T READ WITHOUT STALLING, so expect a transition to HAVE_ENOUGH
  // after just 1 frame.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  QueueFrames("0");
  StartPlayingFrom(0);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(renderer_->min_buffered_frames_for_testing(), 7);
  EXPECT_EQ(renderer_->effective_frames_queued_for_testing(), 1u);
  Mock::VerifyAndClearExpectations(&mock_cb_);

  // Queue some additional frames, verify buffering state holds at HAVE_ENOUGH.
  QueueFrames("30 60 90 120");
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(_, _)).Times(0);
  // SatisfyPendingDecode();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_cb_);

  // Unset latency hint to verify 1-frame HAVE_ENOUGH threshold is maintained.
  renderer_->SetLatencyHint(std::nullopt);

  // Flush to return to clean slate.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  Flush();
  Mock::VerifyAndClearExpectations(&mock_cb_);

  // Expect HAVE_ENOUGH (and various other callbacks) again.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _));
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestampMatcher(1000)));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoFrameRateChange(_)).Times(AnyNumber());

  // Queue 1 frame.
  QueueFrames("1000");
  StartPlayingFrom(1000);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(renderer_->min_buffered_frames_for_testing(), 4);
  EXPECT_EQ(renderer_->effective_frames_queued_for_testing(), 1u);
  Mock::VerifyAndClearExpectations(&mock_cb_);

  Destroy();
}

}  // namespace media

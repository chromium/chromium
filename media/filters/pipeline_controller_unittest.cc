// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/pipeline_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/mock_filters.h"
#include "media/base/pipeline.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceClosure;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

class PipelineControllerTest : public ::testing::Test, public Pipeline::Client {
 public:
  PipelineControllerTest()
      : pipeline_(new StrictMock<MockPipeline>()),
        pipeline_controller_(
            std::unique_ptr<Pipeline>(pipeline_),
            base::BindRepeating(&PipelineControllerTest::OnStarted,
                                base::Unretained(this)),
            base::BindRepeating(&PipelineControllerTest::OnSeeked,
                                base::Unretained(this)),
            base::BindRepeating(&PipelineControllerTest::OnSuspended,
                                base::Unretained(this)),
            base::BindRepeating(&PipelineControllerTest::OnBeforeResume,
                                base::Unretained(this)),
            base::BindRepeating(&PipelineControllerTest::OnResumed,
                                base::Unretained(this)),
            base::BindRepeating(&PipelineControllerTest::OnError,
                                base::Unretained(this))) {}

  PipelineControllerTest(const PipelineControllerTest&) = delete;
  PipelineControllerTest& operator=(const PipelineControllerTest&) = delete;

  ~PipelineControllerTest() override = default;

  PipelineStatusCallback StartPipeline(bool is_streaming, bool is_static) {
    EXPECT_FALSE(pipeline_controller_.IsStable());
    PipelineStatusCallback start_cb;
    EXPECT_CALL(*pipeline_, OnStart(_, _, _, _))
        .WillOnce(MoveArg<3>(&start_cb));
    pipeline_controller_.Start(Pipeline::StartType::kNormal, &demuxer_, this,
                               is_streaming, is_static);
    Mock::VerifyAndClear(pipeline_);
    EXPECT_CALL(*pipeline_, IsSuspended())
        .Times(AnyNumber())
        .WillOnce(Return(false));
    EXPECT_FALSE(pipeline_controller_.IsStable());
    return start_cb;
  }

  PipelineStatusCallback StartPipeline() { return StartPipeline(false, true); }

  PipelineStatusCallback StartPipeline_WithDynamicData() {
    return StartPipeline(false, false);
  }

  PipelineStatusCallback StartPipeline_WithStreamingData() {
    return StartPipeline(true, false);
  }

  PipelineStatusCallback SeekPipeline(base::TimeDelta time) {
    EXPECT_TRUE(pipeline_controller_.IsStable());
    PipelineStatusCallback seek_cb;
    EXPECT_CALL(*pipeline_, OnSeek(time, _)).WillOnce(MoveArg<1>(&seek_cb));
    pipeline_controller_.Seek(time, true);
    Mock::VerifyAndClear(pipeline_);
    EXPECT_FALSE(pipeline_controller_.IsStable());
    return seek_cb;
  }

  PipelineStatusCallback SuspendPipeline() {
    EXPECT_TRUE(pipeline_controller_.IsStable());
    PipelineStatusCallback suspend_cb;
    EXPECT_CALL(*pipeline_, OnSuspend(_)).WillOnce(MoveArg<0>(&suspend_cb));
    pipeline_controller_.Suspend();
    Mock::VerifyAndClear(pipeline_);
    EXPECT_CALL(*pipeline_, IsSuspended())
        .Times(AnyNumber())
        .WillOnce(Return(true));
    EXPECT_TRUE(pipeline_controller_.IsSuspended());
    EXPECT_FALSE(pipeline_controller_.IsStable());
    EXPECT_FALSE(pipeline_controller_.IsPipelineSuspended());
    return suspend_cb;
  }

  PipelineStatusCallback ResumePipeline() {
    EXPECT_TRUE(pipeline_controller_.IsPipelineSuspended());
    PipelineStatusCallback resume_cb;
    EXPECT_CALL(*pipeline_, OnResume(_, _))
        .WillOnce(
            DoAll(SaveArg<0>(&last_resume_time_), MoveArg<1>(&resume_cb)));
    EXPECT_CALL(*pipeline_, GetMediaTime())
        .WillRepeatedly(Return(base::TimeDelta()));
    pipeline_controller_.Resume();
    Mock::VerifyAndClear(pipeline_);
    EXPECT_CALL(*pipeline_, IsSuspended())
        .Times(AnyNumber())
        .WillOnce(Return(false));
    EXPECT_FALSE(pipeline_controller_.IsSuspended());
    EXPECT_FALSE(pipeline_controller_.IsStable());
    EXPECT_FALSE(pipeline_controller_.IsPipelineSuspended());
    return resume_cb;
  }

  void Complete(PipelineStatusCallback cb) {
    std::move(cb).Run(PIPELINE_OK);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void OnStarted(PipelineStatus status) { was_started_ = true; }

  void OnSeeked(bool time_updated) {
    was_seeked_ = true;
    last_seeked_time_updated_ = time_updated;
  }

  void OnSuspended() { was_suspended_ = true; }
  void OnBeforeResume() { was_resuming_ = true; }
  void OnResumed() { was_resumed_ = true; }

  // Pipeline::Client overrides
  void OnError(PipelineStatus status) override { NOTREACHED_IN_MIGRATION(); }
  void OnFallback(PipelineStatus status) override { NOTREACHED_IN_MIGRATION(); }
  void OnEnded() override {}
  void OnMetadata(const PipelineMetadata& metadata) override {}
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) override {}
  void OnDurationChange() override {}
  void OnWaiting(WaitingReason reason) override {}
  void OnVideoNaturalSizeChange(const gfx::Size& size) override {}
  void OnAudioConfigChange(const AudioDecoderConfig& config) override {}
  void OnVideoConfigChange(const VideoDecoderConfig& config) override {}
  void OnVideoOpacityChange(bool opaque) override {}
  void OnVideoFrameRateChange(std::optional<int>) override {}
  void OnVideoAverageKeyframeDistanceUpdate() override {}
  void OnAudioPipelineInfoChange(const AudioPipelineInfo& info) override {}
  void OnVideoPipelineInfoChange(const VideoPipelineInfo& info) override {}

  // testing::Test overrides
  void TearDown() override {
    pipeline_ = nullptr;
    testing::Test::TearDown();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  NiceMock<MockDemuxer> demuxer_;
  // Owned by PipelineController.
  raw_ptr<StrictMock<MockPipeline>> pipeline_;
  PipelineController pipeline_controller_;

  bool was_started_ = false;
  bool was_seeked_ = false;
  bool last_seeked_time_updated_ = false;
  bool was_suspended_ = false;
  bool was_resuming_ = false;
  bool was_resumed_ = false;
  base::TimeDelta last_resume_time_;
};

TEST_F(PipelineControllerTest, Startup) {
  PipelineStatusCallback start_cb = StartPipeline();
  EXPECT_FALSE(was_started_);
  EXPECT_FALSE(was_seeked_);

  Complete(std::move(start_cb));
  EXPECT_TRUE(was_started_);
  EXPECT_TRUE(was_seeked_);
  EXPECT_FALSE(last_seeked_time_updated_);
  EXPECT_FALSE(was_suspended_);
  EXPECT_TRUE(pipeline_controller_.IsStable());
}

TEST_F(PipelineControllerTest, StartSuspendedSeekAndResume) {
  EXPECT_FALSE(pipeline_controller_.IsStable());
  PipelineStatusCallback start_cb;
  EXPECT_CALL(*pipeline_, OnStart(_, _, _, _)).WillOnce(MoveArg<3>(&start_cb));
  pipeline_controller_.Start(Pipeline::StartType::kSuspendAfterMetadata,
                             &demuxer_, this, false, true);
  Mock::VerifyAndClear(pipeline_);

  // Initiate a seek before the pipeline completes suspended startup.
  base::TimeDelta seek_time = base::Seconds(5);
  EXPECT_CALL(demuxer_, StartWaitingForSeek(seek_time));
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_started_);
  EXPECT_FALSE(was_seeked_);

  PipelineStatusCallback resume_cb;
  EXPECT_CALL(*pipeline_, OnResume(_, _))
      .WillOnce(DoAll(MoveArg<1>(&resume_cb)));
  EXPECT_CALL(*pipeline_, GetMediaTime())
      .WillRepeatedly(Return(base::TimeDelta()));

  EXPECT_CALL(*pipeline_, IsSuspended()).WillRepeatedly(Return(true));
  EXPECT_FALSE(pipeline_controller_.IsStable());
  Complete(std::move(start_cb));

  EXPECT_TRUE(was_started_);
  EXPECT_FALSE(was_seeked_);
  EXPECT_FALSE(pipeline_controller_.IsStable());
  EXPECT_FALSE(pipeline_controller_.IsPipelineSuspended());
  EXPECT_FALSE(pipeline_controller_.IsSuspended());
  Mock::VerifyAndClear(pipeline_);

  EXPECT_CALL(*pipeline_, IsSuspended()).WillRepeatedly(Return(false));
  Complete(std::move(resume_cb));
  EXPECT_TRUE(was_seeked_);
  was_seeked_ = false;

  EXPECT_TRUE(pipeline_controller_.IsStable());
  EXPECT_FALSE(pipeline_controller_.IsPipelineSuspended());
  EXPECT_FALSE(pipeline_controller_.IsSuspended());
  Mock::VerifyAndClear(pipeline_);
}

TEST_F(PipelineControllerTest, StartSuspendedAndResume) {
  EXPECT_FALSE(pipeline_controller_.IsStable());
  PipelineStatusCallback start_cb;
  EXPECT_CALL(*pipeline_, OnStart(_, _, _, _)).WillOnce(MoveArg<3>(&start_cb));
  pipeline_controller_.Start(Pipeline::StartType::kSuspendAfterMetadata,
                             &demuxer_, this, false, true);
  Mock::VerifyAndClear(pipeline_);
  EXPECT_CALL(*pipeline_, IsSuspended()).WillRepeatedly(Return(true));
  EXPECT_FALSE(pipeline_controller_.IsStable());
  EXPECT_FALSE(was_started_);
  Complete(std::move(start_cb));
  EXPECT_TRUE(was_started_);
  EXPECT_TRUE(was_seeked_);
  was_started_ = false;
  was_seeked_ = false;

  EXPECT_FALSE(pipeline_controller_.IsStable());
  EXPECT_TRUE(pipeline_controller_.IsPipelineSuspended());
  EXPECT_TRUE(pipeline_controller_.IsSuspended());
  Mock::VerifyAndClear(pipeline_);

  EXPECT_CALL(*pipeline_, IsSuspended()).WillRepeatedly(Return(false));
  PipelineStatusCallback resume_cb = ResumePipeline();
  EXPECT_TRUE(was_resuming_);
  EXPECT_FALSE(was_resumed_);

  Complete(std::move(resume_cb));
  EXPECT_TRUE(was_resumed_);
  EXPECT_TRUE(pipeline_controller_.IsStable());

  // |was_started_|, |was_seeked_| should not be affected by Suspend()/Resume()
  // at all.
  EXPECT_FALSE(was_started_);
  EXPECT_FALSE(was_seeked_);
}

TEST_F(PipelineControllerTest, SuspendResume) {
  Complete(StartPipeline());
  EXPECT_TRUE(was_started_);
  EXPECT_TRUE(was_seeked_);
  was_started_ = false;
  was_seeked_ = false;

  Complete(SuspendPipeline());
  EXPECT_TRUE(was_suspended_);
  EXPECT_FALSE(pipeline_controller_.IsStable());

  PipelineStatusCallback resume_cb = ResumePipeline();
  EXPECT_TRUE(was_resuming_);
  EXPECT_FALSE(was_resumed_);

  Complete(std::move(resume_cb));
  EXPECT_TRUE(was_resumed_);
  EXPECT_TRUE(pipeline_controller_.IsStable());

  // |was_started_|, |was_seeked_| should not be affected by Suspend()/Resume()
  // at all.
  EXPECT_FALSE(was_started_);
  EXPECT_FALSE(was_seeked_);
}

TEST_F(PipelineControllerTest, Seek) {
  // Normal seeking should not result in a cancel.
  EXPECT_CALL(demuxer_, CancelPendingSeek(_)).Times(0);

  Complete(StartPipeline());
  was_seeked_ = false;

  base::TimeDelta seek_time = base::Seconds(5);
  EXPECT_CALL(demuxer_, StartWaitingForSeek(seek_time));
  PipelineStatusCallback seek_cb = SeekPipeline(seek_time);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_seeked_);

  Complete(std::move(seek_cb));
  EXPECT_TRUE(was_seeked_);
  EXPECT_TRUE(pipeline_controller_.IsStable());
}

// Makes sure OnDecoderStateLost() triggers a seek to the current media time.
TEST_F(PipelineControllerTest, DecoderStateLost) {
  Complete(StartPipeline());

  constexpr auto kCurrentMediaTime = base::Seconds(7);
  EXPECT_CALL(*pipeline_, GetMediaTime())
      .WillRepeatedly(Return(kCurrentMediaTime));

  EXPECT_CALL(demuxer_, StartWaitingForSeek(kCurrentMediaTime));
  EXPECT_CALL(*pipeline_, OnSeek(kCurrentMediaTime, _));

  pipeline_controller_.OnDecoderStateLost();
  base::RunLoop().RunUntilIdle();
}

// Makes sure OnDecoderStateLost() does not trigger a seek during pending seek.
TEST_F(PipelineControllerTest, DecoderStateLost_DuringPendingSeek) {
  Complete(StartPipeline());

  // Create a pending seek.
  base::TimeDelta kSeekTime = base::Seconds(5);
  EXPECT_CALL(demuxer_, StartWaitingForSeek(kSeekTime));
  PipelineStatusCallback seek_cb = SeekPipeline(kSeekTime);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&demuxer_);

  // OnDecoderStateLost() should not trigger another seek.
  EXPECT_CALL(*pipeline_, GetMediaTime()).Times(0);
  pipeline_controller_.OnDecoderStateLost();
  base::RunLoop().RunUntilIdle();

  Complete(std::move(seek_cb));
}

TEST_F(PipelineControllerTest, SuspendResumeTime) {
  Complete(StartPipeline());
  Complete(SuspendPipeline());

  base::TimeDelta seek_time = base::Seconds(5);
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();

  Complete(ResumePipeline());
  EXPECT_EQ(seek_time, last_resume_time_);
}

TEST_F(PipelineControllerTest, SuspendResumeTime_WithStreamingData) {
  Complete(StartPipeline_WithStreamingData());
  Complete(SuspendPipeline());

  base::TimeDelta seek_time = base::Seconds(5);
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();

  Complete(ResumePipeline());
  EXPECT_EQ(base::TimeDelta(), last_resume_time_);
}

TEST_F(PipelineControllerTest, SeekAborted) {
  Complete(StartPipeline());

  // Create a first pending seek.
  base::TimeDelta seek_time_1 = base::Seconds(5);
  EXPECT_CALL(demuxer_, StartWaitingForSeek(seek_time_1));
  PipelineStatusCallback seek_cb_1 = SeekPipeline(seek_time_1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&demuxer_);

  // Create a second seek; the first should be aborted.
  base::TimeDelta seek_time_2 = base::Seconds(10);
  EXPECT_CALL(demuxer_, CancelPendingSeek(seek_time_2));
  pipeline_controller_.Seek(seek_time_2, true);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&demuxer_);

  // When the first seek is completed (or aborted) the second should be issued.
  EXPECT_CALL(demuxer_, StartWaitingForSeek(seek_time_2));
  EXPECT_CALL(*pipeline_, OnSeek(seek_time_2, _));
  Complete(std::move(seek_cb_1));
}

TEST_F(PipelineControllerTest, PendingSuspend) {
  Complete(StartPipeline());

  base::TimeDelta seek_time = base::Seconds(5);
  PipelineStatusCallback seek_cb = SeekPipeline(seek_time);
  base::RunLoop().RunUntilIdle();

  // While the seek is ongoing, request a suspend.
  // It will be a mock failure if pipeline_.Suspend() is called.
  pipeline_controller_.Suspend();
  base::RunLoop().RunUntilIdle();

  // Expect the suspend to trigger when the seek is completed.
  EXPECT_CALL(*pipeline_, OnSuspend(_));
  Complete(std::move(seek_cb));
}

TEST_F(PipelineControllerTest, SeekMergesWithResume) {
  Complete(StartPipeline());
  Complete(SuspendPipeline());

  // Pipeline startup always completes with a seek.
  EXPECT_TRUE(was_seeked_);
  was_seeked_ = false;

  // Request a seek while suspended.
  // It will be a mock failure if pipeline_.Seek() is called.
  base::TimeDelta seek_time = base::Seconds(5);
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_seeked_);

  // Resume and verify the resume time includes the seek.
  Complete(ResumePipeline());
  EXPECT_EQ(seek_time, last_resume_time_);
  EXPECT_TRUE(last_seeked_time_updated_);
}

TEST_F(PipelineControllerTest, SeekMergesWithSeek) {
  Complete(StartPipeline());

  base::TimeDelta seek_time_1 = base::Seconds(5);
  PipelineStatusCallback seek_cb_1 = SeekPipeline(seek_time_1);
  base::RunLoop().RunUntilIdle();

  // Request another seek while the first is ongoing.
  base::TimeDelta seek_time_2 = base::Seconds(10);
  pipeline_controller_.Seek(seek_time_2, true);
  base::RunLoop().RunUntilIdle();

  // Request a third seek. (It should replace the second.)
  base::TimeDelta seek_time_3 = base::Seconds(15);
  pipeline_controller_.Seek(seek_time_3, true);
  base::RunLoop().RunUntilIdle();

  // Expect the third seek to trigger when the first seek completes.
  EXPECT_CALL(*pipeline_, OnSeek(seek_time_3, _));
  Complete(std::move(seek_cb_1));
}

TEST_F(PipelineControllerTest, SeekToSeekTimeElided) {
  Complete(StartPipeline());

  base::TimeDelta seek_time = base::Seconds(5);
  PipelineStatusCallback seek_cb_1 = SeekPipeline(seek_time);
  base::RunLoop().RunUntilIdle();

  // Request a seek to the same time again.
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();

  // Complete the first seek.
  // It would be a mock error if the second seek was dispatched here.
  Complete(std::move(seek_cb_1));
  EXPECT_TRUE(pipeline_controller_.IsStable());
}

TEST_F(PipelineControllerTest, SeekToSeekTimeNotElided) {
  Complete(StartPipeline_WithDynamicData());

  base::TimeDelta seek_time = base::Seconds(5);
  PipelineStatusCallback seek_cb_1 = SeekPipeline(seek_time);
  base::RunLoop().RunUntilIdle();

  // Request a seek to the same time again.
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();

  // Expect the second seek to trigger when the first seek completes.
  EXPECT_CALL(*pipeline_, OnSeek(seek_time, _));
  Complete(std::move(seek_cb_1));
}

TEST_F(PipelineControllerTest, VideoTrackChangeWhileSuspending) {
  Complete(StartPipeline());
  EXPECT_CALL(*pipeline_, OnSuspend(_));
  EXPECT_CALL(*pipeline_, OnSelectedVideoTrackChanged(_, _)).Times(0);
  pipeline_controller_.Suspend();
  pipeline_controller_.OnSelectedVideoTrackChanged({});
}

TEST_F(PipelineControllerTest, AudioTrackChangeWhileSuspending) {
  Complete(StartPipeline());
  EXPECT_CALL(*pipeline_, OnSuspend(_));
  EXPECT_CALL(*pipeline_, OnEnabledAudioTracksChanged(_, _)).Times(0);
  pipeline_controller_.Suspend();
  pipeline_controller_.OnEnabledAudioTracksChanged({});
}

TEST_F(PipelineControllerTest, AudioTrackChangeDuringVideoTrackChange) {
  Complete(StartPipeline());

  EXPECT_CALL(*pipeline_, OnSelectedVideoTrackChanged(_, _));
  pipeline_controller_.OnSelectedVideoTrackChanged({});
  pipeline_controller_.OnEnabledAudioTracksChanged({});
  EXPECT_CALL(*pipeline_, OnEnabledAudioTracksChanged(_, _));

  pipeline_controller_.FireOnTrackChangeCompleteForTesting(
      PipelineController::State::PLAYING);

  pipeline_controller_.FireOnTrackChangeCompleteForTesting(
      PipelineController::State::PLAYING);
}

TEST_F(PipelineControllerTest, SuspendDuringVideoTrackChange) {
  Complete(StartPipeline());
  EXPECT_CALL(*pipeline_, OnSelectedVideoTrackChanged(_, _));
  was_resumed_ = false;
  pipeline_controller_.OnSelectedVideoTrackChanged({});
  pipeline_controller_.Suspend();

  base::RunLoop loop;
  EXPECT_CALL(*pipeline_, OnSuspend(_))
      .WillOnce(RunOnceClosure(loop.QuitClosure()));

  pipeline_controller_.FireOnTrackChangeCompleteForTesting(
      PipelineController::State::PLAYING);

  loop.Run();
  EXPECT_FALSE(was_resumed_);
}

TEST_F(PipelineControllerTest, SuspendDuringAudioTrackChange) {
  Complete(StartPipeline());
  EXPECT_CALL(*pipeline_, OnEnabledAudioTracksChanged(_, _));
  was_resumed_ = false;

  pipeline_controller_.OnEnabledAudioTracksChanged({});
  pipeline_controller_.Suspend();

  base::RunLoop loop;
  EXPECT_CALL(*pipeline_, OnSuspend(_))
      .WillOnce(RunOnceClosure(loop.QuitClosure()));

  pipeline_controller_.FireOnTrackChangeCompleteForTesting(
      PipelineController::State::PLAYING);

  loop.Run();
  EXPECT_FALSE(was_resumed_);
}

TEST_F(PipelineControllerTest, ResumePlaybackDuringSwitchingTracksState) {
  Complete(StartPipeline());
  Complete(SuspendPipeline());
  EXPECT_CALL(*pipeline_, OnSelectedVideoTrackChanged(_, _)).Times(1);
  EXPECT_CALL(*pipeline_, GetMediaTime()).Times(1);
  EXPECT_CALL(*pipeline_, OnResume(_, _)).Times(1);

  pipeline_controller_.OnSelectedVideoTrackChanged({});
  pipeline_controller_.Resume();
  pipeline_controller_.FireOnTrackChangeCompleteForTesting(
      PipelineController::State::SUSPENDED);
}

TEST_F(PipelineControllerTest, PreservesPitch) {
  Complete(StartPipeline());
  EXPECT_CALL(*pipeline_, SetPreservesPitch(false));
  pipeline_controller_.SetPreservesPitch(false);

  EXPECT_CALL(*pipeline_, SetPreservesPitch(true));
  pipeline_controller_.SetPreservesPitch(true);
}

}  // namespace media

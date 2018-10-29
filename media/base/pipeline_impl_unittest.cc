// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

#include <stddef.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "media/base/fake_text_track_stream.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/media_log.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/text_renderer.h"
#include "media/base/text_track_config.h"
#include "media/base/time_delta_interpolator.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::CreateFunctor;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace media {

ACTION_P(SetDemuxerProperties, duration) {
  arg0->SetDuration(duration);
}

ACTION_P(Stop, pipeline) {
  pipeline->Stop();
}

ACTION_P(PostStop, pipeline) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&Pipeline::Stop, base::Unretained(pipeline)));
}

ACTION_P2(SetError, renderer_client, status) {
  (*renderer_client)->OnError(status);
}

ACTION_P2(SetBufferingState, renderer_client, buffering_state) {
  (*renderer_client)->OnBufferingStateChange(buffering_state);
}

ACTION_TEMPLATE(PostCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::get<k>(args), p0));
}

// TODO(scherkus): even though some filters are initialized on separate
// threads these test aren't flaky... why?  It's because filters' Initialize()
// is executed on |message_loop_| and the mock filters instantly call
// InitializationComplete(), which keeps the pipeline humming along.  If
// either filters don't call InitializationComplete() immediately or filter
// initialization is moved to a separate thread this test will become flaky.
class PipelineImplTest : public ::testing::Test {
 public:
  // Used for setting expectations on pipeline callbacks.  Using a StrictMock
  // also lets us test for missing callbacks.
  class CallbackHelper : public MockPipelineClient {
   public:
    CallbackHelper() = default;
    virtual ~CallbackHelper() = default;

    MOCK_METHOD1(OnStart, void(PipelineStatus));
    MOCK_METHOD1(OnSeek, void(PipelineStatus));
    MOCK_METHOD1(OnSuspend, void(PipelineStatus));
    MOCK_METHOD1(OnResume, void(PipelineStatus));

   private:
    DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
  };

  PipelineImplTest()
      : pipeline_(new PipelineImpl(message_loop_.task_runner(),
                                   message_loop_.task_runner(),
                                   &media_log_)),
        demuxer_(new StrictMock<MockDemuxer>()),
        demuxer_host_(nullptr),
        scoped_renderer_(new StrictMock<MockRenderer>()),
        renderer_(scoped_renderer_.get()),
        renderer_client_(nullptr) {
    // SetDemuxerExpectations() adds overriding expectations for expected
    // non-NULL streams.
    std::vector<DemuxerStream*> empty;
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(empty));

    EXPECT_CALL(*demuxer_, GetTimelineOffset())
        .WillRepeatedly(Return(base::Time()));

    EXPECT_CALL(*renderer_, GetMediaTime())
        .WillRepeatedly(Return(base::TimeDelta()));

    EXPECT_CALL(*demuxer_, GetStartTime()).WillRepeatedly(Return(start_time_));
  }

  ~PipelineImplTest() override {
    if (pipeline_->IsRunning()) {
      ExpectDemuxerStop();

      pipeline_->Stop();
    }

    pipeline_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void OnDemuxerError() { demuxer_host_->OnDemuxerError(PIPELINE_ERROR_ABORT); }

 protected:
  // Sets up expectations to allow the demuxer to initialize.
  typedef std::vector<MockDemuxerStream*> MockDemuxerStreamVector;
  void SetDemuxerExpectations(MockDemuxerStreamVector* streams,
                              const base::TimeDelta& duration) {
    EXPECT_CALL(callbacks_, OnDurationChange());
    EXPECT_CALL(*demuxer_, Initialize(_, _))
        .WillOnce(DoAll(SaveArg<0>(&demuxer_host_),
                        SetDemuxerProperties(duration),
                        PostCallback<1>(PIPELINE_OK)));

    // Configure the demuxer to return the streams.
    std::vector<DemuxerStream*> mock_streams;
    for (size_t i = 0; i < streams->size(); ++i) {
      DemuxerStream* stream = (*streams)[i];
      mock_streams.push_back(stream);
    }
    EXPECT_CALL(*demuxer_, GetAllStreams())
        .WillRepeatedly(Return(mock_streams));
  }

  void SetDemuxerExpectations(MockDemuxerStreamVector* streams) {
    // Initialize with a default non-zero duration.
    SetDemuxerExpectations(streams, base::TimeDelta::FromSeconds(10));
  }

  std::unique_ptr<StrictMock<MockDemuxerStream>> CreateStream(
      DemuxerStream::Type type) {
    std::unique_ptr<StrictMock<MockDemuxerStream>> stream(
        new StrictMock<MockDemuxerStream>(type));
    return stream;
  }

  // Sets up expectations to allow the video renderer to initialize.
  void SetRendererExpectations() {
    EXPECT_CALL(*renderer_, Initialize(_, _, _))
        .WillOnce(
            DoAll(SaveArg<1>(&renderer_client_), PostCallback<2>(PIPELINE_OK)));
  }

  void StartPipeline(
      Pipeline::StartType start_type = Pipeline::StartType::kNormal) {
    EXPECT_CALL(callbacks_, OnWaitingForDecryptionKey()).Times(0);
    pipeline_->Start(
        start_type, demuxer_.get(), std::move(scoped_renderer_), &callbacks_,
        base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)));
  }

  // Suspension status of the pipeline post Start().
  enum class PostStartStatus {
    kNormal,
    kSuspended,
  };

  // Sets up expectations on the callback and initializes the pipeline. Called
  // after tests have set expectations any filters they wish to use.
  void StartPipelineAndExpect(
      PipelineStatus start_status,
      Pipeline::StartType start_type = Pipeline::StartType::kNormal,
      PostStartStatus post_start_status = PostStartStatus::kNormal) {
    EXPECT_CALL(callbacks_, OnStart(start_status));

    if (start_status == PIPELINE_OK) {
      EXPECT_CALL(callbacks_, OnMetadata(_)).WillOnce(SaveArg<0>(&metadata_));
      if (post_start_status == PostStartStatus::kNormal) {
        EXPECT_CALL(*renderer_, SetPlaybackRate(0.0));
        EXPECT_CALL(*renderer_, SetVolume(1.0f));
        EXPECT_CALL(*renderer_, StartPlayingFrom(start_time_))
            .WillOnce(
                SetBufferingState(&renderer_client_, BUFFERING_HAVE_ENOUGH));
        EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
      }
    }

    StartPipeline(start_type);
    base::RunLoop().RunUntilIdle();

    if (start_status == PIPELINE_OK)
      EXPECT_TRUE(pipeline_->IsRunning());

    if (post_start_status != PostStartStatus::kNormal)
      EXPECT_TRUE(pipeline_->IsSuspended());
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
  }

  void CreateVideoStream() {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
    video_stream_->set_video_decoder_config(video_decoder_config_);
  }

  MockDemuxerStream* audio_stream() { return audio_stream_.get(); }

  MockDemuxerStream* video_stream() { return video_stream_.get(); }

  void ExpectSeek(const base::TimeDelta& seek_time, bool underflowed) {
    EXPECT_CALL(*demuxer_, AbortPendingReads());
    EXPECT_CALL(*demuxer_, Seek(seek_time, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    EXPECT_CALL(*renderer_, Flush(_))
        .WillOnce(
            DoAll(SetBufferingState(&renderer_client_, BUFFERING_HAVE_NOTHING),
                  RunClosure<0>()));
    EXPECT_CALL(*renderer_, SetPlaybackRate(_));
    EXPECT_CALL(*renderer_, SetVolume(_));
    EXPECT_CALL(*renderer_, StartPlayingFrom(seek_time))
        .WillOnce(SetBufferingState(&renderer_client_, BUFFERING_HAVE_ENOUGH));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));

    // We expect a successful seek callback followed by a buffering update.
    EXPECT_CALL(callbacks_, OnSeek(PIPELINE_OK));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  }

  void DoSeek(const base::TimeDelta& seek_time) {
    pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                          base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectSuspend() {
    EXPECT_CALL(*demuxer_, AbortPendingReads());
    EXPECT_CALL(*renderer_, SetPlaybackRate(0));
    EXPECT_CALL(callbacks_, OnSuspend(PIPELINE_OK));
  }

  void DoSuspend() {
    pipeline_->Suspend(
        base::Bind(&CallbackHelper::OnSuspend, base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
    CreateRenderer();
  }

  void CreateRenderer() {
    // |renderer_| has been deleted, replace it.
    scoped_renderer_.reset(new StrictMock<MockRenderer>());
    renderer_ = scoped_renderer_.get();
  }

  void ExpectResume(const base::TimeDelta& seek_time) {
    SetRendererExpectations();
    EXPECT_CALL(*demuxer_, Seek(seek_time, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));
    EXPECT_CALL(*renderer_, SetPlaybackRate(_));
    EXPECT_CALL(*renderer_, SetVolume(_));
    EXPECT_CALL(*renderer_, StartPlayingFrom(seek_time))
        .WillOnce(SetBufferingState(&renderer_client_, BUFFERING_HAVE_ENOUGH));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
    EXPECT_CALL(callbacks_, OnResume(PIPELINE_OK));
  }

  void DoResume(const base::TimeDelta& seek_time) {
    pipeline_->Resume(
        std::move(scoped_renderer_), seek_time,
        base::Bind(&CallbackHelper::OnResume, base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectDemuxerStop() {
    if (demuxer_)
      EXPECT_CALL(*demuxer_, Stop());
  }

  void RunBufferedTimeRangesTest(const base::TimeDelta duration) {
    EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());
    EXPECT_FALSE(pipeline_->DidLoadingProgress());

    Ranges<base::TimeDelta> ranges;
    ranges.Add(base::TimeDelta(), duration);
    demuxer_host_->OnBufferedTimeRangesChanged(ranges);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(pipeline_->DidLoadingProgress());
    EXPECT_FALSE(pipeline_->DidLoadingProgress());
    EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
    EXPECT_EQ(base::TimeDelta(), pipeline_->GetBufferedTimeRanges().start(0));
    EXPECT_EQ(duration, pipeline_->GetBufferedTimeRanges().end(0));
  }

  // Fixture members.
  StrictMock<CallbackHelper> callbacks_;
  base::SimpleTestTickClock test_tick_clock_;
  base::MessageLoop message_loop_;
  MediaLog media_log_;
  std::unique_ptr<PipelineImpl> pipeline_;

  std::unique_ptr<StrictMock<MockDemuxer>> demuxer_;
  DemuxerHost* demuxer_host_;
  std::unique_ptr<StrictMock<MockRenderer>> scoped_renderer_;
  StrictMock<MockRenderer>* renderer_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  RendererClient* renderer_client_;
  VideoDecoderConfig video_decoder_config_;
  PipelineMetadata metadata_;
  base::TimeDelta start_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PipelineImplTest);
};

// Test that playback controls methods no-op when the pipeline hasn't been
// started.
TEST_F(PipelineImplTest, NotStarted) {
  const base::TimeDelta kZero;

  EXPECT_FALSE(pipeline_->IsRunning());

  // Setting should still work.
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  pipeline_->SetPlaybackRate(-1.0);
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  pipeline_->SetPlaybackRate(1.0);
  EXPECT_EQ(1.0f, pipeline_->GetPlaybackRate());

  // Setting should still work.
  EXPECT_EQ(1.0f, pipeline_->GetVolume());
  pipeline_->SetVolume(-1.0f);
  EXPECT_EQ(1.0f, pipeline_->GetVolume());
  pipeline_->SetVolume(0.0f);
  EXPECT_EQ(0.0f, pipeline_->GetVolume());

  EXPECT_TRUE(kZero == pipeline_->GetMediaTime());
  EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_TRUE(kZero == pipeline_->GetMediaDuration());
}

TEST_F(PipelineImplTest, NeverInitializes) {
  // Don't execute the callback passed into Initialize().
  EXPECT_CALL(*demuxer_, Initialize(_, _));

  // This test hangs during initialization by never calling
  // InitializationComplete().  StrictMock<> will ensure that the callback is
  // never executed.
  StartPipeline();
  base::RunLoop().RunUntilIdle();

  // Because our callback will get executed when the test tears down, we'll
  // verify that nothing has been called, then set our expectation for the call
  // made during tear down.
  Mock::VerifyAndClear(&callbacks_);
}

TEST_F(PipelineImplTest, StopWithoutStart) {
  pipeline_->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, StartThenStopImmediately) {
  EXPECT_CALL(*demuxer_, Initialize(_, _))
      .WillOnce(PostCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*demuxer_, Stop());
  EXPECT_CALL(callbacks_, OnMetadata(_));

  EXPECT_CALL(callbacks_, OnStart(_));
  StartPipeline();
  base::RunLoop().RunUntilIdle();

  pipeline_->Stop();
}

TEST_F(PipelineImplTest, StartSuspendedAndResumeAudioOnly) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  StartPipelineAndExpect(PIPELINE_OK,
                         Pipeline::StartType::kSuspendAfterMetadataForAudioOnly,
                         PostStartStatus::kSuspended);
  ASSERT_TRUE(pipeline_->IsSuspended());

  CreateRenderer();
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectResume(expected);
  DoResume(expected);
}

TEST_F(PipelineImplTest, StartSuspendedAndResumeAudioVideo) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  StartPipelineAndExpect(PIPELINE_OK,
                         Pipeline::StartType::kSuspendAfterMetadata,
                         PostStartStatus::kSuspended);
  ASSERT_TRUE(pipeline_->IsSuspended());

  CreateRenderer();
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectResume(expected);
  DoResume(expected);
}

TEST_F(PipelineImplTest, StartSuspendedFailsOnVideoWithAudioOnlyExpectation) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK,
                         Pipeline::StartType::kSuspendAfterMetadataForAudioOnly,
                         PostStartStatus::kNormal);
  ASSERT_FALSE(pipeline_->IsSuspended());
}

TEST_F(PipelineImplTest, DemuxerErrorDuringStop) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*demuxer_, Stop())
      .WillOnce(InvokeWithoutArgs(this, &PipelineImplTest::OnDemuxerError));
  pipeline_->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, NoStreams) {
  EXPECT_CALL(*demuxer_, Initialize(_, _))
      .WillOnce(PostCallback<1>(PIPELINE_OK));
  EXPECT_CALL(callbacks_, OnMetadata(_));

  StartPipelineAndExpect(PIPELINE_ERROR_COULD_NOT_RENDER);
}

TEST_F(PipelineImplTest, AudioStream) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_FALSE(metadata_.has_video);
}

TEST_F(PipelineImplTest, VideoStream) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_FALSE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);
}

TEST_F(PipelineImplTest, AudioVideoStream) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);
}

TEST_F(PipelineImplTest, Seek) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  SetRendererExpectations();

  // Initialize then seek!
  StartPipelineAndExpect(PIPELINE_OK);

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectSeek(expected, false);
  DoSeek(expected);
}

TEST_F(PipelineImplTest, SeekAfterError) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  SetRendererExpectations();

  // Initialize then seek!
  StartPipelineAndExpect(PIPELINE_OK);

  // Pipeline::Client is supposed to call Pipeline::Stop() after errors.
  EXPECT_CALL(callbacks_, OnError(_)).WillOnce(Stop(pipeline_.get()));
  EXPECT_CALL(*demuxer_, Stop());
  OnDemuxerError();
  base::RunLoop().RunUntilIdle();

  pipeline_->Seek(
      base::TimeDelta::FromMilliseconds(100),
      base::Bind(&CallbackHelper::OnSeek, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, SuspendResume) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);

  // Inject some fake memory usage to verify its cleared after suspend.
  PipelineStatistics stats;
  stats.audio_memory_usage = 12345;
  stats.video_memory_usage = 67890;
  renderer_client_->OnStatisticsUpdate(stats);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(stats.audio_memory_usage,
            pipeline_->GetStatistics().audio_memory_usage);
  EXPECT_EQ(stats.video_memory_usage,
            pipeline_->GetStatistics().video_memory_usage);

  ExpectSuspend();
  DoSuspend();

  EXPECT_EQ(0, pipeline_->GetStatistics().audio_memory_usage);
  EXPECT_EQ(0, pipeline_->GetStatistics().video_memory_usage);

  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectResume(expected);
  DoResume(expected);
}

TEST_F(PipelineImplTest, SetVolume) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*renderer_, SetVolume(expected));

  // Initialize then set volume!
  StartPipelineAndExpect(PIPELINE_OK);
  pipeline_->SetVolume(expected);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, Properties) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  SetDemuxerExpectations(&streams, kDuration);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetMediaDuration().ToInternalValue());
  EXPECT_FALSE(pipeline_->DidLoadingProgress());
}

TEST_F(PipelineImplTest, GetBufferedTimeRanges) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  SetDemuxerExpectations(&streams, kDuration);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  RunBufferedTimeRangesTest(kDuration / 8);

  base::TimeDelta kSeekTime = kDuration / 2;
  ExpectSeek(kSeekTime, false);
  DoSeek(kSeekTime);

  EXPECT_FALSE(pipeline_->DidLoadingProgress());
}

TEST_F(PipelineImplTest, BufferedTimeRangesCanChangeAfterStop) {
  EXPECT_CALL(*demuxer_, Initialize(_, _))
      .WillOnce(
          DoAll(SaveArg<0>(&demuxer_host_), PostCallback<1>(PIPELINE_OK)));
  EXPECT_CALL(*demuxer_, Stop());
  EXPECT_CALL(callbacks_, OnMetadata(_));
  EXPECT_CALL(callbacks_, OnStart(_));
  StartPipeline();
  base::RunLoop().RunUntilIdle();

  pipeline_->Stop();
  RunBufferedTimeRangesTest(base::TimeDelta::FromSeconds(5));
}

TEST_F(PipelineImplTest, EndedCallback) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);


  // The ended callback shouldn't run until all renderers have ended.
  EXPECT_CALL(callbacks_, OnEnded());
  renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, ErrorDuringSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  double playback_rate = 1.0;
  EXPECT_CALL(*renderer_, SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  base::RunLoop().RunUntilIdle();

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  EXPECT_CALL(*renderer_, Flush(_))
      .WillOnce(
          DoAll(SetBufferingState(&renderer_client_, BUFFERING_HAVE_NOTHING),
                RunClosure<0>()));

  EXPECT_CALL(*demuxer_, AbortPendingReads());
  EXPECT_CALL(*demuxer_, Seek(seek_time, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
  EXPECT_CALL(*demuxer_, Stop());

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ))
      .WillOnce(Stop(pipeline_.get()));
  base::RunLoop().RunUntilIdle();
}

// Invoked function OnError. This asserts that the pipeline does not enqueue
// non-teardown related tasks while tearing down.
static void TestNoCallsAfterError(PipelineImpl* pipeline,
                                  base::MessageLoop* message_loop,
                                  PipelineStatus /* status */) {
  CHECK(pipeline);
  CHECK(message_loop);

  // When we get to this stage, the message loop should be empty.
  EXPECT_TRUE(message_loop->IsIdleForTesting());

  // Make calls on pipeline after error has occurred.
  pipeline->SetPlaybackRate(0.5);
  pipeline->SetVolume(0.5f);

  // No additional tasks should be queued as a result of these calls.
  EXPECT_TRUE(message_loop->IsIdleForTesting());
}

TEST_F(PipelineImplTest, NoMessageDuringTearDownFromError) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Trigger additional requests on the pipeline during tear down from error.
  base::Callback<void(PipelineStatus)> cb =
      base::Bind(&TestNoCallsAfterError, pipeline_.get(), &message_loop_);
  ON_CALL(callbacks_, OnError(_)).WillByDefault(Invoke(CreateFunctor(cb)));

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  // Seek() isn't called as the demuxer errors out first.
  EXPECT_CALL(*renderer_, Flush(_))
      .WillOnce(
          DoAll(SetBufferingState(&renderer_client_, BUFFERING_HAVE_NOTHING),
                RunClosure<0>()));
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));

  EXPECT_CALL(*demuxer_, AbortPendingReads());
  EXPECT_CALL(*demuxer_, Seek(seek_time, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
  EXPECT_CALL(*demuxer_, Stop());

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ))
      .WillOnce(Stop(pipeline_.get()));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, DestroyAfterStop) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  ExpectDemuxerStop();
  pipeline_->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, Underflow) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Simulate underflow.
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
  base::RunLoop().RunUntilIdle();

  // Seek while underflowed.
  base::TimeDelta expected = base::TimeDelta::FromSeconds(5);
  ExpectSeek(expected, true);
  DoSeek(expected);
}

TEST_F(PipelineImplTest, PositiveStartTime) {
  start_time_ = base::TimeDelta::FromSeconds(1);
  EXPECT_CALL(*demuxer_, GetStartTime()).WillRepeatedly(Return(start_time_));
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);
  ExpectDemuxerStop();
  pipeline_->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, GetMediaTime) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Pipeline should report the same media time returned by the renderer.
  base::TimeDelta kMediaTime = base::TimeDelta::FromSeconds(2);
  EXPECT_CALL(*renderer_, GetMediaTime()).WillRepeatedly(Return(kMediaTime));
  EXPECT_EQ(kMediaTime, pipeline_->GetMediaTime());

  // Media time should not go backwards even if the renderer returns an
  // errorneous value. PipelineImpl should clamp it to last reported value.
  EXPECT_CALL(*renderer_, GetMediaTime())
      .WillRepeatedly(Return(base::TimeDelta::FromSeconds(1)));
  EXPECT_EQ(kMediaTime, pipeline_->GetMediaTime());
}

// Seeking posts a task from main thread to media thread to seek the renderer,
// resetting its internal clock. Calling GetMediaTime() should be safe even
// when the renderer has not performed the seek (simulated by its continuing
// to return the pre-seek time). Verifies fix for http://crbug.com/675556
TEST_F(PipelineImplTest, GetMediaTimeAfterSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Pipeline should report the same media time returned by the renderer.
  base::TimeDelta kMediaTime = base::TimeDelta::FromSeconds(2);
  EXPECT_CALL(*renderer_, GetMediaTime()).WillRepeatedly(Return(kMediaTime));
  EXPECT_EQ(kMediaTime, pipeline_->GetMediaTime());

  // Seek backward 1 second. Do not run RunLoop to ensure renderer is not yet
  // notified of the seek (via media thread).
  base::TimeDelta kSeekTime = kMediaTime - base::TimeDelta::FromSeconds(1);
  ExpectSeek(kSeekTime, false);
  pipeline_->Seek(kSeekTime, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));

  // Verify pipeline returns the seek time in spite of renderer returning the
  // stale media time.
  EXPECT_EQ(kSeekTime, pipeline_->GetMediaTime());
  EXPECT_EQ(kMediaTime, renderer_->GetMediaTime());

  // Allow seek task to post to the renderer.
  base::RunLoop().RunUntilIdle();

  // With seek completed, pipeline should again return the renderer's media time
  // (as long as media time is moving forward).
  EXPECT_EQ(kMediaTime, pipeline_->GetMediaTime());
}

// This test makes sure that, after receiving an error, stopping and starting
// the pipeline clears all internal error state, and allows errors to be
// propagated again.
TEST_F(PipelineImplTest, RendererErrorsReset) {
  // Basic setup
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Trigger two errors. The second error will be ignored.
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ)).Times(1);
  renderer_client_->OnError(PIPELINE_ERROR_READ);
  renderer_client_->OnError(PIPELINE_ERROR_READ);

  base::RunLoop().RunUntilIdle();

  // Stopping the demuxer should clear internal state.
  EXPECT_CALL(*demuxer_, Stop());
  pipeline_->Stop();

  base::RunLoop().RunUntilIdle();

  CreateRenderer();
  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // New errors should propagate and not be ignored.
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ)).Times(1);
  renderer_client_->OnError(PIPELINE_ERROR_READ);

  base::RunLoop().RunUntilIdle();
}

class PipelineTeardownTest : public PipelineImplTest {
 public:
  enum TeardownState {
    kInitDemuxer,
    kInitRenderer,
    kFlushing,
    kSeeking,
    kPlaying,
    kSuspending,
    kSuspended,
    kResuming,
  };

  enum StopOrError {
    kStop,
    kError,
    kErrorAndStop,
  };

  PipelineTeardownTest() = default;
  ~PipelineTeardownTest() override = default;

  void RunTest(TeardownState state, StopOrError stop_or_error) {
    switch (state) {
      case kInitDemuxer:
      case kInitRenderer:
        DoInitialize(state, stop_or_error);
        break;

      case kFlushing:
      case kSeeking:
        DoInitialize(state, stop_or_error);
        DoSeek(state, stop_or_error);
        break;

      case kPlaying:
        DoInitialize(state, stop_or_error);
        DoStopOrError(stop_or_error, true);
        break;

      case kSuspending:
      case kSuspended:
      case kResuming:
        DoInitialize(state, stop_or_error);
        DoSuspend(state, stop_or_error);
        break;
    }
  }

 private:
  // TODO(scherkus): We do radically different things whether teardown is
  // invoked via stop vs error. The teardown path should be the same,
  // see http://crbug.com/110228
  void DoInitialize(TeardownState state, StopOrError stop_or_error) {
    SetInitializeExpectations(state, stop_or_error);
    StartPipeline();
    base::RunLoop().RunUntilIdle();
  }

  void SetInitializeExpectations(TeardownState state,
                                 StopOrError stop_or_error) {
    if (state == kInitDemuxer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, Initialize(_, _))
            .WillOnce(
                DoAll(PostStop(pipeline_.get()), PostCallback<1>(PIPELINE_OK)));
        // Note: OnStart callback is not called after pipeline is stopped.
      } else {
        EXPECT_CALL(*demuxer_, Initialize(_, _))
            .WillOnce(PostCallback<1>(DEMUXER_ERROR_COULD_NOT_OPEN));
        EXPECT_CALL(callbacks_, OnStart(DEMUXER_ERROR_COULD_NOT_OPEN))
            .WillOnce(Stop(pipeline_.get()));
      }

      EXPECT_CALL(*demuxer_, Stop());
      return;
    }

    CreateAudioStream();
    CreateVideoStream();
    MockDemuxerStreamVector streams;
    streams.push_back(audio_stream());
    streams.push_back(video_stream());
    SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));

    if (state == kInitRenderer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*renderer_, Initialize(_, _, _))
            .WillOnce(
                DoAll(PostStop(pipeline_.get()), PostCallback<2>(PIPELINE_OK)));
        // Note: OnStart is not callback after pipeline is stopped.
      } else {
        EXPECT_CALL(*renderer_, Initialize(_, _, _))
            .WillOnce(PostCallback<2>(PIPELINE_ERROR_INITIALIZATION_FAILED));
        EXPECT_CALL(callbacks_, OnStart(PIPELINE_ERROR_INITIALIZATION_FAILED))
            .WillOnce(Stop(pipeline_.get()));
      }

      EXPECT_CALL(callbacks_, OnMetadata(_));
      EXPECT_CALL(*demuxer_, Stop());
      return;
    }

    EXPECT_CALL(*renderer_, Initialize(_, _, _))
        .WillOnce(
            DoAll(SaveArg<1>(&renderer_client_), PostCallback<2>(PIPELINE_OK)));

    // If we get here it's a successful initialization.
    EXPECT_CALL(callbacks_, OnStart(PIPELINE_OK));
    EXPECT_CALL(callbacks_, OnMetadata(_));

    EXPECT_CALL(*renderer_, SetPlaybackRate(0.0));
    EXPECT_CALL(*renderer_, SetVolume(1.0f));
    EXPECT_CALL(*renderer_, StartPlayingFrom(base::TimeDelta()))
        .WillOnce(SetBufferingState(&renderer_client_, BUFFERING_HAVE_ENOUGH));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  }

  void DoSeek(TeardownState state, StopOrError stop_or_error) {
    SetSeekExpectations(state, stop_or_error);

    EXPECT_CALL(*demuxer_, AbortPendingReads());
    EXPECT_CALL(*demuxer_, Stop());

    pipeline_->Seek(
        base::TimeDelta::FromSeconds(10),
        base::Bind(&CallbackHelper::OnSeek, base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void SetSeekExpectations(TeardownState state, StopOrError stop_or_error) {
    if (state == kFlushing) {
      EXPECT_CALL(*demuxer_, Seek(_, _));
      if (stop_or_error == kStop) {
        EXPECT_CALL(*renderer_, Flush(_))
            .WillOnce(DoAll(
                SetBufferingState(&renderer_client_, BUFFERING_HAVE_NOTHING),
                Stop(pipeline_.get()), RunClosure<0>()));
        // Note: OnBufferingStateChange or OnSeek callbacks are not called
        // after pipeline is stopped.
      } else {
        EXPECT_CALL(*renderer_, Flush(_))
            .WillOnce(DoAll(
                SetBufferingState(&renderer_client_, BUFFERING_HAVE_NOTHING),
                SetError(&renderer_client_, PIPELINE_ERROR_READ),
                RunClosure<0>()));
        EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
        EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ))
            .WillOnce(Stop(pipeline_.get()));
      }
      return;
    }

    EXPECT_CALL(*renderer_, Flush(_))
        .WillOnce(
            DoAll(SetBufferingState(&renderer_client_, BUFFERING_HAVE_NOTHING),
                  RunClosure<0>()));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));

    if (state == kSeeking) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, Seek(_, _))
            .WillOnce(
                DoAll(PostStop(pipeline_.get()), RunCallback<1>(PIPELINE_OK)));
        // Note: OnSeek callback is not called after pipeline is stopped.
      } else {
        EXPECT_CALL(*demuxer_, Seek(_, _))
            .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
        EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ))
            .WillOnce(Stop(pipeline_.get()));
      }
      return;
    }

    NOTREACHED() << "State not supported: " << state;
  }

  void DoSuspend(TeardownState state, StopOrError stop_or_error) {
    SetSuspendExpectations(state, stop_or_error);

    if (state == kResuming) {
      EXPECT_CALL(*demuxer_, Stop());
    }

    PipelineImplTest::DoSuspend();

    if (state == kResuming) {
      PipelineImplTest::DoResume(base::TimeDelta());
      return;
    }

    // kSuspended, kSuspending never throw errors, since Resume() is always able
    // to restore the pipeline to a pristine state.
    DoStopOrError(stop_or_error, false);
  }

  void SetSuspendExpectations(TeardownState state, StopOrError stop_or_error) {
    EXPECT_CALL(*renderer_, SetPlaybackRate(0));
    EXPECT_CALL(*demuxer_, AbortPendingReads());
    EXPECT_CALL(callbacks_, OnSuspend(PIPELINE_OK));
    if (state == kResuming) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, Seek(_, _))
            .WillOnce(
                DoAll(PostStop(pipeline_.get()), RunCallback<1>(PIPELINE_OK)));
        // Note: OnResume callback is not called after pipeline is stopped.
      } else {
        EXPECT_CALL(*demuxer_, Seek(_, _))
            .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
        EXPECT_CALL(callbacks_, OnResume(PIPELINE_ERROR_READ))
            .WillOnce(Stop(pipeline_.get()));
      }
    } else if (state != kSuspended && state != kSuspending) {
      NOTREACHED() << "State not supported: " << state;
    }
  }

  void DoStopOrError(StopOrError stop_or_error, bool expect_errors) {
    switch (stop_or_error) {
      case kStop:
        EXPECT_CALL(*demuxer_, Stop());
        pipeline_->Stop();
        break;

      case kError:
        if (expect_errors) {
          EXPECT_CALL(*demuxer_, Stop());
          EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ))
              .WillOnce(Stop(pipeline_.get()));
        }
        renderer_client_->OnError(PIPELINE_ERROR_READ);
        break;

      case kErrorAndStop:
        EXPECT_CALL(*demuxer_, Stop());
        if (expect_errors)
          EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ));
        renderer_client_->OnError(PIPELINE_ERROR_READ);
        base::RunLoop().RunUntilIdle();
        pipeline_->Stop();
        break;
    }

    base::RunLoop().RunUntilIdle();
  }

  DISALLOW_COPY_AND_ASSIGN(PipelineTeardownTest);
};

#define INSTANTIATE_TEARDOWN_TEST(stop_or_error, state)   \
  TEST_F(PipelineTeardownTest, stop_or_error##_##state) { \
    RunTest(k##state, k##stop_or_error);                  \
  }

INSTANTIATE_TEARDOWN_TEST(Stop, InitDemuxer);
INSTANTIATE_TEARDOWN_TEST(Stop, InitRenderer);
INSTANTIATE_TEARDOWN_TEST(Stop, Flushing);
INSTANTIATE_TEARDOWN_TEST(Stop, Seeking);
INSTANTIATE_TEARDOWN_TEST(Stop, Playing);
INSTANTIATE_TEARDOWN_TEST(Stop, Suspending);
INSTANTIATE_TEARDOWN_TEST(Stop, Suspended);
INSTANTIATE_TEARDOWN_TEST(Stop, Resuming);

INSTANTIATE_TEARDOWN_TEST(Error, InitDemuxer);
INSTANTIATE_TEARDOWN_TEST(Error, InitRenderer);
INSTANTIATE_TEARDOWN_TEST(Error, Flushing);
INSTANTIATE_TEARDOWN_TEST(Error, Seeking);
INSTANTIATE_TEARDOWN_TEST(Error, Playing);
INSTANTIATE_TEARDOWN_TEST(Error, Suspending);
INSTANTIATE_TEARDOWN_TEST(Error, Suspended);
INSTANTIATE_TEARDOWN_TEST(Error, Resuming);

INSTANTIATE_TEARDOWN_TEST(ErrorAndStop, Playing);
INSTANTIATE_TEARDOWN_TEST(ErrorAndStop, Suspended);

}  // namespace media

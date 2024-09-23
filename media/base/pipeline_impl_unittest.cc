// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/time_delta_interpolator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NiceMock;
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Pipeline::Stop, base::Unretained(pipeline)));
}

ACTION_P2(SetError, renderer_client, status) {
  (*renderer_client)->OnError(status);
}

ACTION_P3(SetBufferingState, renderer_client, buffering_state, reason) {
  (*renderer_client)->OnBufferingStateChange(buffering_state, reason);
}

ACTION_TEMPLATE(PostCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(std::get<k>(args)), p0));
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

    CallbackHelper(const CallbackHelper&) = delete;
    CallbackHelper& operator=(const CallbackHelper&) = delete;

    virtual ~CallbackHelper() = default;

    MOCK_METHOD1(OnStart, void(PipelineStatus));
    MOCK_METHOD1(OnSeek, void(PipelineStatus));
    MOCK_METHOD1(OnSuspend, void(PipelineStatus));
    MOCK_METHOD1(OnResume, void(PipelineStatus));
    MOCK_METHOD1(OnCdmAttached, void(bool));
  };

  PipelineImplTest()
      : demuxer_(std::make_unique<StrictMock<MockDemuxer>>()),
        scoped_renderer_(std::make_unique<StrictMock<MockRenderer>>()),
        renderer_(scoped_renderer_->AsWeakPtr()) {
    pipeline_ = std::make_unique<PipelineImpl>(
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(&PipelineImplTest::TakeRenderer,
                            base::Unretained(this)),
        &media_log_);

    // SetDemuxerExpectations() adds overriding expectations for expected
    // non-NULL streams.
    std::vector<DemuxerStream*> empty;
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(empty));

    EXPECT_CALL(*demuxer_, GetTimelineOffset())
        .WillRepeatedly(Return(base::Time()));

    EXPECT_CALL(*renderer_, GetMediaTime())
        .WillRepeatedly(Return(base::TimeDelta()));

    EXPECT_CALL(*demuxer_, GetStartTime()).WillRepeatedly(Return(start_time_));

    EXPECT_CALL(*renderer_, SetPreservesPitch(true)).Times(AnyNumber());
  }

  PipelineImplTest(const PipelineImplTest&) = delete;
  PipelineImplTest& operator=(const PipelineImplTest&) = delete;

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
  void SetDemuxerExpectations(base::TimeDelta duration) {
    EXPECT_CALL(callbacks_, OnDurationChange());
    EXPECT_CALL(*demuxer_, OnInitialize(_, _))
        .WillOnce(DoAll(SaveArg<0>(&demuxer_host_),
                        SetDemuxerProperties(duration),
                        PostCallback<1>(PIPELINE_OK)));
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  void SetDemuxerExpectations() {
    // Initialize with a default non-zero duration.
    SetDemuxerExpectations(base::Seconds(10));
  }

  std::unique_ptr<StrictMock<MockDemuxerStream>> CreateStream(
      DemuxerStream::Type type) {
    return std::make_unique<StrictMock<MockDemuxerStream>>(type);
  }

  // Sets up expectations to allow the renderer to initialize.
  void ExpectRendererInitialization() {
    EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
        .WillOnce(
            DoAll(SaveArg<1>(&renderer_client_), PostCallback<2>(PIPELINE_OK)));
  }

  void StartPipeline(
      Pipeline::StartType start_type = Pipeline::StartType::kNormal) {
    EXPECT_CALL(callbacks_, OnWaiting(_)).Times(0);
    pipeline_->Start(start_type, demuxer_.get(), &callbacks_,
                     base::BindOnce(&CallbackHelper::OnStart,
                                    base::Unretained(&callbacks_)));
  }

  void SetRendererPostStartExpectations() {
    EXPECT_CALL(*renderer_, SetPlaybackRate(0.0));
    EXPECT_CALL(*renderer_, SetVolume(1.0f));
    EXPECT_CALL(*renderer_,
                SetWasPlayedWithUserActivationAndHighMediaEngagement(false));
    EXPECT_CALL(*renderer_, StartPlayingFrom(start_time_))
        .WillOnce(SetBufferingState(&renderer_client_, BUFFERING_HAVE_ENOUGH,
                                    BUFFERING_CHANGE_REASON_UNKNOWN));
    EXPECT_CALL(callbacks_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN));
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
    EXPECT_CALL(callbacks_, OnStart(SameStatusCode(start_status)));

    if (start_status == PIPELINE_OK) {
      EXPECT_CALL(callbacks_, OnMetadata(_)).WillOnce(SaveArg<0>(&metadata_));

      if (start_type == Pipeline::StartType::kNormal)
        ExpectRendererInitialization();

      if (post_start_status == PostStartStatus::kNormal)
        SetRendererPostStartExpectations();
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
    streams_.push_back(audio_stream_.get());
  }

  void CreateVideoStream(bool is_encrypted = false) {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
    video_stream_->set_video_decoder_config(
        is_encrypted ? TestVideoConfig::NormalEncrypted()
                     : TestVideoConfig::Normal());
    streams_.push_back(video_stream_.get());
  }

  void CreateAudioAndVideoStream() {
    CreateAudioStream();
    CreateVideoStream();
  }

  void CreateEncryptedVideoStream() { CreateVideoStream(true); }

  void SetCdmAndExpect(bool expected_result) {
    EXPECT_CALL(*renderer_, OnSetCdm(_, _)).WillOnce(RunOnceCallback<1>(true));
    EXPECT_CALL(callbacks_, OnCdmAttached(expected_result));
    pipeline_->SetCdm(&cdm_context_,
                      base::BindOnce(&CallbackHelper::OnCdmAttached,
                                     base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectSeek(const base::TimeDelta& seek_time, bool underflowed) {
    EXPECT_CALL(*demuxer_, AbortPendingReads());
    EXPECT_CALL(*demuxer_, OnSeek(seek_time, _))
        .WillOnce(RunOnceCallback<1>(PIPELINE_OK));

    EXPECT_CALL(*renderer_, OnFlush(_)).WillOnce(RunOnceClosure<0>());
    EXPECT_CALL(*renderer_, SetPlaybackRate(_));
    EXPECT_CALL(*renderer_, StartPlayingFrom(seek_time))
        .WillOnce(SetBufferingState(&renderer_client_, BUFFERING_HAVE_ENOUGH,
                                    BUFFERING_CHANGE_REASON_UNKNOWN));

    // We expect a successful seek callback followed by a buffering update.
    EXPECT_CALL(callbacks_, OnSeek(HasStatusCode(PIPELINE_OK)));
    EXPECT_CALL(callbacks_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN));
  }

  void DoSeek(const base::TimeDelta& seek_time) {
    pipeline_->Seek(seek_time, base::BindOnce(&CallbackHelper::OnSeek,
                                              base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectSuspend() {
    EXPECT_CALL(*demuxer_, AbortPendingReads());
    EXPECT_CALL(*renderer_, SetPlaybackRate(0));
    EXPECT_CALL(callbacks_, OnSuspend(HasStatusCode(PIPELINE_OK)));
  }

  void DoSuspend() {
    pipeline_->Suspend(base::BindOnce(&CallbackHelper::OnSuspend,
                                      base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
    ResetRenderer();
  }

  std::unique_ptr<Renderer> TakeRenderer(
      std::optional<RendererType> /* renderer_type */) {
    return std::move(scoped_renderer_);
  }

  void ResetRenderer() {
    // |renderer_| has been deleted, replace it.
    scoped_renderer_ = std::make_unique<StrictMock<MockRenderer>>();
    renderer_ = scoped_renderer_->AsWeakPtr();
    EXPECT_CALL(*renderer_, SetPreservesPitch(_)).Times(AnyNumber());
  }

  void ExpectResume(const base::TimeDelta& seek_time) {
    ExpectRendererInitialization();
    EXPECT_CALL(*demuxer_, OnSeek(seek_time, _))
        .WillOnce(RunOnceCallback<1>(PIPELINE_OK));
    EXPECT_CALL(*renderer_, SetPlaybackRate(_));
    EXPECT_CALL(*renderer_, SetVolume(_));
    EXPECT_CALL(*renderer_,
                SetWasPlayedWithUserActivationAndHighMediaEngagement(false));
    EXPECT_CALL(*renderer_, StartPlayingFrom(seek_time))
        .WillOnce(SetBufferingState(&renderer_client_, BUFFERING_HAVE_ENOUGH,
                                    BUFFERING_CHANGE_REASON_UNKNOWN));
    EXPECT_CALL(callbacks_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN));
    EXPECT_CALL(callbacks_, OnResume(HasStatusCode(PIPELINE_OK)));
  }

  void DoResume(const base::TimeDelta& seek_time) {
    pipeline_->Resume(seek_time, base::BindOnce(&CallbackHelper::OnResume,
                                                base::Unretained(&callbacks_)));
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
  base::test::SingleThreadTaskEnvironment task_environment_;
  NullMediaLog media_log_;
  std::unique_ptr<PipelineImpl> pipeline_;
  NiceMock<MockCdmContext> cdm_context_;

  std::unique_ptr<StrictMock<MockDemuxer>> demuxer_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION DemuxerHost* demuxer_host_ = nullptr;
  std::unique_ptr<StrictMock<MockRenderer>> scoped_renderer_;
  base::WeakPtr<MockRenderer> renderer_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  std::vector<DemuxerStream*> streams_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION RendererClient* renderer_client_ = nullptr;
  VideoDecoderConfig video_decoder_config_;
  PipelineMetadata metadata_;
  base::TimeDelta start_time_;
};

// Test that playback controls methods can be set even before the pipeline is
// started.
TEST_F(PipelineImplTest, ControlMethods) {
  const base::TimeDelta kZero;

  EXPECT_FALSE(pipeline_->IsRunning());

  // Initial value.
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  // Invalid values cannot be set.
  pipeline_->SetPlaybackRate(-1.0);
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  // Valid settings should work.
  pipeline_->SetPlaybackRate(1.0);
  EXPECT_EQ(1.0f, pipeline_->GetPlaybackRate());

  // Initial value.
  EXPECT_EQ(1.0f, pipeline_->GetVolume());
  // Invalid values cannot be set.
  pipeline_->SetVolume(-1.0f);
  EXPECT_EQ(1.0f, pipeline_->GetVolume());
  // Valid settings should work.
  pipeline_->SetVolume(0.0f);
  EXPECT_EQ(0.0f, pipeline_->GetVolume());

  EXPECT_TRUE(kZero == pipeline_->GetMediaTime());
  EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_TRUE(kZero == pipeline_->GetMediaDuration());
}

TEST_F(PipelineImplTest, NeverInitializes) {
  // Don't execute the callback passed into Initialize().
  EXPECT_CALL(*demuxer_, OnInitialize(_, _));

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
  EXPECT_CALL(*demuxer_, OnInitialize(_, _))
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
  SetDemuxerExpectations(base::Seconds(3000));

  StartPipelineAndExpect(PIPELINE_OK,
                         Pipeline::StartType::kSuspendAfterMetadataForAudioOnly,
                         PostStartStatus::kSuspended);
  ASSERT_TRUE(pipeline_->IsSuspended());

  ResetRenderer();
  base::TimeDelta expected = base::Seconds(2000);
  ExpectResume(expected);
  DoResume(expected);
}

TEST_F(PipelineImplTest, StartSuspendedAndResumeAudioVideo) {
  CreateAudioAndVideoStream();
  SetDemuxerExpectations(base::Seconds(3000));

  StartPipelineAndExpect(PIPELINE_OK,
                         Pipeline::StartType::kSuspendAfterMetadata,
                         PostStartStatus::kSuspended);
  ASSERT_TRUE(pipeline_->IsSuspended());

  ResetRenderer();
  base::TimeDelta expected = base::Seconds(2000);
  ExpectResume(expected);
  DoResume(expected);
}

TEST_F(PipelineImplTest, StartSuspendedFailsOnVideoWithAudioOnlyExpectation) {
  CreateAudioAndVideoStream();
  SetDemuxerExpectations(base::Seconds(3000));

  // StartType kSuspendAfterMetadataForAudioOnly only applies to AudioOnly.
  // Since this playback has video, renderer will be initialized and the
  // pipeline is not suspended.
  ExpectRendererInitialization();
  StartPipelineAndExpect(PIPELINE_OK,
                         Pipeline::StartType::kSuspendAfterMetadataForAudioOnly,
                         PostStartStatus::kNormal);
  ASSERT_FALSE(pipeline_->IsSuspended());
}

TEST_F(PipelineImplTest, DemuxerErrorDuringStop) {
  CreateAudioStream();
  SetDemuxerExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*demuxer_, Stop())
      .WillOnce(InvokeWithoutArgs(this, &PipelineImplTest::OnDemuxerError));
  pipeline_->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, NoStreams) {
  EXPECT_CALL(*demuxer_, OnInitialize(_, _))
      .WillOnce(PostCallback<1>(PIPELINE_OK));
  EXPECT_CALL(callbacks_, OnMetadata(_));

  StartPipelineAndExpect(PIPELINE_ERROR_COULD_NOT_RENDER);
}

TEST_F(PipelineImplTest, AudioStream) {
  CreateAudioStream();
  SetDemuxerExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_FALSE(metadata_.has_video);
}

TEST_F(PipelineImplTest, VideoStream) {
  CreateVideoStream();
  SetDemuxerExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_FALSE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);
}

TEST_F(PipelineImplTest, AudioVideoStream) {
  CreateAudioAndVideoStream();
  SetDemuxerExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);
}

TEST_F(PipelineImplTest, EncryptedStream_SetCdmBeforeStart) {
  CreateEncryptedVideoStream();
  SetDemuxerExpectations();

  SetCdmAndExpect(true);
  StartPipelineAndExpect(PIPELINE_OK);
}

TEST_F(PipelineImplTest, EncryptedStream_SetCdmAfterStart) {
  CreateEncryptedVideoStream();
  SetDemuxerExpectations();

  // Demuxer initialization and metadata reporting don't wait for CDM.
  EXPECT_CALL(callbacks_, OnMetadata(_)).WillOnce(SaveArg<0>(&metadata_));

  base::RunLoop run_loop;
  EXPECT_CALL(callbacks_, OnWaiting(_))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  pipeline_->Start(
      Pipeline::StartType::kNormal, demuxer_.get(), &callbacks_,
      base::BindOnce(&CallbackHelper::OnStart, base::Unretained(&callbacks_)));
  run_loop.Run();

  ExpectRendererInitialization();
  EXPECT_CALL(callbacks_, OnStart(HasStatusCode(PIPELINE_OK)));
  SetRendererPostStartExpectations();
  SetCdmAndExpect(true);
}

TEST_F(PipelineImplTest, Seek) {
  CreateAudioAndVideoStream();
  SetDemuxerExpectations(base::Seconds(3000));

  // Initialize then seek!
  StartPipelineAndExpect(PIPELINE_OK);

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::Seconds(2000);
  ExpectSeek(expected, false);
  DoSeek(expected);
}

TEST_F(PipelineImplTest, SeekAfterError) {
  CreateAudioStream();
  SetDemuxerExpectations(base::Seconds(3000));

  // Initialize then seek!
  StartPipelineAndExpect(PIPELINE_OK);

  // Pipeline::Client is supposed to call Pipeline::Stop() after errors.
  EXPECT_CALL(callbacks_, OnError(_)).WillOnce(Stop(pipeline_.get()));
  EXPECT_CALL(*demuxer_, Stop());
  OnDemuxerError();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(callbacks_, OnSeek(HasStatusCode(PIPELINE_ERROR_INVALID_STATE)));
  pipeline_->Seek(
      base::Milliseconds(100),
      base::BindOnce(&CallbackHelper::OnSeek, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, SuspendResume) {
  CreateAudioAndVideoStream();
  SetDemuxerExpectations(base::Seconds(3000));

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

  // Make sure the preserves pitch flag is preserved between after resuming.
  EXPECT_CALL(*renderer_, SetPreservesPitch(false)).Times(1);
  pipeline_->SetPreservesPitch(false);

  ExpectSuspend();
  DoSuspend();

  EXPECT_EQ(0, pipeline_->GetStatistics().audio_memory_usage);
  EXPECT_EQ(0, pipeline_->GetStatistics().video_memory_usage);

  base::TimeDelta expected = base::Seconds(2000);
  ExpectResume(expected);
  EXPECT_CALL(*renderer_, SetPreservesPitch(false)).Times(1);

  DoResume(expected);
}

TEST_F(PipelineImplTest, SetVolume) {
  CreateAudioStream();
  SetDemuxerExpectations();

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*renderer_, SetVolume(expected));

  // Initialize then set volume!
  StartPipelineAndExpect(PIPELINE_OK);
  pipeline_->SetVolume(expected);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, SetVolumeDuringStartup) {
  CreateAudioStream();
  SetDemuxerExpectations();

  // The audio renderer should receive two calls to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*renderer_, SetVolume(expected)).Times(2);
  EXPECT_CALL(*renderer_,
              SetWasPlayedWithUserActivationAndHighMediaEngagement(false));
  EXPECT_CALL(callbacks_, OnStart(HasStatusCode(PIPELINE_OK)));
  EXPECT_CALL(callbacks_, OnMetadata(_))
      .WillOnce(RunOnceClosure(base::BindOnce(&PipelineImpl::SetVolume,
                                              base::Unretained(pipeline_.get()),
                                              expected)));
  ExpectRendererInitialization();
  EXPECT_CALL(*renderer_, SetPlaybackRate(0.0));
  EXPECT_CALL(*renderer_, StartPlayingFrom(start_time_))
      .WillOnce(SetBufferingState(&renderer_client_, BUFFERING_HAVE_ENOUGH,
                                  BUFFERING_CHANGE_REASON_UNKNOWN));
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  StartPipeline();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, SetPreservesPitch) {
  CreateAudioStream();
  SetDemuxerExpectations();

  // The audio renderer preserve pitch by default.
  EXPECT_CALL(*renderer_, SetPreservesPitch(true));
  StartPipelineAndExpect(PIPELINE_OK);
  base::RunLoop().RunUntilIdle();

  // Changes to the preservesPitch flag should be propagated.
  EXPECT_CALL(*renderer_, SetPreservesPitch(false));
  pipeline_->SetPreservesPitch(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, Properties) {
  CreateVideoStream();
  const auto kDuration = base::Seconds(100);
  SetDemuxerExpectations(kDuration);

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetMediaDuration().ToInternalValue());
  EXPECT_FALSE(pipeline_->DidLoadingProgress());
}

TEST_F(PipelineImplTest, GetBufferedTimeRanges) {
  CreateVideoStream();
  const auto kDuration = base::Seconds(100);
  SetDemuxerExpectations(kDuration);

  StartPipelineAndExpect(PIPELINE_OK);
  RunBufferedTimeRangesTest(kDuration / 8);

  base::TimeDelta kSeekTime = kDuration / 2;
  ExpectSeek(kSeekTime, false);
  DoSeek(kSeekTime);

  EXPECT_FALSE(pipeline_->DidLoadingProgress());
}

TEST_F(PipelineImplTest, BufferedTimeRangesCanChangeAfterStop) {
  EXPECT_CALL(*demuxer_, OnInitialize(_, _))
      .WillOnce(
          DoAll(SaveArg<0>(&demuxer_host_), PostCallback<1>(PIPELINE_OK)));
  EXPECT_CALL(*demuxer_, Stop());
  EXPECT_CALL(callbacks_, OnMetadata(_));
  EXPECT_CALL(callbacks_, OnStart(_));
  StartPipeline();
  base::RunLoop().RunUntilIdle();

  pipeline_->Stop();
  RunBufferedTimeRangesTest(base::Seconds(5));
}

TEST_F(PipelineImplTest, OnStatisticsUpdate) {
  CreateAudioAndVideoStream();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  PipelineStatistics stats;
  stats.audio_pipeline_info.decoder_type = AudioDecoderType::kMojo;
  stats.audio_pipeline_info.is_platform_decoder = false;
  EXPECT_CALL(callbacks_, OnAudioPipelineInfoChange(_));
  renderer_client_->OnStatisticsUpdate(stats);
  base::RunLoop().RunUntilIdle();

  // VideoPipelineInfo changed and we expect OnVideoPipelineInfoChange() to be
  // called.
  stats.video_pipeline_info.decoder_type = VideoDecoderType::kMojo;
  stats.video_pipeline_info.is_platform_decoder = true;
  EXPECT_CALL(callbacks_, OnVideoPipelineInfoChange(_));
  renderer_client_->OnStatisticsUpdate(stats);
  base::RunLoop().RunUntilIdle();

  // OnStatisticsUpdate() with the same |stats| should not cause new
  // PipelineClient calls.
  renderer_client_->OnStatisticsUpdate(stats);
  base::RunLoop().RunUntilIdle();

  // AudioPipelineInfo changed and we expect OnAudioPipelineInfoChange() to be
  // called.
  stats.audio_pipeline_info.is_platform_decoder = true;
  EXPECT_CALL(callbacks_, OnAudioPipelineInfoChange(_));
  renderer_client_->OnStatisticsUpdate(stats);
  base::RunLoop().RunUntilIdle();

  // Both info changed.
  stats.audio_pipeline_info.decoder_type = AudioDecoderType::kFFmpeg;
  stats.video_pipeline_info.has_decrypting_demuxer_stream = true;
  EXPECT_CALL(callbacks_, OnAudioPipelineInfoChange(_));
  EXPECT_CALL(callbacks_, OnVideoPipelineInfoChange(_));
  renderer_client_->OnStatisticsUpdate(stats);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, EndedCallback) {
  CreateAudioAndVideoStream();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // The ended callback shouldn't run until all renderers have ended.
  EXPECT_CALL(callbacks_, OnEnded());
  renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, DemuxerErrorDuringSeek) {
  CreateAudioStream();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  double playback_rate = 1.0;
  EXPECT_CALL(*renderer_, SetPlaybackRate(playback_rate));
  EXPECT_CALL(*demuxer_, SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  base::RunLoop().RunUntilIdle();

  base::TimeDelta seek_time = base::Seconds(5);

  EXPECT_CALL(*renderer_, OnFlush(_)).WillOnce(RunOnceClosure<0>());

  EXPECT_CALL(*demuxer_, AbortPendingReads());
  EXPECT_CALL(*demuxer_, OnSeek(seek_time, _))
      .WillOnce(RunOnceCallback<1>(PIPELINE_ERROR_READ));
  EXPECT_CALL(*demuxer_, Stop());

  pipeline_->Seek(seek_time, base::BindOnce(&CallbackHelper::OnSeek,
                                            base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(HasStatusCode(PIPELINE_ERROR_READ)))
      .WillOnce(Stop(pipeline_.get()));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, PipelineErrorDuringSeek) {
  CreateAudioStream();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  base::TimeDelta seek_time = base::Seconds(5);

  // Set expectations for seek.
  EXPECT_CALL(*renderer_, OnFlush(_)).WillOnce(RunOnceClosure<0>());
  EXPECT_CALL(*renderer_, SetPlaybackRate(_));
  EXPECT_CALL(*renderer_, StartPlayingFrom(seek_time));
  EXPECT_CALL(*demuxer_, AbortPendingReads());
  EXPECT_CALL(*demuxer_, OnSeek(seek_time, _))
      .WillOnce(RunOnceCallback<1>(PIPELINE_OK));
  EXPECT_CALL(callbacks_, OnSeek(HasStatusCode(PIPELINE_ERROR_DECODE)));

  // Triggers pipeline error during pending seek.
  pipeline_->Seek(seek_time, base::BindOnce(&CallbackHelper::OnSeek,
                                            base::Unretained(&callbacks_)));
  renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, PipelineErrorDuringSuspend) {
  CreateAudioAndVideoStream();
  SetDemuxerExpectations(base::Seconds(3000));
  StartPipelineAndExpect(PIPELINE_OK);

  // Set expectations for suspend.
  EXPECT_CALL(*demuxer_, AbortPendingReads());
  EXPECT_CALL(*renderer_, SetPlaybackRate(0));
  EXPECT_CALL(callbacks_, OnSuspend(HasStatusCode(PIPELINE_ERROR_DECODE)));

  // Triggers pipeline error during pending suspend. The order matters for
  // reproducing crbug.com/1250636. Otherwise OnError() is ignored if already in
  // kSuspending state.
  renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  pipeline_->Suspend(base::BindOnce(&CallbackHelper::OnSuspend,
                                    base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, DestroyAfterStop) {
  CreateAudioStream();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  ExpectDemuxerStop();
  pipeline_->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, Underflow) {
  CreateAudioAndVideoStream();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Simulate underflow.
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                           BUFFERING_CHANGE_REASON_UNKNOWN);
  base::RunLoop().RunUntilIdle();

  // Seek while underflowed.
  base::TimeDelta expected = base::Seconds(5);
  ExpectSeek(expected, true);
  DoSeek(expected);
}

TEST_F(PipelineImplTest, PositiveStartTime) {
  start_time_ = base::Seconds(1);
  EXPECT_CALL(*demuxer_, GetStartTime()).WillRepeatedly(Return(start_time_));
  CreateAudioStream();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);
  ExpectDemuxerStop();
  pipeline_->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PipelineImplTest, GetMediaTime) {
  CreateAudioStream();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Pipeline should report the same media time returned by the renderer.
  base::TimeDelta kMediaTime = base::Seconds(2);
  EXPECT_CALL(*renderer_, GetMediaTime()).WillRepeatedly(Return(kMediaTime));
  EXPECT_EQ(kMediaTime, pipeline_->GetMediaTime());

  // Media time should not go backwards even if the renderer returns an
  // erroneous value. PipelineImpl should clamp it to last reported value.
  EXPECT_CALL(*renderer_, GetMediaTime())
      .WillRepeatedly(Return(base::Seconds(1)));
  EXPECT_EQ(kMediaTime, pipeline_->GetMediaTime());
}

// Seeking posts a task from main thread to media thread to seek the renderer,
// resetting its internal clock. Calling GetMediaTime() should be safe even
// when the renderer has not performed the seek (simulated by its continuing
// to return the pre-seek time). Verifies fix for http://crbug.com/675556
TEST_F(PipelineImplTest, GetMediaTimeAfterSeek) {
  CreateAudioStream();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Pipeline should report the same media time returned by the renderer.
  base::TimeDelta kMediaTime = base::Seconds(2);
  EXPECT_CALL(*renderer_, GetMediaTime()).WillRepeatedly(Return(kMediaTime));
  EXPECT_EQ(kMediaTime, pipeline_->GetMediaTime());

  // Seek backward 1 second. Do not run RunLoop to ensure renderer is not yet
  // notified of the seek (via media thread).
  base::TimeDelta kSeekTime = kMediaTime - base::Seconds(1);
  ExpectSeek(kSeekTime, false);
  pipeline_->Seek(kSeekTime, base::BindOnce(&CallbackHelper::OnSeek,
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
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Trigger two errors. The second error will be ignored.
  EXPECT_CALL(callbacks_, OnError(HasStatusCode(PIPELINE_ERROR_READ))).Times(1);
  renderer_client_->OnError(PIPELINE_ERROR_READ);
  renderer_client_->OnError(PIPELINE_ERROR_READ);

  base::RunLoop().RunUntilIdle();

  // Stopping the demuxer should clear internal state.
  EXPECT_CALL(*demuxer_, Stop());
  pipeline_->Stop();

  base::RunLoop().RunUntilIdle();

  ResetRenderer();
  SetDemuxerExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // New errors should propagate and not be ignored.
  EXPECT_CALL(callbacks_, OnError(HasStatusCode(PIPELINE_ERROR_READ))).Times(1);
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

  PipelineTeardownTest(const PipelineTeardownTest&) = delete;
  PipelineTeardownTest& operator=(const PipelineTeardownTest&) = delete;

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
        EXPECT_CALL(*demuxer_, OnInitialize(_, _))
            .WillOnce(
                DoAll(PostStop(pipeline_.get()), PostCallback<1>(PIPELINE_OK)));
        // Note: OnStart callback is not called after pipeline is stopped.
      } else {
        EXPECT_CALL(*demuxer_, OnInitialize(_, _))
            .WillOnce(PostCallback<1>(DEMUXER_ERROR_COULD_NOT_OPEN));
        EXPECT_CALL(callbacks_,
                    OnStart(HasStatusCode(DEMUXER_ERROR_COULD_NOT_OPEN)))
            .WillOnce(Stop(pipeline_.get()));
      }

      EXPECT_CALL(*demuxer_, Stop());
      return;
    }

    CreateAudioStream();
    CreateVideoStream();
    SetDemuxerExpectations(base::Seconds(3000));
    EXPECT_CALL(*renderer_, SetVolume(1.0f));
    EXPECT_CALL(*renderer_,
                SetWasPlayedWithUserActivationAndHighMediaEngagement(false));

    if (state == kInitRenderer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
            .WillOnce(
                DoAll(PostStop(pipeline_.get()), PostCallback<2>(PIPELINE_OK)));
        // Note: OnStart is not callback after pipeline is stopped.
      } else {
        EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
            .WillOnce(PostCallback<2>(PIPELINE_ERROR_INITIALIZATION_FAILED));
        EXPECT_CALL(
            callbacks_,
            OnStart(HasStatusCode(PIPELINE_ERROR_INITIALIZATION_FAILED)))
            .WillOnce(Stop(pipeline_.get()));
      }

      EXPECT_CALL(callbacks_, OnMetadata(_));
      EXPECT_CALL(*demuxer_, Stop());
      return;
    }

    EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
        .WillOnce(
            DoAll(SaveArg<1>(&renderer_client_), PostCallback<2>(PIPELINE_OK)));

    // If we get here it's a successful initialization.
    EXPECT_CALL(callbacks_, OnStart(HasStatusCode(PIPELINE_OK)));
    EXPECT_CALL(callbacks_, OnMetadata(_));

    EXPECT_CALL(*renderer_, SetPlaybackRate(0.0));
    EXPECT_CALL(*renderer_, StartPlayingFrom(base::TimeDelta()))
        .WillOnce(SetBufferingState(&renderer_client_, BUFFERING_HAVE_ENOUGH,
                                    BUFFERING_CHANGE_REASON_UNKNOWN));
    EXPECT_CALL(callbacks_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN));
  }

  void DoSeek(TeardownState state, StopOrError stop_or_error) {
    SetSeekExpectations(state, stop_or_error);

    EXPECT_CALL(*demuxer_, AbortPendingReads());
    EXPECT_CALL(*demuxer_, Stop());

    pipeline_->Seek(
        base::Seconds(10),
        base::BindOnce(&CallbackHelper::OnSeek, base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void SetSeekExpectations(TeardownState state, StopOrError stop_or_error) {
    if (state == kFlushing) {
      EXPECT_CALL(*demuxer_, OnSeek(_, _));
      if (stop_or_error == kStop) {
        EXPECT_CALL(*renderer_, OnFlush(_))
            .WillOnce(DoAll(Stop(pipeline_.get()), RunOnceClosure<0>()));
        // Note: OnSeek callbacks are not called
        // after pipeline is stopped.
      } else {
        EXPECT_CALL(*renderer_, OnFlush(_))
            .WillOnce(DoAll(SetError(&renderer_client_, PIPELINE_ERROR_READ),
                            RunOnceClosure<0>()));
        EXPECT_CALL(callbacks_, OnSeek(HasStatusCode(PIPELINE_ERROR_READ)))
            .WillOnce(Stop(pipeline_.get()));
      }
      return;
    }

    EXPECT_CALL(*renderer_, OnFlush(_)).WillOnce(RunOnceClosure<0>());

    if (state == kSeeking) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, OnSeek(_, _))
            .WillOnce(DoAll(PostStop(pipeline_.get()),
                            RunOnceCallback<1>(PIPELINE_OK)));
        // Note: OnSeek callback is not called after pipeline is stopped.
      } else {
        EXPECT_CALL(*demuxer_, OnSeek(_, _))
            .WillOnce(RunOnceCallback<1>(PIPELINE_ERROR_READ));
        EXPECT_CALL(callbacks_, OnSeek(HasStatusCode(PIPELINE_ERROR_READ)))
            .WillOnce(Stop(pipeline_.get()));
      }
      return;
    }

    NOTREACHED_IN_MIGRATION() << "State not supported: " << state;
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
    EXPECT_CALL(callbacks_, OnSuspend(HasStatusCode(PIPELINE_OK)));
    if (state == kResuming) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, OnSeek(_, _))
            .WillOnce(DoAll(PostStop(pipeline_.get()),
                            RunOnceCallback<1>(PIPELINE_OK)));
        // Note: OnResume callback is not called after pipeline is stopped.
      } else {
        EXPECT_CALL(*demuxer_, OnSeek(_, _))
            .WillOnce(RunOnceCallback<1>(PIPELINE_ERROR_READ));
        EXPECT_CALL(callbacks_, OnResume(HasStatusCode(PIPELINE_ERROR_READ)))
            .WillOnce(Stop(pipeline_.get()));
      }
    } else if (state != kSuspended && state != kSuspending) {
      NOTREACHED_IN_MIGRATION() << "State not supported: " << state;
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
          EXPECT_CALL(callbacks_, OnError(HasStatusCode(PIPELINE_ERROR_READ)))
              .WillOnce(Stop(pipeline_.get()));
        }
        renderer_client_->OnError(PIPELINE_ERROR_READ);
        break;

      case kErrorAndStop:
        EXPECT_CALL(*demuxer_, Stop());
        if (expect_errors)
          EXPECT_CALL(callbacks_, OnError(HasStatusCode(PIPELINE_ERROR_READ)));
        renderer_client_->OnError(PIPELINE_ERROR_READ);
        base::RunLoop().RunUntilIdle();
        pipeline_->Stop();
        break;
    }

    base::RunLoop().RunUntilIdle();
  }
};

#define INSTANTIATE_TEARDOWN_TEST(stop_or_error, state)   \
  TEST_F(PipelineTeardownTest, stop_or_error##_##state) { \
    RunTest(k##state, k##stop_or_error);                  \
  }

INSTANTIATE_TEARDOWN_TEST(Stop, InitDemuxer)
INSTANTIATE_TEARDOWN_TEST(Stop, InitRenderer)
INSTANTIATE_TEARDOWN_TEST(Stop, Flushing)
INSTANTIATE_TEARDOWN_TEST(Stop, Seeking)
INSTANTIATE_TEARDOWN_TEST(Stop, Playing)
INSTANTIATE_TEARDOWN_TEST(Stop, Suspending)
INSTANTIATE_TEARDOWN_TEST(Stop, Suspended)
INSTANTIATE_TEARDOWN_TEST(Stop, Resuming)

INSTANTIATE_TEARDOWN_TEST(Error, InitDemuxer)
INSTANTIATE_TEARDOWN_TEST(Error, InitRenderer)
INSTANTIATE_TEARDOWN_TEST(Error, Flushing)
INSTANTIATE_TEARDOWN_TEST(Error, Seeking)
INSTANTIATE_TEARDOWN_TEST(Error, Playing)
INSTANTIATE_TEARDOWN_TEST(Error, Suspending)
INSTANTIATE_TEARDOWN_TEST(Error, Suspended)
INSTANTIATE_TEARDOWN_TEST(Error, Resuming)

INSTANTIATE_TEARDOWN_TEST(ErrorAndStop, Playing)
INSTANTIATE_TEARDOWN_TEST(ErrorAndStop, Suspended)

}  // namespace media

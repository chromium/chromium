// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/renderer_impl.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunCallback;
using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::WithArgs;

namespace media {

const int64_t kStartPlayingTimeInMs = 100;

ACTION_P3(SetBufferingState, renderer_client, buffering_state, reason) {
  (*renderer_client)->OnBufferingStateChange(buffering_state, reason);
}

ACTION_P2(SetError, renderer_client, error) {
  (*renderer_client)->OnError(error);
}

class RendererImplTest : public ::testing::Test {
 public:
  class CallbackHelper : public MockRendererClient {
   public:
    CallbackHelper() = default;

    CallbackHelper(const CallbackHelper&) = delete;
    CallbackHelper& operator=(const CallbackHelper&) = delete;

    virtual ~CallbackHelper() = default;

    // Completion callbacks.
    MOCK_METHOD1(OnInitialize, void(PipelineStatus));
    MOCK_METHOD0(OnFlushed, void());
    MOCK_METHOD1(OnCdmAttached, void(bool));
    MOCK_METHOD1(OnDurationChange, void(base::TimeDelta duration));
    MOCK_METHOD0(OnVideoTrackChangeComplete, void());
    MOCK_METHOD0(OnAudioTrackChangeComplete, void());
  };

  RendererImplTest()
      : demuxer_(new StrictMock<MockDemuxer>()),
        video_renderer_(new StrictMock<MockVideoRenderer>()),
        audio_renderer_(new StrictMock<MockAudioRenderer>()),
        renderer_impl_(
            new RendererImpl(task_environment_.GetMainThreadTaskRunner(),
                             std::unique_ptr<AudioRenderer>(audio_renderer_),
                             std::unique_ptr<VideoRenderer>(video_renderer_))),
        cdm_context_(new StrictMock<MockCdmContext>()),
        video_renderer_client_(nullptr),
        audio_renderer_client_(nullptr),
        initialization_status_(PIPELINE_OK) {
    // CreateAudioStream() and CreateVideoStream() overrides expectations for
    // expected non-NULL streams.
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  RendererImplTest(const RendererImplTest&) = delete;
  RendererImplTest& operator=(const RendererImplTest&) = delete;

  ~RendererImplTest() override { Destroy(); }

 protected:
  void Destroy() {
    renderer_impl_.reset();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<StrictMock<MockDemuxerStream>> CreateStream(
      DemuxerStream::Type type) {
    std::unique_ptr<StrictMock<MockDemuxerStream>> stream(
        new StrictMock<MockDemuxerStream>(type));
    return stream;
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void SetAudioRendererInitializeExpectations(PipelineStatus status) {
    EXPECT_CALL(*audio_renderer_, OnInitialize(audio_stream_.get(), _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&audio_renderer_client_),
                        RunOnceCallback<3>(status)));
  }

  // Sets up expectations to allow the video renderer to initialize.
  void SetVideoRendererInitializeExpectations(PipelineStatus status) {
    EXPECT_CALL(*video_renderer_, OnInitialize(video_stream_.get(), _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&video_renderer_client_),
                        RunOnceCallback<4>(status)));
  }

  void InitializeAndExpect(PipelineStatus start_status) {
    EXPECT_CALL(callbacks_, OnInitialize(start_status))
        .WillOnce(SaveArg<0>(&initialization_status_));
    if (is_encrypted_ && !is_cdm_set_)
      EXPECT_CALL(callbacks_, OnWaiting(WaitingReason::kNoCdm));

    if (start_status == PIPELINE_OK && audio_stream_) {
      EXPECT_CALL(*audio_renderer_, GetTimeSource())
          .WillOnce(Return(&time_source_));
    } else {
      renderer_impl_->set_time_source_for_testing(&time_source_);
    }

    renderer_impl_->Initialize(demuxer_.get(), &callbacks_,
                               base::BindOnce(&CallbackHelper::OnInitialize,
                                              base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();

    if (start_status == PIPELINE_OK && audio_stream_) {
      ON_CALL(*audio_renderer_, Flush(_))
          .WillByDefault([this](base::OnceClosure on_done) {
            audio_renderer_client_->OnBufferingStateChange(
                BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
            std::move(on_done).Run();
          });
      ON_CALL(*audio_renderer_, StartPlaying())
          .WillByDefault(SetBufferingState(&audio_renderer_client_,
                                           BUFFERING_HAVE_ENOUGH,
                                           BUFFERING_CHANGE_REASON_UNKNOWN));
    }
    if (start_status == PIPELINE_OK && video_stream_) {
      ON_CALL(*video_renderer_, Flush(_))
          .WillByDefault([this](base::OnceClosure on_done) {
            video_renderer_client_->OnBufferingStateChange(
                BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
            std::move(on_done).Run();
          });
      ON_CALL(*video_renderer_, StartPlayingFrom(_))
          .WillByDefault(SetBufferingState(&video_renderer_client_,
                                           BUFFERING_HAVE_ENOUGH,
                                           BUFFERING_CHANGE_REASON_UNKNOWN));
    }
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
    streams_.push_back(audio_stream_.get());
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  void CreateVideoStream(bool is_encrypted = false) {
    is_encrypted_ = is_encrypted;
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
    video_stream_->set_video_decoder_config(
        is_encrypted ? TestVideoConfig::NormalEncrypted()
                     : TestVideoConfig::Normal());
    streams_.push_back(video_stream_.get());
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  void CreateEncryptedVideoStream() { CreateVideoStream(true); }

  void CreateAudioAndVideoStream() {
    CreateAudioStream();
    CreateVideoStream();
  }

  void InitializeWithAudio() {
    CreateAudioStream();
    SetAudioRendererInitializeExpectations(PIPELINE_OK);
    // There is a potential race between HTMLMediaElement/WMPI shutdown and
    // renderers being initialized which might result in MediaResource
    // GetAllStreams suddenly returning fewer streams than before or even
    // returning
    // and empty stream collection (see crbug.com/668604). So we are going to
    // check here that GetAllStreams will be invoked exactly 3 times during
    // RendererImpl initialization to help catch potential issues. Currently the
    // GetAllStreams is invoked once from the RendererImpl::Initialize via
    // HasEncryptedStream, once from the RendererImpl::InitializeAudioRenderer
    // and once from the RendererImpl::InitializeVideoRenderer.
    EXPECT_CALL(*demuxer_, GetAllStreams())
        .Times(3)
        .WillRepeatedly(Return(streams_));
    InitializeAndExpect(PIPELINE_OK);
  }

  void InitializeWithVideo() {
    CreateVideoStream();
    SetVideoRendererInitializeExpectations(PIPELINE_OK);
    // There is a potential race between HTMLMediaElement/WMPI shutdown and
    // renderers being initialized which might result in MediaResource
    // GetAllStreams suddenly returning fewer streams than before or even
    // returning
    // and empty stream collection (see crbug.com/668604). So we are going to
    // check here that GetAllStreams will be invoked exactly 3 times during
    // RendererImpl initialization to help catch potential issues. Currently the
    // GetAllStreams is invoked once from the RendererImpl::Initialize via
    // HasEncryptedStream, once from the RendererImpl::InitializeAudioRenderer
    // and once from the RendererImpl::InitializeVideoRenderer.
    EXPECT_CALL(*demuxer_, GetAllStreams())
        .Times(3)
        .WillRepeatedly(Return(streams_));
    InitializeAndExpect(PIPELINE_OK);
  }

  void InitializeWithAudioAndVideo() {
    CreateAudioAndVideoStream();
    SetAudioRendererInitializeExpectations(PIPELINE_OK);
    SetVideoRendererInitializeExpectations(PIPELINE_OK);
    InitializeAndExpect(PIPELINE_OK);
  }

  void Play() {
    DCHECK(audio_stream_ || video_stream_);
    EXPECT_CALL(callbacks_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN));

    base::TimeDelta start_time(base::Milliseconds(kStartPlayingTimeInMs));
    EXPECT_CALL(time_source_, SetMediaTime(start_time));
    EXPECT_CALL(time_source_, StartTicking());

    if (audio_stream_) {
      EXPECT_CALL(*audio_renderer_, StartPlaying());
    }

    if (video_stream_) {
      EXPECT_CALL(*video_renderer_, StartPlayingFrom(start_time));
    }

    renderer_impl_->StartPlayingFrom(start_time);
    base::RunLoop().RunUntilIdle();
  }

  void SetFlushExpectationsForAVRenderers() {
    if (audio_stream_)
      EXPECT_CALL(*audio_renderer_, Flush(_));

    if (video_stream_)
      EXPECT_CALL(*video_renderer_, Flush(_));
  }

  void Flush(bool underflowed) {
    if (!underflowed)
      EXPECT_CALL(time_source_, StopTicking());

    SetFlushExpectationsForAVRenderers();
    EXPECT_CALL(callbacks_, OnFlushed());

    renderer_impl_->Flush(base::BindOnce(&CallbackHelper::OnFlushed,
                                         base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void SetPlaybackRate(double playback_rate) {
    EXPECT_CALL(time_source_, SetPlaybackRate(playback_rate));
    renderer_impl_->SetPlaybackRate(playback_rate);
    base::RunLoop().RunUntilIdle();
  }

  int64_t GetMediaTimeMs() {
    return renderer_impl_->GetMediaTime().InMilliseconds();
  }

  bool IsMediaTimeAdvancing(double playback_rate) {
    int64_t start_time_ms = GetMediaTimeMs();
    const int64_t time_to_advance_ms = 100;

    test_tick_clock_.Advance(base::Milliseconds(time_to_advance_ms));

    if (GetMediaTimeMs() == start_time_ms + time_to_advance_ms * playback_rate)
      return true;

    DCHECK_EQ(start_time_ms, GetMediaTimeMs());
    return false;
  }

  bool IsMediaTimeAdvancing() {
    return IsMediaTimeAdvancing(1.0);
  }

  void SetCdmAndExpect(bool expected_result) {
    EXPECT_CALL(callbacks_, OnCdmAttached(expected_result))
        .WillOnce(SaveArg<0>(&is_cdm_set_));
    renderer_impl_->SetCdm(cdm_context_.get(),
                           base::BindOnce(&CallbackHelper::OnCdmAttached,
                                          base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void SetAudioTrackSwitchExpectations() {
    InSequence track_switch_seq;

    // Called from within OnEnabledAudioTracksChanged
    EXPECT_CALL(time_source_, CurrentMediaTime());
    EXPECT_CALL(time_source_, CurrentMediaTime());
    EXPECT_CALL(time_source_, StopTicking());
    EXPECT_CALL(*audio_renderer_, Flush(_));

    // Callback into RestartAudioRenderer
    EXPECT_CALL(*audio_renderer_, StartPlaying());

    // Callback into OnBufferingStateChange
    EXPECT_CALL(time_source_, StartTicking());
    EXPECT_CALL(callbacks_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN));
  }

  void SetVideoTrackSwitchExpectations() {
    InSequence track_switch_seq;

    // Called from within OnSelectedVideoTrackChanged
    EXPECT_CALL(time_source_, CurrentMediaTime());
    EXPECT_CALL(*video_renderer_, Flush(_));

    // Callback into RestartVideoRenderer
    EXPECT_CALL(*video_renderer_, StartPlayingFrom(_));

    // Callback into OnBufferingStateChange
    EXPECT_CALL(callbacks_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN));
  }

  // Fixture members.
  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<CallbackHelper> callbacks_;
  base::SimpleTestTickClock test_tick_clock_;

  std::unique_ptr<StrictMock<MockDemuxer>> demuxer_;
  raw_ptr<StrictMock<MockVideoRenderer>, DanglingUntriaged> video_renderer_;
  raw_ptr<StrictMock<MockAudioRenderer>, DanglingUntriaged> audio_renderer_;
  std::unique_ptr<RendererImpl> renderer_impl_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;

  StrictMock<MockTimeSource> time_source_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  std::vector<DemuxerStream*> streams_;
  raw_ptr<RendererClient, DanglingUntriaged> video_renderer_client_;
  raw_ptr<RendererClient, DanglingUntriaged> audio_renderer_client_;
  VideoDecoderConfig video_decoder_config_;
  PipelineStatus initialization_status_;
  bool is_encrypted_ = false;
  bool is_cdm_set_ = false;
};

TEST_F(RendererImplTest, NoStreams) {
  // Ensure initialization without streams fails and doesn't crash.
  EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  InitializeAndExpect(PIPELINE_ERROR_COULD_NOT_RENDER);
}

TEST_F(RendererImplTest, Destroy_BeforeInitialize) {
  Destroy();
}

TEST_F(RendererImplTest, Destroy_PendingInitialize) {
  CreateAudioAndVideoStream();

  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  // Not returning the video initialization callback.
  EXPECT_CALL(*video_renderer_, OnInitialize(video_stream_.get(), _, _, _, _));

  InitializeAndExpect(PIPELINE_ERROR_ABORT);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  Destroy();
}

TEST_F(RendererImplTest, Destroy_PendingInitializeWithoutCdm) {
  CreateAudioStream();
  CreateEncryptedVideoStream();

  // Audio is clear and video is encrypted. Initialization will not start
  // because no CDM is set. So neither AudioRenderer::Initialize() nor
  // VideoRenderer::Initialize() should not be called. The InitCB will be
  // aborted when |renderer_impl_| is destructed.
  InitializeAndExpect(PIPELINE_ERROR_ABORT);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  Destroy();
}

TEST_F(RendererImplTest, Destroy_PendingInitializeAfterSetCdm) {
  CreateAudioStream();
  CreateEncryptedVideoStream();

  // Audio is clear and video is encrypted. Initialization will not start
  // because no CDM is set.
  InitializeAndExpect(PIPELINE_ERROR_ABORT);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  // Not returning the video initialization callback. So initialization will
  // be pending.
  EXPECT_CALL(*video_renderer_, OnInitialize(video_stream_.get(), _, _, _, _));

  // SetCdm() will trigger the initialization to start. But it will not complete
  // because the |video_renderer_| is not returning the initialization callback.
  SetCdmAndExpect(true);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  Destroy();
}

TEST_F(RendererImplTest, InitializeWithAudio) {
  InitializeWithAudio();
}

TEST_F(RendererImplTest, InitializeWithVideo) {
  InitializeWithVideo();
}

TEST_F(RendererImplTest, InitializeWithAudioVideo) {
  InitializeWithAudioAndVideo();
}

TEST_F(RendererImplTest, InitializeWithAudio_Failed) {
  CreateAudioStream();
  SetAudioRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(RendererImplTest, InitializeWithVideo_Failed) {
  CreateVideoStream();
  SetVideoRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(RendererImplTest, InitializeWithAudioVideo_AudioRendererFailed) {
  CreateAudioAndVideoStream();
  SetAudioRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  // VideoRenderer::Initialize() should not be called.
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(RendererImplTest, InitializeWithAudioVideo_VideoRendererFailed) {
  CreateAudioAndVideoStream();
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(RendererImplTest, SetCdmBeforeInitialize) {
  // CDM will be successfully attached immediately if set before RendererImpl
  // initialization, regardless of the later initialization result.
  SetCdmAndExpect(true);
}

TEST_F(RendererImplTest, SetCdmAfterInitialize_ClearStream) {
  InitializeWithAudioAndVideo();
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  // CDM will be successfully attached immediately since initialization is
  // completed.
  SetCdmAndExpect(true);
}

TEST_F(RendererImplTest, SetCdmAfterInitialize_EncryptedStream_Success) {
  CreateAudioStream();
  CreateEncryptedVideoStream();

  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  // Initialization is pending until CDM is set.
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  SetCdmAndExpect(true);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);
}

TEST_F(RendererImplTest, SetCdmAfterInitialize_EncryptedStream_Failure) {
  CreateAudioStream();
  CreateEncryptedVideoStream();

  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
  // Initialization is pending until CDM is set.
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  SetCdmAndExpect(true);
  EXPECT_EQ(PIPELINE_ERROR_INITIALIZATION_FAILED, initialization_status_);
}

TEST_F(RendererImplTest, SetCdmMultipleTimes) {
  SetCdmAndExpect(true);
  SetCdmAndExpect(false);  // Do not support switching CDM.
}

TEST_F(RendererImplTest, StartPlayingFrom) {
  InitializeWithAudioAndVideo();
  Play();
}

TEST_F(RendererImplTest, StartPlayingFromWithPlaybackRate) {
  InitializeWithAudioAndVideo();

  // Play with a zero playback rate shouldn't start time.
  Play();
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Positive playback rate when ticking should start time.
  EXPECT_CALL(*video_renderer_, OnTimeProgressing());
  SetPlaybackRate(1.0);
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Double notifications shouldn't be sent.
  SetPlaybackRate(1.0);
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Zero playback rate should stop time.
  EXPECT_CALL(*video_renderer_, OnTimeStopped());
  SetPlaybackRate(0.0);
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Double notifications shouldn't be sent.
  SetPlaybackRate(0.0);
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Starting playback and flushing should cause time to stop.
  EXPECT_CALL(*video_renderer_, OnTimeProgressing());
  EXPECT_CALL(*video_renderer_, OnTimeStopped());
  SetPlaybackRate(1.0);
  Flush(false);

  // A positive playback rate when playback isn't started should do nothing.
  SetPlaybackRate(1.0);
}

TEST_F(RendererImplTest, FlushAfterInitialization) {
  InitializeWithAudioAndVideo();
  EXPECT_CALL(callbacks_, OnFlushed());
  renderer_impl_->Flush(base::BindOnce(&CallbackHelper::OnFlushed,
                                       base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, FlushAfterPlay) {
  InitializeWithAudioAndVideo();
  Play();
  Flush(false);
}

TEST_F(RendererImplTest, FlushAfterUnderflow) {
  InitializeWithAudioAndVideo();
  Play();

  // Simulate underflow.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  audio_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);

  // Flush while underflowed. We shouldn't call StopTicking() again.
  Flush(true);
}

TEST_F(RendererImplTest, SetPlaybackRate) {
  InitializeWithAudioAndVideo();
  SetPlaybackRate(1.0);
  SetPlaybackRate(2.0);
}

TEST_F(RendererImplTest, SetVolume) {
  InitializeWithAudioAndVideo();
  EXPECT_CALL(*audio_renderer_, SetVolume(2.0f));
  renderer_impl_->SetVolume(2.0f);
}

TEST_F(RendererImplTest, AudioStreamEnded) {
  InitializeWithAudio();
  Play();

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnEnded());

  audio_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, VideoStreamEnded) {
  InitializeWithVideo();
  Play();

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnEnded());
  EXPECT_CALL(*video_renderer_, OnTimeStopped());

  video_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, AudioVideoStreamsEnded) {
  InitializeWithAudioAndVideo();
  Play();

  // OnEnded() is called only when all streams have finished.
  audio_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnEnded());
  EXPECT_CALL(*video_renderer_, OnTimeStopped());

  video_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorAfterInitialize) {
  InitializeWithAudio();
  EXPECT_CALL(callbacks_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)));
  audio_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorDuringPlaying) {
  InitializeWithAudio();
  Play();

  EXPECT_CALL(callbacks_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)));
  audio_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorDuringFlush) {
  InitializeWithAudio();
  Play();

  InSequence s;
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce([this](base::OnceClosure on_done) {
        audio_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
        std::move(on_done).Run();
      });
  EXPECT_CALL(callbacks_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)));
  EXPECT_CALL(callbacks_, OnFlushed());
  renderer_impl_->Flush(base::BindOnce(&CallbackHelper::OnFlushed,
                                       base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorAfterFlush) {
  InitializeWithAudio();
  Play();
  Flush(false);

  EXPECT_CALL(callbacks_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)));
  audio_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorDuringInitialize) {
  CreateAudioAndVideoStream();
  SetAudioRendererInitializeExpectations(PIPELINE_OK);

  // Force an audio error to occur during video renderer initialization.
  EXPECT_CALL(*video_renderer_, OnInitialize(video_stream_.get(), _, _, _, _))
      .WillOnce(DoAll(SetError(&audio_renderer_client_, PIPELINE_ERROR_DECODE),
                      SaveArg<2>(&video_renderer_client_),
                      RunOnceCallback<4>(PIPELINE_OK)));

  InitializeAndExpect(PIPELINE_ERROR_DECODE);
}

TEST_F(RendererImplTest, AudioUnderflow) {
  InitializeWithAudio();
  Play();

  // Underflow should occur immediately with a single audio track.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  audio_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
}

TEST_F(RendererImplTest, AudioUnderflowWithVideo) {
  InitializeWithAudioAndVideo();
  Play();

  // Underflow should be immediate when both audio and video are present and
  // audio underflows.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  audio_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
}

TEST_F(RendererImplTest, VideoUnderflow) {
  InitializeWithVideo();
  Play();

  // Underflow should occur immediately with a single video track.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
}

TEST_F(RendererImplTest, VideoUnderflowWithAudio) {
  InitializeWithAudioAndVideo();
  Play();

  // Set a zero threshold such that the underflow will be executed on the next
  // run of the message loop.
  renderer_impl_->set_video_underflow_threshold_for_testing(base::TimeDelta());

  // Underflow should be delayed when both audio and video are present and video
  // underflows.
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  Mock::VerifyAndClearExpectations(&time_source_);

  EXPECT_CALL(time_source_, StopTicking());
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, VideoUnderflowWithAudioVideoRecovers) {
  InitializeWithAudioAndVideo();
  Play();

  // Set a zero threshold such that the underflow will be executed on the next
  // run of the message loop.
  renderer_impl_->set_video_underflow_threshold_for_testing(base::TimeDelta());

  // Underflow should be delayed when both audio and video are present and video
  // underflows.
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN))
      .Times(0);
  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  Mock::VerifyAndClearExpectations(&time_source_);

  // If video recovers, the underflow should never occur.
  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_ENOUGH, BUFFERING_CHANGE_REASON_UNKNOWN);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, VideoAndAudioUnderflow) {
  InitializeWithAudioAndVideo();
  Play();

  // Set a zero threshold such that the underflow will be executed on the next
  // run of the message loop.
  renderer_impl_->set_video_underflow_threshold_for_testing(base::TimeDelta());

  // Underflow should be delayed when both audio and video are present and video
  // underflows.
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN))
      .Times(0);
  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  Mock::VerifyAndClearExpectations(&time_source_);

  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  EXPECT_CALL(time_source_, StopTicking());
  audio_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);

  // Nothing else should primed on the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, VideoUnderflowWithAudioFlush) {
  InitializeWithAudioAndVideo();
  Play();

  // Set a massive threshold such that it shouldn't fire within this test.
  renderer_impl_->set_video_underflow_threshold_for_testing(base::Seconds(100));

  // Simulate the cases where audio underflows and then video underflows.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  audio_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  Mock::VerifyAndClearExpectations(&time_source_);

  // Flush the audio and video renderers, both think they're in an underflow
  // state, but if the video renderer underflow was deferred, RendererImpl would
  // think it still has enough data.
  EXPECT_CALL(*audio_renderer_, Flush(_)).WillOnce(RunOnceClosure<0>());
  EXPECT_CALL(*video_renderer_, Flush(_)).WillOnce(RunOnceClosure<0>());
  EXPECT_CALL(callbacks_, OnFlushed());
  renderer_impl_->Flush(base::BindOnce(&CallbackHelper::OnFlushed,
                                       base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();

  // Start playback after the flush, but never return BUFFERING_HAVE_ENOUGH from
  // the video renderer (which simulates spool up time for the video renderer).
  const base::TimeDelta kStartTime;
  EXPECT_CALL(time_source_, SetMediaTime(kStartTime));
  EXPECT_CALL(time_source_, StartTicking());
  EXPECT_CALL(*audio_renderer_, StartPlaying());
  EXPECT_CALL(*video_renderer_, StartPlayingFrom(kStartTime));
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  renderer_impl_->StartPlayingFrom(kStartTime);

  // Nothing else should primed on the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, AudioTrackDisableThenEnable) {
  InitializeWithAudioAndVideo();
  Play();
  Mock::VerifyAndClearExpectations(&time_source_);

  base::RunLoop disable_wait;
  SetAudioTrackSwitchExpectations();
  renderer_impl_->OnEnabledAudioTracksChanged({}, disable_wait.QuitClosure());
  disable_wait.Run();

  base::RunLoop enable_wait;
  SetAudioTrackSwitchExpectations();
  renderer_impl_->OnEnabledAudioTracksChanged({streams_[0]},
                                              enable_wait.QuitClosure());
  enable_wait.Run();
}

TEST_F(RendererImplTest, VideoTrackDisableThenEnable) {
  InitializeWithAudioAndVideo();
  Play();
  Mock::VerifyAndClearExpectations(&time_source_);

  base::RunLoop disable_wait;
  SetVideoTrackSwitchExpectations();
  renderer_impl_->OnSelectedVideoTracksChanged({}, disable_wait.QuitClosure());
  disable_wait.Run();

  base::RunLoop enable_wait;
  SetVideoTrackSwitchExpectations();
  renderer_impl_->OnSelectedVideoTracksChanged({streams_[1]},
                                               enable_wait.QuitClosure());
  enable_wait.Run();

  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, AudioUnderflowDuringAudioTrackChange) {
  InitializeWithAudioAndVideo();
  Play();

  base::RunLoop loop;

  // Underflow should occur immediately with a single audio track.
  EXPECT_CALL(time_source_, StopTicking());

  // Capture the callback from the audio renderer flush.
  base::OnceClosure audio_renderer_flush_cb;
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce(MoveArg(&audio_renderer_flush_cb));

  EXPECT_CALL(time_source_, CurrentMediaTime()).Times(2);
  std::vector<DemuxerStream*> tracks;
  renderer_impl_->OnEnabledAudioTracksChanged({}, loop.QuitClosure());

  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));

  EXPECT_CALL(time_source_, StartTicking());
  EXPECT_CALL(*audio_renderer_, StartPlaying());
  audio_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  std::move(audio_renderer_flush_cb).Run();
  loop.Run();
}

TEST_F(RendererImplTest, VideoUnderflowDuringVideoTrackChange) {
  InitializeWithAudioAndVideo();
  Play();

  base::RunLoop loop;

  // Capture the callback from the video renderer flush.
  base::OnceClosure video_renderer_flush_cb;
  {
    InSequence track_switch_seq;
    EXPECT_CALL(time_source_, CurrentMediaTime());
    EXPECT_CALL(*video_renderer_, Flush(_))
        .WillOnce(MoveArg(&video_renderer_flush_cb));
    EXPECT_CALL(*video_renderer_, StartPlayingFrom(_));
    EXPECT_CALL(callbacks_,
                OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                       BUFFERING_CHANGE_REASON_UNKNOWN));
  }

  renderer_impl_->OnSelectedVideoTracksChanged({}, loop.QuitClosure());

  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  std::move(video_renderer_flush_cb).Run();
  loop.Run();
}

TEST_F(RendererImplTest, VideoUnderflowDuringAudioTrackChange) {
  InitializeWithAudioAndVideo();
  Play();

  base::RunLoop loop;

  // Capture the callback from the audio renderer flush.
  base::OnceClosure audio_renderer_flush_cb;
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce(MoveArg(&audio_renderer_flush_cb));

  EXPECT_CALL(time_source_, CurrentMediaTime()).Times(2);
  EXPECT_CALL(time_source_, StopTicking());
  renderer_impl_->OnEnabledAudioTracksChanged({}, loop.QuitClosure());

  EXPECT_CALL(*audio_renderer_, StartPlaying());
  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  std::move(audio_renderer_flush_cb).Run();
  loop.Run();
}

TEST_F(RendererImplTest, AudioUnderflowDuringVideoTrackChange) {
  InitializeWithAudioAndVideo();
  Play();

  base::RunLoop loop;
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  EXPECT_CALL(time_source_, CurrentMediaTime());

  // Capture the callback from the audio renderer flush.
  base::OnceClosure video_renderer_flush_cb;
  EXPECT_CALL(*video_renderer_, Flush(_))
      .WillOnce(MoveArg(&video_renderer_flush_cb));

  renderer_impl_->OnSelectedVideoTracksChanged({}, loop.QuitClosure());

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(*video_renderer_, StartPlayingFrom(_));

  audio_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);

  std::move(video_renderer_flush_cb).Run();
  loop.Run();
}

TEST_F(RendererImplTest, VideoResumedFromUnderflowDuringAudioTrackChange) {
  InitializeWithAudioAndVideo();
  Play();

  // Underflow the renderer.
  base::RunLoop underflow_wait;
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN))
      .WillOnce(RunClosure(underflow_wait.QuitClosure()));
  EXPECT_CALL(time_source_, StopTicking());
  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  underflow_wait.Run();

  // Start a track change.
  base::OnceClosure audio_renderer_flush_cb;
  base::RunLoop track_change;
  {
    InSequence track_switch_seq;
    EXPECT_CALL(time_source_, CurrentMediaTime()).Times(2);
    EXPECT_CALL(*audio_renderer_, Flush(_))
        .WillOnce(MoveArg(&audio_renderer_flush_cb));
  }
  renderer_impl_->OnEnabledAudioTracksChanged({}, track_change.QuitClosure());

  // Signal that the renderer has enough data to resume from underflow.
  // Nothing should bubble up, since we are pending audio track change.
  EXPECT_CALL(callbacks_, OnBufferingStateChange(_, _)).Times(0);
  EXPECT_CALL(time_source_, StartTicking()).Times(0);
  video_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_ENOUGH, BUFFERING_CHANGE_REASON_UNKNOWN);

  // Finish the track change.
  EXPECT_CALL(*audio_renderer_, StartPlaying());
  std::move(audio_renderer_flush_cb).Run();
  track_change.Run();
}

TEST_F(RendererImplTest, AudioResumedFromUnderflowDuringVideoTrackChange) {
  InitializeWithAudioAndVideo();
  Play();

  // Underflow the renderer.
  base::RunLoop underflow_wait;
  EXPECT_CALL(callbacks_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                     BUFFERING_CHANGE_REASON_UNKNOWN))
      .WillOnce(RunClosure(underflow_wait.QuitClosure()));
  EXPECT_CALL(time_source_, StopTicking());
  audio_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_NOTHING, BUFFERING_CHANGE_REASON_UNKNOWN);
  underflow_wait.Run();

  // Start a track change.
  base::OnceClosure video_renderer_flush_cb;
  base::RunLoop track_change;
  {
    InSequence track_switch_seq;
    EXPECT_CALL(time_source_, CurrentMediaTime());
    EXPECT_CALL(*video_renderer_, Flush(_))
        .WillOnce(MoveArg(&video_renderer_flush_cb));
  }
  renderer_impl_->OnSelectedVideoTracksChanged({}, track_change.QuitClosure());

  // Signal that the renderer has enough data to resume from underflow.
  // Nothing should bubble up, since we are pending audio track change.
  EXPECT_CALL(callbacks_, OnBufferingStateChange(_, _)).Times(0);
  EXPECT_CALL(time_source_, StartTicking()).Times(0);
  audio_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_ENOUGH, BUFFERING_CHANGE_REASON_UNKNOWN);

  // Finish the track change.
  EXPECT_CALL(*video_renderer_, StartPlayingFrom(_));
  std::move(video_renderer_flush_cb).Run();
  track_change.Run();
}

}  // namespace media

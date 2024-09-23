// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_message_loop.h"
#include "base/threading/platform_thread.h"
#include "base/timer/elapsed_timer.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_context.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_renderer_service.h"
#include "media/renderers/video_overlay_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

namespace {
const int64_t kStartPlayingTimeInMs = 100;

ACTION_P2(GetMediaTime, start_time, elapsed_timer) {
  return start_time + elapsed_timer->Elapsed();
}

void WaitFor(base::TimeDelta duration) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();
}
}  // namespace

class MojoRendererTest : public ::testing::Test {
 public:
  MojoRendererTest() {
    std::unique_ptr<StrictMock<MockRenderer>> mock_renderer(
        new StrictMock<MockRenderer>());
    mock_renderer_ = mock_renderer.get();

    mojo::PendingRemote<mojom::Renderer> remote_renderer_remote;
    renderer_receiver_ = MojoRendererService::Create(
        &mojo_cdm_service_context_, std::move(mock_renderer),
        remote_renderer_remote.InitWithNewPipeAndPassReceiver());

    mojo_renderer_ = std::make_unique<MojoRenderer>(
        message_loop_.task_runner(),
        std::unique_ptr<VideoOverlayFactory>(nullptr), nullptr,
        std::move(remote_renderer_remote));

    // CreateAudioStream() and CreateVideoStream() overrides expectations for
    // expected non-NULL streams.
    EXPECT_CALL(demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));

    EXPECT_CALL(*mock_renderer_, GetMediaTime())
        .WillRepeatedly(Return(base::TimeDelta()));
  }

  MojoRendererTest(const MojoRendererTest&) = delete;
  MojoRendererTest& operator=(const MojoRendererTest&) = delete;

  ~MojoRendererTest() override = default;

  void Destroy() {
    mojo_renderer_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Completion callbacks.
  MOCK_METHOD1(OnInitialized, void(PipelineStatus));
  MOCK_METHOD0(OnFlushed, void());
  MOCK_METHOD1(OnCdmAttached, void(bool));

  std::unique_ptr<StrictMock<MockDemuxerStream>> CreateStream(
      DemuxerStream::Type type) {
    std::unique_ptr<StrictMock<MockDemuxerStream>> stream(
        new StrictMock<MockDemuxerStream>(type));
    return stream;
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
    audio_stream_->set_audio_decoder_config(TestAudioConfig::Normal());
    streams_.push_back(audio_stream_.get());
    EXPECT_CALL(demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  void CreateVideoStream(bool is_encrypted = false) {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
    video_stream_->set_video_decoder_config(
        is_encrypted ? TestVideoConfig::NormalEncrypted()
                     : TestVideoConfig::Normal());
    std::vector<DemuxerStream*> streams;
    streams_.push_back(audio_stream_.get());
    EXPECT_CALL(demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  void InitializeAndExpect(PipelineStatus status) {
    DVLOG(1) << __func__ << ": " << status;
    EXPECT_CALL(*this, OnInitialized(SameStatusCode(status)));
    mojo_renderer_->Initialize(&demuxer_, &renderer_client_,
                               base::BindOnce(&MojoRendererTest::OnInitialized,
                                              base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Initialize() {
    CreateAudioStream();
    EXPECT_CALL(*mock_renderer_, SetVolume(1));
    EXPECT_CALL(*mock_renderer_, OnInitialize(_, _, _))
        .WillOnce(DoAll(SaveArg<1>(&remote_renderer_client_),
                        RunOnceCallback<2>(PIPELINE_OK)));
    InitializeAndExpect(PIPELINE_OK);
  }

  void Flush() {
    DVLOG(1) << __func__;
    // Flush callback should always be fired.
    EXPECT_CALL(*this, OnFlushed());
    mojo_renderer_->Flush(
        base::BindOnce(&MojoRendererTest::OnFlushed, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void SetCdmAndExpect(bool success) {
    DVLOG(1) << __func__;
    // Set CDM callback should always be fired.
    EXPECT_CALL(*this, OnCdmAttached(success));
    mojo_renderer_->SetCdm(&cdm_context_,
                           base::BindOnce(&MojoRendererTest::OnCdmAttached,
                                          base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Simulates a connection error at the client side by killing the service.
  // Note that |mock_renderer_| will also be destroyed, do NOT expect anything
  // on it. Otherwise the test will crash.
  void ConnectionError() {
    DVLOG(1) << __func__;
    DCHECK(renderer_receiver_);
    renderer_receiver_->Close();
    base::RunLoop().RunUntilIdle();
  }

  void OnCdmServiceInitialized(mojom::CdmContextPtr cdm_context,
                               CreateCdmStatus status) {
    cdm_context_.set_cdm_id(cdm_context->cdm_id);
  }

  void CreateCdm() {
    mojo_cdm_service_ =
        std::make_unique<MojoCdmService>(&mojo_cdm_service_context_);
    mojo_cdm_service_->Initialize(
        &cdm_factory_, {kClearKeyKeySystem, false, false, false},
        base::BindOnce(&MojoRendererTest::OnCdmServiceInitialized,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void StartPlayingFrom(base::TimeDelta start_time) {
    EXPECT_CALL(*mock_renderer_, StartPlayingFrom(start_time));
    mojo_renderer_->StartPlayingFrom(start_time);
    EXPECT_EQ(start_time, mojo_renderer_->GetMediaTime());
    base::RunLoop().RunUntilIdle();
  }

  void Play() { StartPlayingFrom(base::Milliseconds(kStartPlayingTimeInMs)); }

  // Fixture members.
  base::TestMessageLoop message_loop_;

  // The MojoRenderer that we are testing.
  std::unique_ptr<MojoRenderer> mojo_renderer_;

  // Client side mocks and helpers.
  StrictMock<MockRendererClient> renderer_client_;
  StrictMock<MockCdmContext> cdm_context_;
  mojo::Remote<mojom::ContentDecryptionModule> cdm_remote_;

  // Client side mock demuxer and demuxer streams.
  StrictMock<MockDemuxer> demuxer_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  std::vector<DemuxerStream*> streams_;

  // Service side bindings (declaration order is critical).
  MojoCdmServiceContext mojo_cdm_service_context_;
  DefaultCdmFactory cdm_factory_;
  std::unique_ptr<MojoCdmService> mojo_cdm_service_;

  // Service side mocks and helpers.
  raw_ptr<StrictMock<MockRenderer>, AcrossTasksDanglingUntriaged>
      mock_renderer_;
  raw_ptr<RendererClient, DanglingUntriaged> remote_renderer_client_;

  mojo::SelfOwnedReceiverRef<mojom::Renderer> renderer_receiver_;
};

TEST_F(MojoRendererTest, Initialize_Success) {
  Initialize();
}

TEST_F(MojoRendererTest, Initialize_Failure) {
  CreateAudioStream();
  // Mojo Renderer only expects a boolean result, which will be translated
  // to PIPELINE_OK or PIPELINE_ERROR_INITIALIZATION_FAILED.
  EXPECT_CALL(*mock_renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_ERROR_ABORT));
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, Initialize_BeforeConnectionError) {
  CreateAudioStream();
  EXPECT_CALL(*mock_renderer_, OnInitialize(_, _, _))
      .WillOnce(InvokeWithoutArgs(this, &MojoRendererTest::ConnectionError));
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, Initialize_AfterConnectionError) {
  ConnectionError();
  CreateAudioStream();
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, Flush_Success) {
  Initialize();

  EXPECT_CALL(*mock_renderer_, OnFlush(_)).WillOnce(RunOnceClosure<0>());
  Flush();
}

TEST_F(MojoRendererTest, Flush_ConnectionError) {
  Initialize();

  // Upon connection error, OnError() should be called once and only once.
  EXPECT_CALL(renderer_client_,
              OnError(HasStatusCode(PIPELINE_ERROR_DISCONNECTED)))
      .Times(1);
  EXPECT_CALL(*mock_renderer_, OnFlush(_))
      .WillOnce(InvokeWithoutArgs(this, &MojoRendererTest::ConnectionError));
  Flush();
}

TEST_F(MojoRendererTest, Flush_AfterConnectionError) {
  Initialize();

  // Upon connection error, OnError() should be called once and only once.
  EXPECT_CALL(renderer_client_,
              OnError(HasStatusCode(PIPELINE_ERROR_DISCONNECTED)))
      .Times(1);
  ConnectionError();

  Flush();
}

TEST_F(MojoRendererTest, SetCdm_Success) {
  Initialize();
  CreateCdm();
  EXPECT_CALL(*mock_renderer_, OnSetCdm(_, _))
      .WillOnce(RunOnceCallback<1>(true));
  SetCdmAndExpect(true);
}

TEST_F(MojoRendererTest, SetCdm_Failure) {
  Initialize();
  CreateCdm();
  EXPECT_CALL(*mock_renderer_, OnSetCdm(_, _))
      .WillOnce(RunOnceCallback<1>(false));
  SetCdmAndExpect(false);
}

TEST_F(MojoRendererTest, SetCdm_InvalidCdmId) {
  Initialize();
  SetCdmAndExpect(false);
}

TEST_F(MojoRendererTest, SetCdm_NonExistCdmId) {
  Initialize();
  cdm_context_.set_cdm_id(base::UnguessableToken::Create());
  SetCdmAndExpect(false);
}

TEST_F(MojoRendererTest, SetCdm_ReleasedCdmId) {
  // The CdmContext set on |mock_renderer_|.
  CdmContext* mock_renderer_cdm_context = nullptr;

  Initialize();
  CreateCdm();
  EXPECT_CALL(*mock_renderer_, OnSetCdm(_, _))
      .WillOnce(DoAll(SaveArg<0>(&mock_renderer_cdm_context),
                      RunOnceCallback<1>(true)));
  SetCdmAndExpect(true);
  EXPECT_TRUE(mock_renderer_cdm_context);

  // Release the CDM.
  mojo_cdm_service_.reset();
  base::RunLoop().RunUntilIdle();

  // SetCdm() on |mock_renderer_| should not be called.
  SetCdmAndExpect(false);

  // The CDM should still be around since it's set on the |mock_renderer_|. It
  // should have a Decryptor since we use kClearKeyKeySystem.
  EXPECT_TRUE(mock_renderer_cdm_context->GetDecryptor());
}

TEST_F(MojoRendererTest, SetCdm_BeforeInitialize) {
  CreateCdm();
  EXPECT_CALL(*mock_renderer_, OnSetCdm(_, _))
      .WillOnce(RunOnceCallback<1>(true));
  SetCdmAndExpect(true);
}

TEST_F(MojoRendererTest, SetCdm_AfterInitializeAndConnectionError) {
  CreateCdm();
  Initialize();
  EXPECT_CALL(renderer_client_,
              OnError(HasStatusCode(PIPELINE_ERROR_DISCONNECTED)))
      .Times(1);
  ConnectionError();
  SetCdmAndExpect(false);
}

TEST_F(MojoRendererTest, SetCdm_AfterConnectionErrorAndBeforeInitialize) {
  CreateCdm();
  // Initialize() is not called so RendererClient::OnError() is not expected.
  ConnectionError();
  SetCdmAndExpect(false);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, SetCdm_BeforeInitializeAndConnectionError) {
  CreateCdm();
  EXPECT_CALL(*mock_renderer_, OnSetCdm(_, _))
      .WillOnce(RunOnceCallback<1>(true));
  SetCdmAndExpect(true);
  // Initialize() is not called so RendererClient::OnError() is not expected.
  ConnectionError();
  CreateAudioStream();
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(MojoRendererTest, StartPlayingFrom) {
  Initialize();
  Play();
}

TEST_F(MojoRendererTest, GetMediaTime) {
  Initialize();
  EXPECT_EQ(base::TimeDelta(), mojo_renderer_->GetMediaTime());

  const base::TimeDelta kSleepTime = base::Milliseconds(500);
  const base::TimeDelta kStartTime = base::Milliseconds(kStartPlayingTimeInMs);

  // Media time should not advance since playback rate is 0.
  EXPECT_CALL(*mock_renderer_, SetPlaybackRate(0));
  EXPECT_CALL(*mock_renderer_, StartPlayingFrom(kStartTime));
  EXPECT_CALL(*mock_renderer_, GetMediaTime())
      .WillRepeatedly(Return(kStartTime));
  mojo_renderer_->SetPlaybackRate(0);
  mojo_renderer_->StartPlayingFrom(kStartTime);
  WaitFor(kSleepTime);
  EXPECT_EQ(kStartTime, mojo_renderer_->GetMediaTime());

  // Media time should now advance since playback rate is > 0.
  std::unique_ptr<base::ElapsedTimer> elapsed_timer(new base::ElapsedTimer);
  EXPECT_CALL(*mock_renderer_, SetPlaybackRate(1.0));
  EXPECT_CALL(*mock_renderer_, GetMediaTime())
      .WillRepeatedly(GetMediaTime(kStartTime, elapsed_timer.get()));
  mojo_renderer_->SetPlaybackRate(1.0);
  WaitFor(kSleepTime);
  EXPECT_GT(mojo_renderer_->GetMediaTime(), kStartTime);

  // Flushing should pause media-time updates.
  EXPECT_CALL(*mock_renderer_, OnFlush(_)).WillOnce(RunOnceClosure<0>());
  Flush();
  base::TimeDelta pause_time = mojo_renderer_->GetMediaTime();
  EXPECT_GT(pause_time, kStartTime);
  WaitFor(kSleepTime);
  EXPECT_EQ(pause_time, mojo_renderer_->GetMediaTime());
  Destroy();
}

TEST_F(MojoRendererTest, OnBufferingStateChange) {
  Initialize();
  Play();

  EXPECT_CALL(renderer_client_,
              OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                     BUFFERING_CHANGE_REASON_UNKNOWN))
      .Times(1);
  remote_renderer_client_->OnBufferingStateChange(
      BUFFERING_HAVE_ENOUGH, BUFFERING_CHANGE_REASON_UNKNOWN);

  EXPECT_CALL(renderer_client_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING, DECODER_UNDERFLOW))
      .Times(1);
  remote_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING,
                                                  DECODER_UNDERFLOW);

  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoRendererTest, OnEnded) {
  Initialize();
  Play();

  EXPECT_CALL(renderer_client_, OnEnded()).Times(1);
  remote_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

// TODO(xhwang): Add tests for all RendererClient methods.

TEST_F(MojoRendererTest, Destroy_PendingInitialize) {
  CreateAudioStream();
  EXPECT_CALL(*mock_renderer_, OnInitialize(_, _, _))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<2>(PIPELINE_ERROR_ABORT));
  EXPECT_CALL(*this, OnInitialized(
                         HasStatusCode(PIPELINE_ERROR_INITIALIZATION_FAILED)));
  mojo_renderer_->Initialize(
      &demuxer_, &renderer_client_,
      base::BindOnce(&MojoRendererTest::OnInitialized, base::Unretained(this)));
  Destroy();
}

TEST_F(MojoRendererTest, Destroy_PendingFlush) {
  EXPECT_CALL(*mock_renderer_, OnSetCdm(_, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(true));
  EXPECT_CALL(*this, OnCdmAttached(false));
  mojo_renderer_->SetCdm(
      &cdm_context_,
      base::BindOnce(&MojoRendererTest::OnCdmAttached, base::Unretained(this)));
  Destroy();
}

TEST_F(MojoRendererTest, Destroy_PendingSetCdm) {
  Initialize();

  EXPECT_CALL(*mock_renderer_, OnFlush(_)).WillRepeatedly(RunOnceClosure<0>());
  EXPECT_CALL(*this, OnFlushed());
  mojo_renderer_->Flush(
      base::BindOnce(&MojoRendererTest::OnFlushed, base::Unretained(this)));
  Destroy();
}

TEST_F(MojoRendererTest, ErrorDuringPlayback) {
  Initialize();

  EXPECT_CALL(renderer_client_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)))
      .Times(1);

  Play();
  remote_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_renderer_, SetPlaybackRate(0.0)).Times(1);
  mojo_renderer_->SetPlaybackRate(0.0);
  Flush();
}

TEST_F(MojoRendererTest, ErrorBeforeFlush) {
  Initialize();
  Play();

  EXPECT_CALL(renderer_client_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)))
      .Times(1);
  remote_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  Flush();
}

TEST_F(MojoRendererTest, ErrorDuringFlush) {
  Initialize();
  Play();

  EXPECT_CALL(renderer_client_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)))
      .Times(1);
  EXPECT_CALL(*mock_renderer_, OnFlush(_))
      .WillOnce(DoAll(InvokeWithoutArgs([&]() {
                        remote_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
                      }),
                      RunOnceClosure<0>()));
  Flush();
}

}  // namespace media

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/receiver.h"

#include <utility>

#include "base/check.h"
#include "base/optional.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/renderer.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder_config.h"
#include "media/remoting/mock_receiver_controller.h"
#include "media/remoting/proto_enum_utils.h"
#include "media/remoting/proto_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::StrictMock;

namespace media {
namespace remoting {

class MockSender {
 public:
  MockSender(RpcBroker* rpc_broker, int remote_handle)
      : rpc_broker_(rpc_broker),
        rpc_handle_(rpc_broker->GetUniqueHandle()),
        remote_handle_(remote_handle) {
    rpc_broker_->RegisterMessageReceiverCallback(
        rpc_handle_, base::BindRepeating(&MockSender::OnReceivedRpc,
                                         base::Unretained(this)));
  }

  MOCK_METHOD(void, AcquireRendererDone, ());
  MOCK_METHOD(void, InitializeCallback, (bool));
  MOCK_METHOD(void, FlushUntilCallback, ());
  MOCK_METHOD(void, OnTimeUpdate, (int64_t, int64_t));
  MOCK_METHOD(void, OnBufferingStateChange, (BufferingState));
  MOCK_METHOD(void, OnEnded, ());
  MOCK_METHOD(void, OnFatalError, ());
  MOCK_METHOD(void, OnAudioConfigChange, (AudioDecoderConfig));
  MOCK_METHOD(void, OnVideoConfigChange, (VideoDecoderConfig));
  MOCK_METHOD(void, OnVideoNaturalSizeChange, (gfx::Size));
  MOCK_METHOD(void, OnVideoOpacityChange, (bool));
  MOCK_METHOD(void, OnStatisticsUpdate, (PipelineStatistics));
  MOCK_METHOD(void, OnWaiting, ());

  void OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message) {
    DCHECK(message);
    switch (message->proc()) {
      case pb::RpcMessage::RPC_ACQUIRE_RENDERER_DONE:
        AcquireRendererDone();
        break;
      case pb::RpcMessage::RPC_R_INITIALIZE_CALLBACK:
        InitializeCallback(message->boolean_value());
        break;
      case pb::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK:
        FlushUntilCallback();
        break;
      case pb::RpcMessage::RPC_RC_ONTIMEUPDATE: {
        DCHECK(message->has_rendererclient_ontimeupdate_rpc());
        const int64_t time_usec =
            message->rendererclient_ontimeupdate_rpc().time_usec();
        const int64_t max_time_usec =
            message->rendererclient_ontimeupdate_rpc().max_time_usec();
        OnTimeUpdate(time_usec, max_time_usec);
        break;
      }
      case pb::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE: {
        base::Optional<BufferingState> state = ToMediaBufferingState(
            message->rendererclient_onbufferingstatechange_rpc().state());
        if (state.has_value())
          OnBufferingStateChange(state.value());
        break;
      }
      case pb::RpcMessage::RPC_RC_ONENDED:
        OnEnded();
        break;
      case pb::RpcMessage::RPC_RC_ONERROR:
        OnFatalError();
        break;
      case pb::RpcMessage::RPC_RC_ONAUDIOCONFIGCHANGE: {
        DCHECK(message->has_rendererclient_onaudioconfigchange_rpc());
        const auto* audio_config_message =
            message->mutable_rendererclient_onaudioconfigchange_rpc();
        const pb::AudioDecoderConfig pb_audio_config =
            audio_config_message->audio_decoder_config();
        AudioDecoderConfig out_audio_config;
        ConvertProtoToAudioDecoderConfig(pb_audio_config, &out_audio_config);
        DCHECK(out_audio_config.IsValidConfig());
        OnAudioConfigChange(out_audio_config);
        break;
      }
      case pb::RpcMessage::RPC_RC_ONVIDEOCONFIGCHANGE: {
        DCHECK(message->has_rendererclient_onvideoconfigchange_rpc());
        const auto* video_config_message =
            message->mutable_rendererclient_onvideoconfigchange_rpc();
        const pb::VideoDecoderConfig pb_video_config =
            video_config_message->video_decoder_config();
        VideoDecoderConfig out_video_config;
        ConvertProtoToVideoDecoderConfig(pb_video_config, &out_video_config);
        DCHECK(out_video_config.IsValidConfig());

        OnVideoConfigChange(out_video_config);
        break;
      }
      case pb::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE: {
        DCHECK(message->has_rendererclient_onvideonatualsizechange_rpc());

        gfx::Size size(
            message->rendererclient_onvideonatualsizechange_rpc().width(),
            message->rendererclient_onvideonatualsizechange_rpc().height());
        OnVideoNaturalSizeChange(size);
        break;
      }
      case pb::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE:
        OnVideoOpacityChange(message->boolean_value());
        break;
      case pb::RpcMessage::RPC_RC_ONSTATISTICSUPDATE: {
        DCHECK(message->has_rendererclient_onstatisticsupdate_rpc());
        auto rpc_message = message->rendererclient_onstatisticsupdate_rpc();
        PipelineStatistics statistics;
        statistics.audio_bytes_decoded = rpc_message.audio_bytes_decoded();
        statistics.video_bytes_decoded = rpc_message.video_bytes_decoded();
        statistics.video_frames_decoded = rpc_message.video_frames_decoded();
        statistics.video_frames_dropped = rpc_message.video_frames_dropped();
        statistics.audio_memory_usage = rpc_message.audio_memory_usage();
        statistics.video_memory_usage = rpc_message.video_memory_usage();
        OnStatisticsUpdate(statistics);
        break;
      }

      default:
        VLOG(1) << "Unknown RPC: " << message->proc();
    }
  }

  void SendRpcAcquireRenderer() {
    auto rpc = std::make_unique<pb::RpcMessage>();
    rpc->set_handle(RpcBroker::kAcquireRendererHandle);
    rpc->set_proc(pb::RpcMessage::RPC_ACQUIRE_RENDERER);
    rpc->set_integer_value(rpc_handle_);
    rpc_broker_->SendMessageToRemote(std::move(rpc));
  }

  void SendRpcInitialize() {
    auto rpc = std::make_unique<pb::RpcMessage>();
    rpc->set_handle(remote_handle_);
    rpc->set_proc(pb::RpcMessage::RPC_R_INITIALIZE);
    rpc_broker_->SendMessageToRemote(std::move(rpc));
  }

  void SendRpcSetPlaybackRate(double playback_rate) {
    auto rpc = std::make_unique<pb::RpcMessage>();
    rpc->set_handle(remote_handle_);
    rpc->set_proc(pb::RpcMessage::RPC_R_SETPLAYBACKRATE);
    rpc->set_double_value(playback_rate);
    rpc_broker_->SendMessageToRemote(std::move(rpc));
  }

  void SendRpcFlushUntil(uint32_t audio_count, uint32_t video_count) {
    auto rpc = std::make_unique<pb::RpcMessage>();
    rpc->set_handle(remote_handle_);
    rpc->set_proc(pb::RpcMessage::RPC_R_FLUSHUNTIL);
    pb::RendererFlushUntil* message = rpc->mutable_renderer_flushuntil_rpc();
    message->set_audio_count(audio_count);
    message->set_video_count(video_count);
    message->set_callback_handle(rpc_handle_);
    rpc_broker_->SendMessageToRemote(std::move(rpc));
  }

  void SendRpcStartPlayingFrom(base::TimeDelta time) {
    auto rpc = std::make_unique<pb::RpcMessage>();
    rpc->set_handle(remote_handle_);
    rpc->set_proc(pb::RpcMessage::RPC_R_STARTPLAYINGFROM);
    rpc->set_integer64_value(time.InMicroseconds());
    rpc_broker_->SendMessageToRemote(std::move(rpc));
  }

  void SendRpcSetVolume(float volume) {
    auto rpc = std::make_unique<pb::RpcMessage>();
    rpc->set_handle(remote_handle_);
    rpc->set_proc(pb::RpcMessage::RPC_R_SETVOLUME);
    rpc->set_double_value(volume);
    rpc_broker_->SendMessageToRemote(std::move(rpc));
  }

 private:
  RpcBroker* const rpc_broker_;
  const int rpc_handle_;
  const int remote_handle_;
};

class ReceiverTest : public ::testing::Test {
 public:
  ReceiverTest() = default;

  void SetUp() override {
    mock_controller_ = MockReceiverController::GetInstance();
    mock_controller_->Initialize(
        mock_controller_->mock_remotee()->BindNewPipeAndPassRemote());
    mock_remotee_ = mock_controller_->mock_remotee();

    rpc_broker_ = mock_controller_->rpc_broker();
    receiver_renderer_handle_ = rpc_broker_->GetUniqueHandle();

    mock_sender_ = std::make_unique<StrictMock<MockSender>>(
        rpc_broker_, receiver_renderer_handle_);

    rpc_broker_->RegisterMessageReceiverCallback(
        RpcBroker::kAcquireRendererHandle,
        base::BindRepeating(&ReceiverTest::OnReceivedRpc,
                            weak_factory_.GetWeakPtr()));
  }

  void TearDown() override {
    rpc_broker_->UnregisterMessageReceiverCallback(
        RpcBroker::kAcquireRendererHandle);
  }

  void OnReceivedRpc(std::unique_ptr<media::remoting::pb::RpcMessage> message) {
    DCHECK(message);
    EXPECT_EQ(message->proc(),
              media::remoting::pb::RpcMessage::RPC_ACQUIRE_RENDERER);
    OnAcquireRenderer(std::move(message));
  }

  void OnAcquireRenderer(
      std::unique_ptr<media::remoting::pb::RpcMessage> message) {
    DCHECK(message->has_integer_value());
    DCHECK(message->integer_value() != RpcBroker::kInvalidHandle);

    if (sender_renderer_handle_ == RpcBroker::kInvalidHandle) {
      sender_renderer_handle_ = message->integer_value();
      SetRemoteHandle();
    }
  }

  void OnAcquireRendererDone(int receiver_renderer_handle) {
    DVLOG(3) << __func__
             << ": Issues RPC_ACQUIRE_RENDERER_DONE RPC message. remote_handle="
             << sender_renderer_handle_
             << " rpc_handle=" << receiver_renderer_handle;
    auto rpc = std::make_unique<pb::RpcMessage>();
    rpc->set_handle(sender_renderer_handle_);
    rpc->set_proc(pb::RpcMessage::RPC_ACQUIRE_RENDERER_DONE);
    rpc->set_integer_value(receiver_renderer_handle);
    rpc_broker_->SendMessageToRemote(std::move(rpc));
  }

  void CreateReceiver() {
    auto renderer = std::make_unique<NiceMock<MockRenderer>>();
    mock_renderer_ = renderer.get();
    receiver_ = std::make_unique<Receiver>(
        receiver_renderer_handle_, sender_renderer_handle_, mock_controller_,
        base::ThreadTaskRunnerHandle::Get(), std::move(renderer),
        base::BindOnce(&ReceiverTest::OnAcquireRendererDone,
                       weak_factory_.GetWeakPtr()));
  }

  void SetRemoteHandle() {
    if (!receiver_)
      return;
    receiver_->SetRemoteHandle(sender_renderer_handle_);
  }

  void InitializeReceiver() {
    receiver_->Initialize(&mock_media_resource_, nullptr,
                          base::BindOnce(&ReceiverTest::OnRendererInitialized,
                                         weak_factory_.GetWeakPtr()));
  }

  MOCK_METHOD(void, OnRendererInitialized, (PipelineStatus));

  base::test::TaskEnvironment task_environment_;

  int sender_renderer_handle_ = RpcBroker::kInvalidHandle;
  int receiver_renderer_handle_ = RpcBroker::kInvalidHandle;

  MockMediaResource mock_media_resource_;
  MockRenderer* mock_renderer_;
  std::unique_ptr<MockSender> mock_sender_;

  RpcBroker* rpc_broker_;
  MockRemotee* mock_remotee_;
  MockReceiverController* mock_controller_;
  std::unique_ptr<Receiver> receiver_;

  base::WeakPtrFactory<ReceiverTest> weak_factory_{this};
};

TEST_F(ReceiverTest, AcquireRendererBeforeCreateReceiver) {
  mock_sender_->SendRpcAcquireRenderer();
  EXPECT_CALL(*mock_sender_, AcquireRendererDone()).Times(1);
  CreateReceiver();
  task_environment_.RunUntilIdle();
}

TEST_F(ReceiverTest, AcquireRendererAfterCreateReceiver) {
  CreateReceiver();
  EXPECT_CALL(*mock_sender_, AcquireRendererDone()).Times(1);
  mock_sender_->SendRpcAcquireRenderer();
  task_environment_.RunUntilIdle();
}

// |Receiver::Initialize| will be called by the local pipeline, and the
// |Receiver::RpcInitialize| will be called once it received the
// RPC_R_INITIALIZE messages, so these two initialization functions are possible
// to be called in difference orders.
//
// Call |Receiver::Initialize| first, then send RPC_R_INITIALIZE.
TEST_F(ReceiverTest, InitializeBeforeRpcInitialize) {
  EXPECT_CALL(*mock_sender_, AcquireRendererDone()).Times(1);
  mock_sender_->SendRpcAcquireRenderer();
  CreateReceiver();

  EXPECT_CALL(*mock_renderer_,
              OnInitialize(&mock_media_resource_, receiver_.get(), _))
      .WillOnce(RunOnceCallback<2>(PipelineStatus::PIPELINE_OK));
  EXPECT_CALL(*this, OnRendererInitialized(PipelineStatus::PIPELINE_OK))
      .Times(1);
  EXPECT_CALL(*mock_sender_, InitializeCallback(true)).Times(1);

  InitializeReceiver();
  mock_sender_->SendRpcInitialize();
  task_environment_.RunUntilIdle();
}

// Send RPC_R_INITIALIZE first, then call |Receiver::Initialize|.
TEST_F(ReceiverTest, InitializeAfterRpcInitialize) {
  EXPECT_CALL(*mock_sender_, AcquireRendererDone()).Times(1);
  mock_sender_->SendRpcAcquireRenderer();
  CreateReceiver();

  EXPECT_CALL(*mock_renderer_,
              OnInitialize(&mock_media_resource_, receiver_.get(), _))
      .WillOnce(RunOnceCallback<2>(PipelineStatus::PIPELINE_OK));
  EXPECT_CALL(*this, OnRendererInitialized(PipelineStatus::PIPELINE_OK))
      .Times(1);
  EXPECT_CALL(*mock_sender_, InitializeCallback(true)).Times(1);

  mock_sender_->SendRpcInitialize();
  InitializeReceiver();
  task_environment_.RunUntilIdle();
}

TEST_F(ReceiverTest, RpcRendererMessages) {
  EXPECT_CALL(*mock_sender_, AcquireRendererDone()).Times(1);
  mock_sender_->SendRpcAcquireRenderer();
  CreateReceiver();
  mock_sender_->SendRpcInitialize();
  InitializeReceiver();
  task_environment_.RunUntilIdle();

  // SetVolume
  const float volume = 0.5;
  EXPECT_CALL(*mock_renderer_, SetVolume(volume)).Times(1);
  mock_sender_->SendRpcSetVolume(volume);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*mock_sender_, OnTimeUpdate(_, _)).Times(AtLeast(1));

  // SetPlaybackRate
  const double playback_rate = 1.2;
  EXPECT_CALL(*mock_renderer_, SetPlaybackRate(playback_rate)).Times(1);
  mock_sender_->SendRpcSetPlaybackRate(playback_rate);
  task_environment_.RunUntilIdle();

  // Flush
  const uint32_t flush_audio_count = 10;
  const uint32_t flush_video_count = 20;
  EXPECT_CALL(*mock_renderer_, OnFlush(_)).WillOnce(RunOnceCallback<0>());
  EXPECT_CALL(*mock_sender_, FlushUntilCallback()).Times(1);
  mock_sender_->SendRpcFlushUntil(flush_audio_count, flush_video_count);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(flush_audio_count, mock_remotee_->flush_audio_count());
  EXPECT_EQ(flush_video_count, mock_remotee_->flush_video_count());

  // StartPlayingFrom
  const base::TimeDelta time = base::TimeDelta::FromSeconds(100);
  EXPECT_CALL(*mock_renderer_, StartPlayingFrom(time)).Times(1);
  mock_sender_->SendRpcStartPlayingFrom(time);
  task_environment_.RunUntilIdle();
}

TEST_F(ReceiverTest, RendererClientInterface) {
  EXPECT_CALL(*mock_sender_, AcquireRendererDone()).Times(1);
  mock_sender_->SendRpcAcquireRenderer();
  CreateReceiver();
  mock_sender_->SendRpcInitialize();
  InitializeReceiver();
  task_environment_.RunUntilIdle();

  // OnBufferingStateChange
  EXPECT_CALL(*mock_sender_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(1);
  receiver_->OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                    BUFFERING_CHANGE_REASON_UNKNOWN);
  task_environment_.RunUntilIdle();

  // OnEnded
  EXPECT_CALL(*mock_sender_, OnEnded()).Times(1);
  receiver_->OnEnded();
  task_environment_.RunUntilIdle();

  // OnError
  EXPECT_CALL(*mock_sender_, OnFatalError()).Times(1);
  receiver_->OnError(PipelineStatus::AUDIO_RENDERER_ERROR);
  task_environment_.RunUntilIdle();

  // OnAudioConfigChange
  const auto kNewAudioConfig = TestAudioConfig::Normal();
  EXPECT_CALL(*mock_sender_,
              OnAudioConfigChange(DecoderConfigEq(kNewAudioConfig)))
      .Times(1);
  receiver_->OnAudioConfigChange(kNewAudioConfig);
  task_environment_.RunUntilIdle();

  // OnVideoConfigChange
  const auto kNewVideoConfig = TestVideoConfig::Normal();
  EXPECT_CALL(*mock_sender_,
              OnVideoConfigChange(DecoderConfigEq(kNewVideoConfig)))
      .Times(1);
  receiver_->OnVideoConfigChange(kNewVideoConfig);
  task_environment_.RunUntilIdle();

  // OnVideoNaturalSizeChange
  const gfx::Size size(100, 200);
  EXPECT_CALL(*mock_sender_, OnVideoNaturalSizeChange(size)).Times(1);
  receiver_->OnVideoNaturalSizeChange(size);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(size, mock_remotee_->changed_size());

  // OnVideoOpacityChange
  const bool opaque = true;
  EXPECT_CALL(*mock_sender_, OnVideoOpacityChange(opaque)).Times(1);
  receiver_->OnVideoOpacityChange(opaque);
  task_environment_.RunUntilIdle();

  // OnStatisticsUpdate
  PipelineStatistics statistics;
  statistics.audio_bytes_decoded = 100;
  statistics.video_bytes_decoded = 200;
  statistics.video_frames_decoded = 300;
  statistics.video_frames_dropped = 400;
  statistics.audio_memory_usage = 500;
  statistics.video_memory_usage = 600;
  EXPECT_CALL(*mock_sender_, OnStatisticsUpdate(statistics)).Times(1);
  receiver_->OnStatisticsUpdate(statistics);
  task_environment_.RunUntilIdle();
}

}  // namespace remoting
}  // namespace media

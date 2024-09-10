// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/courier_renderer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "media/base/media_util.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/base/test_helpers.h"
#include "media/cast/openscreen/remoting_proto_enum_utils.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "media/remoting/fake_media_resource.h"
#include "media/remoting/fake_remoter.h"
#include "media/remoting/renderer_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

using openscreen::cast::RpcMessenger;
using testing::_;
using testing::Invoke;
using testing::Return;

namespace media {
namespace remoting {

namespace {

PipelineMetadata DefaultMetadata() {
  PipelineMetadata data;
  data.has_audio = true;
  data.has_video = true;
  data.video_decoder_config = TestVideoConfig::Normal();
  return data;
}

PipelineStatistics DefaultStats() {
  PipelineStatistics stats;
  stats.audio_bytes_decoded = 1234U;
  stats.video_bytes_decoded = 2345U;
  stats.video_frames_decoded = 3000U;
  stats.video_frames_dropped = 91U;
  stats.audio_memory_usage = 5678;
  stats.video_memory_usage = 6789;
  stats.video_keyframe_distance_average = base::TimeDelta::Max();
  stats.audio_pipeline_info = {false, false, AudioDecoderType::kUnknown,
                               EncryptionType::kClear};
  stats.video_pipeline_info = {false, false, VideoDecoderType::kUnknown,
                               EncryptionType::kClear};
  return stats;
}

class RendererClientImpl final : public RendererClient {
 public:
  RendererClientImpl() {
    ON_CALL(*this, OnStatisticsUpdate(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnStatisticsUpdate));
    ON_CALL(*this, OnPipelineStatus(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnPipelineStatus));
    ON_CALL(*this, OnBufferingStateChange(_, _))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnBufferingStateChange));
    ON_CALL(*this, OnAudioConfigChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnAudioConfigChange));
    ON_CALL(*this, OnVideoConfigChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnVideoConfigChange));
    ON_CALL(*this, OnVideoNaturalSizeChange(_))
        .WillByDefault(Invoke(
            this, &RendererClientImpl::DelegateOnVideoNaturalSizeChange));
    ON_CALL(*this, OnVideoOpacityChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnVideoOpacityChange));
  }

  RendererClientImpl(const RendererClientImpl&) = delete;
  RendererClientImpl& operator=(const RendererClientImpl&) = delete;

  ~RendererClientImpl() = default;

  // RendererClient implementation.
  void OnError(PipelineStatus status) override {}
  void OnFallback(PipelineStatus status) override {}
  void OnEnded() override {}
  MOCK_METHOD1(OnStatisticsUpdate, void(const PipelineStatistics& stats));
  MOCK_METHOD2(OnBufferingStateChange,
               void(BufferingState state, BufferingStateChangeReason reason));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig& config));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig& config));
  void OnWaiting(WaitingReason reason) override {}
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size& size));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool opaque));
  MOCK_METHOD1(OnVideoFrameRateChange, void(std::optional<int>));
  MOCK_METHOD1(OnRemotePlayStateChange, void(MediaStatus::State state));

  void DelegateOnStatisticsUpdate(const PipelineStatistics& stats) {
    stats_ = stats;
  }
  void DelegateOnBufferingStateChange(BufferingState state,
                                      BufferingStateChangeReason reason) {
    state_ = state;
  }
  void DelegateOnAudioConfigChange(const AudioDecoderConfig& config) {
    audio_decoder_config_ = config;
  }
  void DelegateOnVideoConfigChange(const VideoDecoderConfig& config) {
    video_decoder_config_ = config;
  }
  void DelegateOnVideoNaturalSizeChange(const gfx::Size& size) { size_ = size; }
  void DelegateOnVideoOpacityChange(bool opaque) { opaque_ = opaque; }

  MOCK_METHOD1(OnPipelineStatus, void(PipelineStatus status));
  void DelegateOnPipelineStatus(PipelineStatus status) {
    VLOG(2) << "OnPipelineStatus status:" << status;
    status_ = status;
  }
  MOCK_METHOD0(OnFlushCallback, void());

  PipelineStatus status() const { return status_; }
  PipelineStatistics stats() const { return stats_; }
  BufferingState state() const { return state_; }
  gfx::Size size() const { return size_; }
  bool opaque() const { return opaque_; }
  VideoDecoderConfig video_decoder_config() const {
    return video_decoder_config_;
  }
  AudioDecoderConfig audio_decoder_config() const {
    return audio_decoder_config_;
  }

 private:
  PipelineStatus status_ = PIPELINE_OK;
  BufferingState state_ = BUFFERING_HAVE_NOTHING;
  gfx::Size size_;
  bool opaque_ = false;
  PipelineStatistics stats_;
  VideoDecoderConfig video_decoder_config_;
  AudioDecoderConfig audio_decoder_config_;
};

}  // namespace

class CourierRendererTest : public testing::Test {
 public:
  CourierRendererTest() = default;

  CourierRendererTest(const CourierRendererTest&) = delete;
  CourierRendererTest& operator=(const CourierRendererTest&) = delete;

  ~CourierRendererTest() override = default;

  // Use this function to mimic receiver to handle RPC message for renderer
  // initialization,
  void RpcMessageResponseBot(std::vector<uint8_t> message) {
    openscreen::cast::RpcMessage rpc;
    ASSERT_TRUE(rpc.ParseFromArray(message.data(), message.size()));
    switch (rpc.proc()) {
      case openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER: {
        DCHECK(rpc.has_integer_value());
        sender_renderer_handle_ = rpc.integer_value();
        // Issues RPC_ACQUIRE_RENDERER_DONE RPC message.
        auto acquire_done = std::make_unique<openscreen::cast::RpcMessage>();
        acquire_done->set_handle(sender_renderer_handle_);
        acquire_done->set_proc(
            openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER_DONE);
        acquire_done->set_integer_value(receiver_renderer_handle_);
        controller_->GetRpcMessenger()->ProcessMessageFromRemote(
            std::move(acquire_done));
      } break;
      case openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER: {
        if (!is_backward_compatible_mode_) {
          int acquire_demuxer_handle = RpcMessenger::kAcquireDemuxerHandle;
          EXPECT_EQ(rpc.handle(), acquire_demuxer_handle);
          sender_audio_demuxer_handle_ =
              rpc.acquire_demuxer_rpc().audio_demuxer_handle();
          sender_video_demuxer_handle_ =
              rpc.acquire_demuxer_rpc().video_demuxer_handle();

          // Issues audio RPC_DS_INITIALIZE RPC message.
          if (sender_audio_demuxer_handle_ != RpcMessenger::kInvalidHandle) {
            auto ds_init = std::make_unique<openscreen::cast::RpcMessage>();
            ds_init->set_handle(sender_audio_demuxer_handle_);
            ds_init->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE);
            ds_init->set_integer_value(receiver_audio_demuxer_callback_handle_);
            controller_->GetRpcMessenger()->ProcessMessageFromRemote(
                std::move(ds_init));
          }

          // Issues video RPC_DS_INITIALIZE RPC message.
          if (sender_video_demuxer_handle_ != RpcMessenger::kInvalidHandle) {
            auto ds_init = std::make_unique<openscreen::cast::RpcMessage>();
            ds_init->set_handle(sender_video_demuxer_handle_);
            ds_init->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE);
            ds_init->set_integer_value(receiver_video_demuxer_callback_handle_);
            controller_->GetRpcMessenger()->ProcessMessageFromRemote(
                std::move(ds_init));
          }
        }
      } break;
      case openscreen::cast::RpcMessage::RPC_R_INITIALIZE: {
        sender_renderer_callback_handle_ =
            rpc.renderer_initialize_rpc().callback_handle();
        sender_client_handle_ = rpc.renderer_initialize_rpc().client_handle();

        if (is_backward_compatible_mode_) {
          EXPECT_EQ(rpc.handle(), receiver_renderer_handle_);

          sender_audio_demuxer_handle_ =
              rpc.renderer_initialize_rpc().audio_demuxer_handle();
          sender_video_demuxer_handle_ =
              rpc.renderer_initialize_rpc().video_demuxer_handle();

          // Issues audio RPC_DS_INITIALIZE RPC message.
          if (sender_audio_demuxer_handle_ != RpcMessenger::kInvalidHandle) {
            auto ds_init = std::make_unique<openscreen::cast::RpcMessage>();
            ds_init->set_handle(sender_audio_demuxer_handle_);
            ds_init->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE);
            ds_init->set_integer_value(receiver_audio_demuxer_callback_handle_);
            controller_->GetRpcMessenger()->ProcessMessageFromRemote(
                std::move(ds_init));
          }

          // Issues video RPC_DS_INITIALIZE RPC message.
          if (sender_video_demuxer_handle_ != RpcMessenger::kInvalidHandle) {
            auto ds_init = std::make_unique<openscreen::cast::RpcMessage>();
            ds_init->set_handle(sender_video_demuxer_handle_);
            ds_init->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE);
            ds_init->set_integer_value(receiver_video_demuxer_callback_handle_);
            controller_->GetRpcMessenger()->ProcessMessageFromRemote(
                std::move(ds_init));
          }
        } else {
          // Issues RPC_R_INITIALIZE_CALLBACK RPC message when receiving
          // RPC_R_INITIALIZE.
          auto init_cb = std::make_unique<openscreen::cast::RpcMessage>();
          init_cb->set_handle(sender_renderer_callback_handle_);
          init_cb->set_proc(
              openscreen::cast::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
          init_cb->set_boolean_value(is_successfully_initialized_);
          controller_->GetRpcMessenger()->ProcessMessageFromRemote(
              std::move(init_cb));
        }
      } break;
      case openscreen::cast::RpcMessage::RPC_DS_INITIALIZE_CALLBACK: {
        if (rpc.handle() == receiver_audio_demuxer_callback_handle_)
          received_audio_ds_init_cb_ = true;
        if (rpc.handle() == receiver_video_demuxer_callback_handle_)
          received_video_ds_init_cb_ = true;

        // Check whether the demuxer at the receiver end is initialized.
        if (received_audio_ds_init_cb_ == (sender_audio_demuxer_handle_ !=
                                           RpcMessenger::kInvalidHandle) &&
            received_video_ds_init_cb_ == (sender_video_demuxer_handle_ !=
                                           RpcMessenger::kInvalidHandle)) {
          is_receiver_demuxer_initialized_ = true;
        }

        if (is_backward_compatible_mode_ && is_receiver_demuxer_initialized_) {
          // Issues RPC_R_INITIALIZE_CALLBACK RPC message when receiving
          // RPC_DS_INITIALIZE_CALLBACK on available streams.
          auto init_cb = std::make_unique<openscreen::cast::RpcMessage>();
          init_cb->set_handle(sender_renderer_callback_handle_);
          init_cb->set_proc(
              openscreen::cast::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
          init_cb->set_boolean_value(is_successfully_initialized_);
          controller_->GetRpcMessenger()->ProcessMessageFromRemote(
              std::move(init_cb));
        }
      } break;
      case openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL: {
        // Issues RPC_R_FLUSHUNTIL_CALLBACK RPC message.
        std::unique_ptr<openscreen::cast::RpcMessage> flush_cb(
            new openscreen::cast::RpcMessage());
        flush_cb->set_handle(rpc.renderer_flushuntil_rpc().callback_handle());
        flush_cb->set_proc(
            openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK);
        controller_->GetRpcMessenger()->ProcessMessageFromRemote(
            std::move(flush_cb));
      } break;
      case openscreen::cast::RpcMessage::RPC_R_SETVOLUME:
        // No response needed.
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }
    RunPendingTasks();
  }

  // Callback from RpcMessenger when sending message to remote sink.
  void OnSendMessageToSink(std::vector<uint8_t> message) {
    openscreen::cast::RpcMessage rpc;
    ASSERT_TRUE(rpc.ParseFromArray(message.data(), message.size()));
    received_rpc_.push_back(std::move(rpc));
  }

  void RewireSendMessageCallbackToSink() {
    controller_->GetRpcMessenger()->set_send_message_cb_for_testing(
        [this](std::vector<uint8_t> message) {
          this->OnSendMessageToSink(message);
        });
  }

 protected:
  void InitializeRenderer() {
    // Register media::RendererClient implementation.
    render_client_ = std::make_unique<RendererClientImpl>();
    media_resource_ = std::make_unique<FakeMediaResource>();
    EXPECT_CALL(*render_client_, OnPipelineStatus(_)).Times(1);
    DCHECK(renderer_);
    // Redirect RPC message for simulate receiver scenario
    controller_->GetRpcMessenger()->set_send_message_cb_for_testing(
        [this](std::vector<uint8_t> message) {
          this->RpcMessageResponseBot(message);
        });
    RunPendingTasks();
    renderer_->Initialize(
        media_resource_.get(), render_client_.get(),
        base::BindOnce(&RendererClientImpl::OnPipelineStatus,
                       base::Unretained(render_client_.get())));
    RunPendingTasks();
    RewireSendMessageCallbackToSink();
    RunPendingTasks();
  }

  void InitializeRendererBackwardsCompatible() {
    is_backward_compatible_mode_ = true;
    InitializeRenderer();
  }

  bool IsRendererInitialized() const {
    EXPECT_TRUE(received_audio_ds_init_cb_);
    EXPECT_TRUE(received_video_ds_init_cb_);
    return renderer_->state_ == CourierRenderer::STATE_PLAYING &&
           is_receiver_demuxer_initialized_;
  }

  bool DidEncounterFatalError() const {
    return renderer_->state_ == CourierRenderer::STATE_ERROR;
  }

  void OnReceivedRpc(std::unique_ptr<openscreen::cast::RpcMessage> message) {
    renderer_->OnReceivedRpc(std::move(message));
  }

  void SetUp() override {
    controller_ = FakeRemoterFactory::CreateController(false);
    controller_->OnMetadataChanged(DefaultMetadata());

    RewireSendMessageCallbackToSink();
    renderer_ = std::make_unique<CourierRenderer>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        controller_->GetWeakPtr(), nullptr);
    renderer_->clock_ = &clock_;
    clock_.Advance(base::Seconds(1));

    RunPendingTasks();
  }

  CourierRenderer::State state() const { return renderer_->state_; }

  void RunPendingTasks() { base::RunLoop().RunUntilIdle(); }

  // Gets first available RpcMessage with specific |proc|.
  const openscreen::cast::RpcMessage* PeekRpcMessage(int proc) const {
    for (auto& s : received_rpc_) {
      if (proc == s.proc())
        return &s;
    }

    return nullptr;
  }
  int ReceivedRpcMessageCount() const { return received_rpc_.size(); }
  void ResetReceivedRpcMessage() { received_rpc_.clear(); }

  void ValidateCurrentTime(base::TimeDelta current,
                           base::TimeDelta current_max) const {
    ASSERT_EQ(renderer_->current_media_time_, current);
    ASSERT_EQ(renderer_->current_max_time_, current_max);
  }

  // Issues RPC_RC_ONTIMEUPDATE RPC message.
  void IssueTimeUpdateRpc(base::TimeDelta media_time,
                          base::TimeDelta max_media_time) {
    std::unique_ptr<openscreen::cast::RpcMessage> rpc(
        new openscreen::cast::RpcMessage());
    rpc->set_handle(5);
    rpc->set_proc(openscreen::cast::RpcMessage::RPC_RC_ONTIMEUPDATE);
    auto* time_message = rpc->mutable_rendererclient_ontimeupdate_rpc();
    time_message->set_time_usec(media_time.InMicroseconds());
    time_message->set_max_time_usec(max_media_time.InMicroseconds());
    OnReceivedRpc(std::move(rpc));
    RunPendingTasks();
  }

  // Verifies no error reported and issues a series of time updates RPC
  // messages. No verification after the last message is issued.
  void VerifyAndReportTimeUpdates(int start_serial_number,
                                  int end_serial_number) {
    for (int i = start_serial_number; i < end_serial_number; ++i) {
      ASSERT_FALSE(DidEncounterFatalError());
      IssueTimeUpdateRpc(base::Milliseconds(100 + i * 800), base::Seconds(100));
      clock_.Advance(base::Seconds(1));
      RunPendingTasks();
    }
  }

  // Issues RPC_RC_ONSTATISTICSUPDATE RPC message with DefaultStats().
  void IssueStatisticsUpdateRpc() {
    EXPECT_CALL(*render_client_, OnStatisticsUpdate(_)).Times(1);
    const PipelineStatistics stats = DefaultStats();
    std::unique_ptr<openscreen::cast::RpcMessage> rpc(
        new openscreen::cast::RpcMessage());
    rpc->set_handle(5);
    rpc->set_proc(openscreen::cast::RpcMessage::RPC_RC_ONSTATISTICSUPDATE);
    auto* message = rpc->mutable_rendererclient_onstatisticsupdate_rpc();
    message->set_audio_bytes_decoded(stats.audio_bytes_decoded);
    message->set_video_bytes_decoded(stats.video_bytes_decoded);
    message->set_video_frames_decoded(stats.video_frames_decoded);
    message->set_video_frames_dropped(stats.video_frames_dropped);
    message->set_audio_memory_usage(stats.audio_memory_usage);
    message->set_video_memory_usage(stats.video_memory_usage);
    message->mutable_audio_decoder_info()->set_is_platform_decoder(
        stats.audio_pipeline_info.is_platform_decoder);
    message->mutable_audio_decoder_info()->set_decoder_type(
        static_cast<int64_t>(stats.audio_pipeline_info.decoder_type));
    message->mutable_video_decoder_info()->set_is_platform_decoder(
        stats.video_pipeline_info.is_platform_decoder);
    message->mutable_video_decoder_info()->set_decoder_type(
        static_cast<int64_t>(stats.video_pipeline_info.decoder_type));
    OnReceivedRpc(std::move(rpc));
    RunPendingTasks();
  }

  // Issue RPC_RC_ONBUFFERINGSTATECHANGE RPC message.
  void IssuesBufferingStateRpc(BufferingState state) {
    std::optional<openscreen::cast::RendererClientOnBufferingStateChange::State>
        pb_state = media::cast::ToProtoMediaBufferingState(state);
    if (!pb_state.has_value())
      return;
    std::unique_ptr<openscreen::cast::RpcMessage> rpc(
        new openscreen::cast::RpcMessage());
    rpc->set_handle(5);
    rpc->set_proc(openscreen::cast::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE);
    auto* buffering_state =
        rpc->mutable_rendererclient_onbufferingstatechange_rpc();
    buffering_state->set_state(pb_state.value());
    OnReceivedRpc(std::move(rpc));
    RunPendingTasks();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<RendererController> controller_;
  std::unique_ptr<RendererClientImpl> render_client_;
  std::unique_ptr<FakeMediaResource> media_resource_;
  std::unique_ptr<CourierRenderer> renderer_;
  base::SimpleTestTickClock clock_;

  // RPC handles.
  const int receiver_renderer_handle_{10};
  const int receiver_audio_demuxer_callback_handle_{11};
  const int receiver_video_demuxer_callback_handle_{12};
  int sender_renderer_handle_;
  int sender_client_handle_{RpcMessenger::kInvalidHandle};
  int sender_renderer_callback_handle_{RpcMessenger::kInvalidHandle};
  int sender_audio_demuxer_handle_{RpcMessenger::kInvalidHandle};
  int sender_video_demuxer_handle_{RpcMessenger::kInvalidHandle};

  // Indicates whether the test runs in backward-compatible mode.
  bool is_backward_compatible_mode_ = false;

  // Indicates whether the demuxer at receiver is initialized or not.
  bool is_receiver_demuxer_initialized_ = false;

  // Indicate whether RPC_DS_INITIALIZE_CALLBACK RPC messages are received.
  bool received_audio_ds_init_cb_ = false;
  bool received_video_ds_init_cb_ = false;

  // Indicates whether the test wants to simulate successful initialization in
  // the renderer on the receiver side.
  bool is_successfully_initialized_ = true;

  // Stores RPC messages that have been sent to the remote sink.
  std::vector<openscreen::cast::RpcMessage> received_rpc_;
};

TEST_F(CourierRendererTest, Initialize) {
  InitializeRenderer();
  RunPendingTasks();

  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);
}

TEST_F(CourierRendererTest, InitializeBackwardCompatible) {
  InitializeRendererBackwardsCompatible();
  RunPendingTasks();

  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);
}

TEST_F(CourierRendererTest, InitializeFailed) {
  is_successfully_initialized_ = false;
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_FALSE(IsRendererInitialized());
  ASSERT_TRUE(DidEncounterFatalError());
  // Don't report error to prevent breaking the pipeline.
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);

  // The CourierRenderer should act as a no-op renderer from this point.

  ResetReceivedRpcMessage();
  EXPECT_CALL(*render_client_, OnFlushCallback()).Times(1);
  renderer_->Flush(base::BindOnce(&RendererClientImpl::OnFlushCallback,
                                  base::Unretained(render_client_.get())));
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  base::TimeDelta seek = base::Microseconds(100);
  renderer_->StartPlayingFrom(seek);
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  renderer_->SetVolume(3.0);
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  renderer_->SetPlaybackRate(2.5);
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());
}

TEST_F(CourierRendererTest, Flush) {
  // Initialize Renderer.
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);

  // Flush Renderer.
  // Redirect RPC message for simulate receiver scenario
  controller_->GetRpcMessenger()->set_send_message_cb_for_testing(
      [this](std::vector<uint8_t> message) {
        this->RpcMessageResponseBot(message);
      });
  RunPendingTasks();
  EXPECT_CALL(*render_client_, OnFlushCallback()).Times(1);
  renderer_->Flush(base::BindOnce(&RendererClientImpl::OnFlushCallback,
                                  base::Unretained(render_client_.get())));
  RunPendingTasks();
}

TEST_F(CourierRendererTest, StartPlayingFrom) {
  // Initialize Renderer
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);

  // StartPlaying from
  base::TimeDelta seek = base::Microseconds(100);
  renderer_->StartPlayingFrom(seek);
  RunPendingTasks();

  // Checks if it sends out RPC message with correct value.
  ASSERT_EQ(1, ReceivedRpcMessageCount());
  const openscreen::cast::RpcMessage* rpc =
      PeekRpcMessage(openscreen::cast::RpcMessage::RPC_R_STARTPLAYINGFROM);
  ASSERT_TRUE(rpc);
  ASSERT_EQ(100, rpc->integer64_value());
}

TEST_F(CourierRendererTest, SetVolume) {
  // Initialize Renderer because, as of this writing, the pipeline guarantees it
  // will not call SetVolume() until after the media::Renderer is initialized.
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  // SetVolume() will send openscreen::cast::RpcMessage::RPC_R_SETVOLUME RPC.
  renderer_->SetVolume(3.0);
  RunPendingTasks();

  // Checks if it sends out RPC message with correct value.
  ASSERT_EQ(1, ReceivedRpcMessageCount());
  const openscreen::cast::RpcMessage* rpc =
      PeekRpcMessage(openscreen::cast::RpcMessage::RPC_R_SETVOLUME);
  ASSERT_TRUE(rpc);
  ASSERT_DOUBLE_EQ(3.0, rpc->double_value());
}

TEST_F(CourierRendererTest, SetPlaybackRate) {
  // Initialize Renderer because, as of this writing, the pipeline guarantees it
  // will not call SetPlaybackRate() until after the media::Renderer is
  // initialized.
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  renderer_->SetPlaybackRate(2.5);
  RunPendingTasks();
  ASSERT_EQ(1, ReceivedRpcMessageCount());
  // Checks if it sends out RPC message with correct value.
  const openscreen::cast::RpcMessage* rpc =
      PeekRpcMessage(openscreen::cast::RpcMessage::RPC_R_SETPLAYBACKRATE);
  ASSERT_TRUE(rpc);
  ASSERT_DOUBLE_EQ(2.5, rpc->double_value());
}

TEST_F(CourierRendererTest, OnTimeUpdate) {
  base::TimeDelta media_time = base::Microseconds(100);
  base::TimeDelta max_media_time = base::Microseconds(500);
  IssueTimeUpdateRpc(media_time, max_media_time);
  ValidateCurrentTime(media_time, max_media_time);

  // Issues RPC_RC_ONTIMEUPDATE RPC message with invalid time
  base::TimeDelta media_time2 = base::Microseconds(-100);
  base::TimeDelta max_media_time2 = base::Microseconds(500);
  IssueTimeUpdateRpc(media_time2, max_media_time2);
  // Because of invalid value, the time will not be updated and remain the same.
  ValidateCurrentTime(media_time, max_media_time);
}

TEST_F(CourierRendererTest, OnBufferingStateChange) {
  InitializeRenderer();
  EXPECT_CALL(*render_client_,
              OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _))
      .Times(1);
  IssuesBufferingStateRpc(BufferingState::BUFFERING_HAVE_NOTHING);
}

TEST_F(CourierRendererTest, OnAudioConfigChange) {
  const AudioDecoderConfig kNewAudioConfig(
      AudioCodec::kVorbis, kSampleFormatPlanarF32, CHANNEL_LAYOUT_STEREO, 44100,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  InitializeRenderer();
  // Make sure initial audio config does not match the one we intend to send.
  ASSERT_FALSE(render_client_->audio_decoder_config().Matches(kNewAudioConfig));
  // Issues RPC_RC_ONVIDEOCONFIGCHANGE RPC message.
  EXPECT_CALL(*render_client_,
              OnAudioConfigChange(DecoderConfigEq(kNewAudioConfig)))
      .Times(1);

  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_RC_ONAUDIOCONFIGCHANGE);
  auto* audio_config_change_message =
      rpc->mutable_rendererclient_onaudioconfigchange_rpc();
  openscreen::cast::AudioDecoderConfig* proto_audio_config =
      audio_config_change_message->mutable_audio_decoder_config();
  media::cast::ConvertAudioDecoderConfigToProto(kNewAudioConfig,
                                                proto_audio_config);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_TRUE(render_client_->audio_decoder_config().Matches(kNewAudioConfig));
}

TEST_F(CourierRendererTest, OnVideoConfigChange) {
  const auto kNewVideoConfig = TestVideoConfig::Normal();
  InitializeRenderer();
  // Make sure initial video config does not match the one we intend to send.
  ASSERT_FALSE(render_client_->video_decoder_config().Matches(kNewVideoConfig));
  // Issues RPC_RC_ONVIDEOCONFIGCHANGE RPC message.
  EXPECT_CALL(*render_client_,
              OnVideoConfigChange(DecoderConfigEq(kNewVideoConfig)))
      .Times(1);

  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_RC_ONVIDEOCONFIGCHANGE);
  auto* video_config_change_message =
      rpc->mutable_rendererclient_onvideoconfigchange_rpc();
  openscreen::cast::VideoDecoderConfig* proto_video_config =
      video_config_change_message->mutable_video_decoder_config();
  media::cast::ConvertVideoDecoderConfigToProto(kNewVideoConfig,
                                                proto_video_config);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_TRUE(render_client_->video_decoder_config().Matches(kNewVideoConfig));
}

TEST_F(CourierRendererTest, OnVideoNaturalSizeChange) {
  InitializeRenderer();
  // Makes sure initial value of video natural size is not set to
  // gfx::Size(100, 200).
  ASSERT_NE(render_client_->size().width(), 100);
  ASSERT_NE(render_client_->size().height(), 200);
  // Issues RPC_RC_ONVIDEONATURALSIZECHANGE RPC message.
  EXPECT_CALL(*render_client_, OnVideoNaturalSizeChange(gfx::Size(100, 200)))
      .Times(1);
  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* size_message =
      rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  size_message->set_width(100);
  size_message->set_height(200);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_EQ(render_client_->size().width(), 100);
  ASSERT_EQ(render_client_->size().height(), 200);
}

TEST_F(CourierRendererTest, OnVideoNaturalSizeChangeWithInvalidValue) {
  InitializeRenderer();
  // Issues RPC_RC_ONVIDEONATURALSIZECHANGE RPC message.
  EXPECT_CALL(*render_client_, OnVideoNaturalSizeChange(_)).Times(0);
  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* size_message =
      rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  size_message->set_width(-100);
  size_message->set_height(0);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
}

TEST_F(CourierRendererTest, OnVideoOpacityChange) {
  InitializeRenderer();
  ASSERT_FALSE(render_client_->opaque());
  // Issues RPC_RC_ONVIDEOOPACITYCHANGE RPC message.
  EXPECT_CALL(*render_client_, OnVideoOpacityChange(true)).Times(1);
  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE);
  rpc->set_boolean_value(true);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_TRUE(render_client_->opaque());
}

TEST_F(CourierRendererTest, OnStatisticsUpdate) {
  InitializeRenderer();
  EXPECT_NE(DefaultStats(), render_client_->stats());
  IssueStatisticsUpdateRpc();
  EXPECT_EQ(DefaultStats(), render_client_->stats());
}

TEST_F(CourierRendererTest, OnPacingTooSlowly) {
  InitializeRenderer();
  RewireSendMessageCallbackToSink();

  // There should be no error reported with this playback rate.
  renderer_->SetPlaybackRate(0.8);
  RunPendingTasks();
  EXPECT_CALL(*render_client_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH, _))
      .Times(1);
  IssuesBufferingStateRpc(BufferingState::BUFFERING_HAVE_ENOUGH);
  clock_.Advance(base::Seconds(3));
  VerifyAndReportTimeUpdates(0, 15);
  ASSERT_FALSE(DidEncounterFatalError());

  // Change playback rate. Pacing keeps same as above. Should report error when
  // playback was continuously delayed for 10 times.
  renderer_->SetPlaybackRate(1);
  RunPendingTasks();
  clock_.Advance(base::Seconds(3));
  VerifyAndReportTimeUpdates(15, 30);
  ASSERT_TRUE(DidEncounterFatalError());
}

TEST_F(CourierRendererTest, OnFrameDropRateHigh) {
  InitializeRenderer();

  for (int i = 0; i < 7; ++i) {
    ASSERT_FALSE(DidEncounterFatalError());  // Not enough measurements.
    IssueStatisticsUpdateRpc();
    clock_.Advance(base::Seconds(1));
    RunPendingTasks();
  }
  ASSERT_TRUE(DidEncounterFatalError());
}

}  // namespace remoting
}  // namespace media

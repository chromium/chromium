// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/end2end_test_renderer.h"
#include "base/memory/raw_ptr.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/cast/openscreen/decoder_buffer_reader.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/remoting/courier_renderer.h"
#include "media/remoting/receiver.h"
#include "media/remoting/receiver_controller.h"
#include "media/remoting/renderer_controller.h"
#include "media/remoting/stream_provider.h"
#include "media/remoting/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"

using openscreen::cast::RpcMessenger;

namespace media {
namespace remoting {

namespace {

class TestStreamSender final : public mojom::RemotingDataStreamSender {
 public:
  using SendFrameToSinkCallback =
      base::RepeatingCallback<void(uint32_t frame_count,
                                   scoped_refptr<media::DecoderBuffer>,
                                   DemuxerStream::Type)>;
  TestStreamSender(
      mojo::PendingReceiver<mojom::RemotingDataStreamSender> receiver,
      mojo::ScopedDataPipeConsumerHandle handle,
      DemuxerStream::Type type,
      SendFrameToSinkCallback callback)
      : receiver_(this, std::move(receiver)),
        decoder_buffer_reader_(
            std::make_unique<media::cast::DecoderBufferReader>(
                base::BindRepeating(&TestStreamSender::OnFrameRead,
                                    base::Unretained(this)),
                std::move(handle))),
        type_(type),
        send_frame_to_sink_cb_(std::move(callback)) {
    decoder_buffer_reader_->ReadBufferAsync();
  }

  TestStreamSender(const TestStreamSender&) = delete;
  TestStreamSender& operator=(const TestStreamSender&) = delete;

  ~TestStreamSender() override = default;

  // mojom::RemotingDataStreamSender implementation.
  void SendFrame(media::mojom::DecoderBufferPtr buffer,
                 SendFrameCallback callback) override {
    DCHECK(decoder_buffer_reader_);
    DCHECK(!read_complete_cb_);
    read_complete_cb_ = std::move(callback);
    decoder_buffer_reader_->ProvideBuffer(std::move(buffer));
  }

  void CancelInFlightData() override {}

 private:
  void OnFrameRead(scoped_refptr<media::DecoderBuffer> buffer) {
    DCHECK(decoder_buffer_reader_);
    DCHECK(read_complete_cb_);
    DCHECK(buffer);

    std::move(read_complete_cb_).Run();

    if (send_frame_to_sink_cb_) {
      send_frame_to_sink_cb_.Run(frame_count_++, std::move(buffer), type_);
    }
    decoder_buffer_reader_->ReadBufferAsync();
  }

  uint32_t frame_count_ = 0;
  mojo::Receiver<RemotingDataStreamSender> receiver_;
  std::unique_ptr<media::cast::DecoderBufferReader> decoder_buffer_reader_;
  SendFrameCallback read_complete_cb_;
  const DemuxerStream::Type type_;
  const SendFrameToSinkCallback send_frame_to_sink_cb_;
};

class TestRemoter final : public mojom::Remoter {
 public:
  using SendMessageToSinkCallback =
      base::RepeatingCallback<void(const std::vector<uint8_t>& message)>;
  TestRemoter(mojo::PendingRemote<mojom::RemotingSource> source,
              SendMessageToSinkCallback send_message_to_sink_cb,
              TestStreamSender::SendFrameToSinkCallback send_frame_to_sink_cb)
      : source_(std::move(source)),
        send_message_to_sink_cb_(std::move(send_message_to_sink_cb)),
        send_frame_to_sink_cb_(std::move(send_frame_to_sink_cb)) {}

  TestRemoter(const TestRemoter&) = delete;
  TestRemoter& operator=(const TestRemoter&) = delete;

  ~TestRemoter() override = default;

  // mojom::Remoter implementation.
  void Start() override { source_->OnStarted(); }
  void StartWithPermissionAlreadyGranted() override { source_->OnStarted(); }

  void StartDataStreams(mojo::ScopedDataPipeConsumerHandle audio_pipe,
                        mojo::ScopedDataPipeConsumerHandle video_pipe,
                        mojo::PendingReceiver<mojom::RemotingDataStreamSender>
                            audio_sender_receiver,
                        mojo::PendingReceiver<mojom::RemotingDataStreamSender>
                            video_sender_receiver) override {
    if (audio_pipe.is_valid()) {
      audio_stream_sender_ = std::make_unique<TestStreamSender>(
          std::move(audio_sender_receiver), std::move(audio_pipe),
          DemuxerStream::AUDIO, send_frame_to_sink_cb_);
    }
    if (video_pipe.is_valid()) {
      video_stream_sender_ = std::make_unique<TestStreamSender>(
          std::move(video_sender_receiver), std::move(video_pipe),
          DemuxerStream::VIDEO, send_frame_to_sink_cb_);
    }
  }

  void Stop(mojom::RemotingStopReason reason) override {
    source_->OnStopped(reason);
  }

  void SendMessageToSink(const std::vector<uint8_t>& message) override {
    if (send_message_to_sink_cb_)
      send_message_to_sink_cb_.Run(message);
  }

  void EstimateTransmissionCapacity(
      mojom::Remoter::EstimateTransmissionCapacityCallback callback) override {
    std::move(callback).Run(0);
  }

  // Called when receives RPC messages from receiver.
  void OnMessageFromSink(const std::vector<uint8_t>& message) {
    source_->OnMessageFromSink(message);
  }

 private:
  mojo::Remote<mojom::RemotingSource> source_;
  const SendMessageToSinkCallback send_message_to_sink_cb_;
  const TestStreamSender::SendFrameToSinkCallback send_frame_to_sink_cb_;
  std::unique_ptr<TestStreamSender> audio_stream_sender_;
  std::unique_ptr<TestStreamSender> video_stream_sender_;
};

std::unique_ptr<RendererController> CreateController(
    TestRemoter::SendMessageToSinkCallback send_message_to_sink_cb,
    TestStreamSender::SendFrameToSinkCallback send_frame_to_sink_cb) {
  mojo::PendingRemote<mojom::RemotingSource> remoting_source;
  auto remoting_source_receiver =
      remoting_source.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::Remoter> remoter;
  std::unique_ptr<TestRemoter> test_remoter = std::make_unique<TestRemoter>(
      std::move(remoting_source), std::move(send_message_to_sink_cb),
      std::move(send_frame_to_sink_cb));
  mojo::MakeSelfOwnedReceiver(std::move(test_remoter),
                              remoter.InitWithNewPipeAndPassReceiver());
  return std::make_unique<RendererController>(
      std::move(remoting_source_receiver), std::move(remoter));
}

}  // namespace

class End2EndTestRenderer::TestRemotee : public mojom::Remotee {
 public:
  explicit TestRemotee(RendererController* controller)
      : controller_(controller) {}

  ~TestRemotee() override = default;

  void OnAudioFrame(uint32_t frame_count,
                    scoped_refptr<DecoderBuffer> decoder_buffer) {
    DCHECK(decoder_buffer);
    ::media::mojom::DecoderBufferPtr mojo_buffer =
        audio_buffer_writer_->WriteDecoderBuffer(std::move(decoder_buffer));
    audio_stream_->ReceiveFrame(frame_count, std::move(mojo_buffer));
  }

  void OnVideoFrame(uint32_t frame_count,
                    scoped_refptr<DecoderBuffer> decoder_buffer) {
    DCHECK(decoder_buffer);
    ::media::mojom::DecoderBufferPtr mojo_buffer =
        video_buffer_writer_->WriteDecoderBuffer(std::move(decoder_buffer));
    video_stream_->ReceiveFrame(frame_count, std::move(mojo_buffer));
  }

  void BindMojoReceiver(mojo::PendingReceiver<mojom::Remotee> receiver) {
    mojo_receiver_.Bind(std::move(receiver));
  }

  void OnMessage(const std::vector<uint8_t>& message) {
    receiver_controller_->OnMessageFromSource(message);
  }

  // mojom::Remotee implementation
  void OnRemotingSinkReady(
      mojo::PendingRemote<::media::mojom::RemotingSink> sink) override {
    receiver_controller_.Bind(std::move(sink));
  }

  void SendMessageToSource(const std::vector<uint8_t>& message) override {
    controller_->OnMessageFromSink(message);
  }

  void StartDataStreams(
      mojo::PendingRemote<::media::mojom::RemotingDataStreamReceiver>
          audio_stream,
      mojo::PendingRemote<::media::mojom::RemotingDataStreamReceiver>
          video_stream) override {
    if (audio_stream.is_valid()) {
      // initialize data pipe for audio data stream receiver
      mojo::ScopedDataPipeConsumerHandle audio_data_pipe;
      audio_stream_.Bind(std::move(audio_stream));
      audio_buffer_writer_ = ::media::MojoDecoderBufferWriter::Create(
          GetDefaultDecoderBufferConverterCapacity(
              ::media::DemuxerStream::AUDIO),
          &audio_data_pipe);
      audio_stream_->InitializeDataPipe(std::move(audio_data_pipe));
    }

    if (video_stream.is_valid()) {
      // initialize data pipe for video data stream receiver
      mojo::ScopedDataPipeConsumerHandle video_data_pipe;
      video_stream_.Bind(std::move(video_stream));
      video_buffer_writer_ = ::media::MojoDecoderBufferWriter::Create(
          GetDefaultDecoderBufferConverterCapacity(
              ::media::DemuxerStream::VIDEO),
          &video_data_pipe);
      video_stream_->InitializeDataPipe(std::move(video_data_pipe));
    }
  }

  void OnFlushUntil(uint32_t audio_frame_count,
                    uint32_t video_frame_count) override {}

  void OnVideoNaturalSizeChange(const gfx::Size& size) override {}

 private:
  raw_ptr<RendererController> controller_;

  std::unique_ptr<MojoDecoderBufferWriter> audio_buffer_writer_;
  std::unique_ptr<MojoDecoderBufferWriter> video_buffer_writer_;

  mojo::Remote<mojom::RemotingDataStreamReceiver> audio_stream_;
  mojo::Remote<mojom::RemotingDataStreamReceiver> video_stream_;

  mojo::Remote<mojom::RemotingSink> receiver_controller_;
  mojo::Receiver<mojom::Remotee> mojo_receiver_{this};
};

End2EndTestRenderer::End2EndTestRenderer(std::unique_ptr<Renderer> renderer)
    : courier_renderer_initialized_(false), receiver_initialized_(false) {
  // create sender components
  controller_ = CreateController(
      base::BindRepeating(&End2EndTestRenderer::SendMessageToSink,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&End2EndTestRenderer::SendFrameToSink,
                          weak_factory_.GetWeakPtr()));
  courier_renderer_ = std::make_unique<CourierRenderer>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      controller_->GetWeakPtr(), nullptr);

  // create receiver components
  media_remotee_ = std::make_unique<TestRemotee>(controller_.get());

  receiver_controller_ = ReceiverController::GetInstance();
  ResetForTesting(receiver_controller_);

  receiver_rpc_messenger_ = receiver_controller_->rpc_messenger();
  receiver_renderer_handle_ = receiver_rpc_messenger_->GetUniqueHandle();

  receiver_rpc_messenger_->RegisterMessageReceiverCallback(
      RpcMessenger::kAcquireRendererHandle,
      [ptr = weak_factory_.GetWeakPtr()](
          std::unique_ptr<openscreen::cast::RpcMessage> message) {
        if (ptr) {
          ptr->OnReceivedRpc(std::move(message));
        }
      });

  receiver_ = std::make_unique<Receiver>(
      receiver_renderer_handle_, sender_renderer_handle_, receiver_controller_,
      base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(renderer),
      base::BindOnce(&End2EndTestRenderer::OnAcquireRendererDone,
                     weak_factory_.GetWeakPtr()));

  mojo::PendingRemote<media::mojom::Remotee> remotee;
  media_remotee_->BindMojoReceiver(remotee.InitWithNewPipeAndPassReceiver());
  receiver_controller_->Initialize(std::move(remotee));
  stream_provider_ = std::make_unique<StreamProvider>(
      receiver_controller_, base::SingleThreadTaskRunner::GetCurrentDefault());
}

End2EndTestRenderer::~End2EndTestRenderer() {
  receiver_rpc_messenger_->UnregisterMessageReceiverCallback(
      RpcMessenger::kAcquireRendererHandle);
}

void End2EndTestRenderer::Initialize(MediaResource* media_resource,
                                     RendererClient* client,
                                     PipelineStatusCallback init_cb) {
  init_cb_ = std::move(init_cb);

  stream_provider_->Initialize(
      nullptr, base::BindOnce(&End2EndTestRenderer::InitializeReceiverRenderer,
                              weak_factory_.GetWeakPtr()));

  courier_renderer_->Initialize(
      media_resource, client,
      base::BindOnce(&End2EndTestRenderer::OnCourierRendererInitialized,
                     weak_factory_.GetWeakPtr()));
}

void End2EndTestRenderer::InitializeReceiverRenderer(PipelineStatus status) {
  DCHECK(status == PIPELINE_OK);
  receiver_->Initialize(
      stream_provider_.get(), nullptr,
      base::BindOnce(&End2EndTestRenderer::OnReceiverInitialized,
                     weak_factory_.GetWeakPtr()));
}

void End2EndTestRenderer::OnCourierRendererInitialized(PipelineStatus status) {
  DCHECK(status == PIPELINE_OK);
  courier_renderer_initialized_ = true;
  CompleteInitialize();
}

void End2EndTestRenderer::OnReceiverInitialized(PipelineStatus status) {
  DCHECK(status == PIPELINE_OK);
  receiver_initialized_ = true;
  CompleteInitialize();
}
void End2EndTestRenderer::CompleteInitialize() {
  if (!courier_renderer_initialized_ || !receiver_initialized_)
    return;

  DCHECK(init_cb_);
  std::move(init_cb_).Run(PIPELINE_OK);
}

void End2EndTestRenderer::OnReceivedRpc(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(message);
  DCHECK_EQ(message->proc(),
            openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER);
  OnAcquireRenderer(std::move(message));
}

void End2EndTestRenderer::OnAcquireRenderer(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(message->has_integer_value());
  DCHECK(message->integer_value() != RpcMessenger::kInvalidHandle);

  if (sender_renderer_handle_ == RpcMessenger::kInvalidHandle) {
    sender_renderer_handle_ = message->integer_value();
    receiver_->SetRemoteHandle(sender_renderer_handle_);
  }
}

void End2EndTestRenderer::OnAcquireRendererDone(int receiver_renderer_handle) {
  openscreen::cast::RpcMessage rpc;
  rpc.set_handle(sender_renderer_handle_);
  rpc.set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER_DONE);
  rpc.set_integer_value(receiver_renderer_handle);
  receiver_rpc_messenger_->SendMessageToRemote(rpc);
}

void End2EndTestRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  courier_renderer_->SetLatencyHint(latency_hint);
}

void End2EndTestRenderer::SetPreservesPitch(bool preserves_pitch) {
  courier_renderer_->SetPreservesPitch(preserves_pitch);
}

void End2EndTestRenderer::Flush(base::OnceClosure flush_cb) {
  courier_renderer_->Flush(std::move(flush_cb));
}

void End2EndTestRenderer::StartPlayingFrom(base::TimeDelta time) {
  courier_renderer_->StartPlayingFrom(time);
}

void End2EndTestRenderer::SetPlaybackRate(double playback_rate) {
  courier_renderer_->SetPlaybackRate(playback_rate);
}

void End2EndTestRenderer::SetVolume(float volume) {
  courier_renderer_->SetVolume(volume);
}

base::TimeDelta End2EndTestRenderer::GetMediaTime() {
  return courier_renderer_->GetMediaTime();
}

RendererType End2EndTestRenderer::GetRendererType() {
  return RendererType::kTest;
}

void End2EndTestRenderer::SendMessageToSink(
    const std::vector<uint8_t>& message) {
  media_remotee_->OnMessage(message);
}

void End2EndTestRenderer::SendFrameToSink(
    uint32_t frame_count,
    scoped_refptr<media::DecoderBuffer> decoder_buffer,
    DemuxerStream::Type type) {
  DCHECK(decoder_buffer);

  if (type == DemuxerStream::Type::AUDIO) {
    media_remotee_->OnAudioFrame(frame_count, decoder_buffer);
  } else if (type == DemuxerStream::Type::VIDEO) {
    media_remotee_->OnVideoFrame(frame_count, decoder_buffer);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void End2EndTestRenderer::OnMessageFromSink(
    std::unique_ptr<std::vector<uint8_t>> message) {
  controller_->OnMessageFromSink(*message);
}

void End2EndTestRenderer::OnSelectedVideoTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  courier_renderer_->OnSelectedVideoTracksChanged(
      enabled_tracks, std::move(change_completed_cb));
}

void End2EndTestRenderer::OnEnabledAudioTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  courier_renderer_->OnEnabledAudioTracksChanged(
      enabled_tracks, std::move(change_completed_cb));
}

}  // namespace remoting
}  // namespace media

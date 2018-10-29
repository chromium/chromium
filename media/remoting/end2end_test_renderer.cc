// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/end2end_test_renderer.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "media/remoting/courier_renderer.h"
#include "media/remoting/proto_utils.h"
#include "media/remoting/receiver.h"
#include "media/remoting/renderer_controller.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace media {
namespace remoting {

namespace {

class TestStreamSender final : public mojom::RemotingDataStreamSender {
 public:
  using SendFrameToSinkCallback =
      base::Callback<void(const std::vector<uint8_t>& data,
                          DemuxerStream::Type type)>;
  TestStreamSender(mojom::RemotingDataStreamSenderRequest request,
                   mojo::ScopedDataPipeConsumerHandle handle,
                   DemuxerStream::Type type,
                   const SendFrameToSinkCallback& callback)
      : binding_(this, std::move(request)),
        data_pipe_reader_(std::move(handle)),
        type_(type),
        send_frame_to_sink_cb_(callback) {}

  ~TestStreamSender() override = default;

  // mojom::RemotingDataStreamSender implementation.
  void SendFrame(uint32_t frame_size) override {
    next_frame_data_.resize(frame_size);
    data_pipe_reader_.Read(
        next_frame_data_.data(), frame_size,
        base::BindOnce(&TestStreamSender::OnFrameRead, base::Unretained(this)));
  }

  void CancelInFlightData() override { next_frame_data_.resize(0); }

 private:
  void OnFrameRead(bool success) {
    DCHECK(success);
    if (send_frame_to_sink_cb_)
      send_frame_to_sink_cb_.Run(next_frame_data_, type_);
    next_frame_data_.resize(0);
  }

  mojo::Binding<RemotingDataStreamSender> binding_;
  MojoDataPipeReader data_pipe_reader_;
  const DemuxerStream::Type type_;
  const SendFrameToSinkCallback send_frame_to_sink_cb_;
  std::vector<uint8_t> next_frame_data_;

  DISALLOW_COPY_AND_ASSIGN(TestStreamSender);
};

class TestRemoter final : public mojom::Remoter {
 public:
  using SendMessageToSinkCallback =
      base::RepeatingCallback<void(const std::vector<uint8_t>& message)>;
  TestRemoter(
      mojom::RemotingSourcePtr source,
      const SendMessageToSinkCallback& send_message_to_sink_cb,
      const TestStreamSender::SendFrameToSinkCallback& send_frame_to_sink_cb)
      : source_(std::move(source)),
        send_message_to_sink_cb_(send_message_to_sink_cb),
        send_frame_to_sink_cb_(send_frame_to_sink_cb) {}

  ~TestRemoter() override = default;

  // mojom::Remoter implementation.

  void Start() override { source_->OnStarted(); }

  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojom::RemotingDataStreamSenderRequest audio_sender_request,
      mojom::RemotingDataStreamSenderRequest video_sender_request) override {
    if (audio_pipe.is_valid()) {
      audio_stream_sender_.reset(new TestStreamSender(
          std::move(audio_sender_request), std::move(audio_pipe),
          DemuxerStream::AUDIO, send_frame_to_sink_cb_));
    }
    if (video_pipe.is_valid()) {
      video_stream_sender_.reset(new TestStreamSender(
          std::move(video_sender_request), std::move(video_pipe),
          DemuxerStream::VIDEO, send_frame_to_sink_cb_));
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
  mojom::RemotingSourcePtr source_;
  const SendMessageToSinkCallback send_message_to_sink_cb_;
  const TestStreamSender::SendFrameToSinkCallback send_frame_to_sink_cb_;
  std::unique_ptr<TestStreamSender> audio_stream_sender_;
  std::unique_ptr<TestStreamSender> video_stream_sender_;

  DISALLOW_COPY_AND_ASSIGN(TestRemoter);
};

std::unique_ptr<RendererController> CreateController(
    const TestRemoter::SendMessageToSinkCallback& send_message_to_sink_cb,
    const TestStreamSender::SendFrameToSinkCallback& send_frame_to_sink_cb) {
  mojom::RemotingSourcePtr remoting_source;
  auto remoting_source_request = mojo::MakeRequest(&remoting_source);
  mojom::RemoterPtr remoter;
  std::unique_ptr<TestRemoter> test_remoter = std::make_unique<TestRemoter>(
      std::move(remoting_source), send_message_to_sink_cb,
      send_frame_to_sink_cb);
  mojo::MakeStrongBinding(std::move(test_remoter), mojo::MakeRequest(&remoter));
  return std::make_unique<RendererController>(
      std::move(remoting_source_request), std::move(remoter));
}

}  // namespace

End2EndTestRenderer::End2EndTestRenderer(std::unique_ptr<Renderer> renderer)
    : receiver_rpc_broker_(
          base::BindRepeating(&End2EndTestRenderer::OnMessageFromSink,
                              base::Unretained(this))),
      receiver_(new Receiver(std::move(renderer), &receiver_rpc_broker_)),
      weak_factory_(this) {
  controller_ = CreateController(
      base::BindRepeating(&End2EndTestRenderer::SendMessageToSink,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&End2EndTestRenderer::SendFrameToSink,
                          weak_factory_.GetWeakPtr()));
  courier_renderer_.reset(new CourierRenderer(
      base::ThreadTaskRunnerHandle::Get(), controller_->GetWeakPtr(), nullptr));
}

End2EndTestRenderer::~End2EndTestRenderer() = default;

void End2EndTestRenderer::Initialize(MediaResource* media_resource,
                                     RendererClient* client,
                                     const PipelineStatusCB& init_cb) {
  courier_renderer_->Initialize(media_resource, client, init_cb);
}

void End2EndTestRenderer::SetCdm(CdmContext* cdm_context,
                                 const CdmAttachedCB& cdc_attached_cb) {
  // TODO(xjz): Add the implementation when media remoting starts supporting
  // encrypted contents.
  NOTIMPLEMENTED() << "Media Remoting doesn't support EME for now.";
}

void End2EndTestRenderer::Flush(const base::Closure& flush_cb) {
  courier_renderer_->Flush(flush_cb);
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

void End2EndTestRenderer::SendMessageToSink(
    const std::vector<uint8_t>& message) {
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  if (!rpc->ParseFromArray(message.data(), message.size())) {
    VLOG(1) << __func__ << ": Received corrupted Rpc message.";
    return;
  }
  receiver_rpc_broker_.ProcessMessageFromRemote(std::move(rpc));
}

void End2EndTestRenderer::SendFrameToSink(const std::vector<uint8_t>& frame,
                                          DemuxerStream::Type type) {
  scoped_refptr<DecoderBuffer> decoder_buffer =
      ByteArrayToDecoderBuffer(frame.data(), frame.size());
  receiver_->OnReceivedBuffer(type, decoder_buffer);
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

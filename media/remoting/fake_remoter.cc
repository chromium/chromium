// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/fake_remoter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "build/buildflag.h"
#include "media/media_buildflags.h"
#include "media/remoting/renderer_controller.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
#include "components/cast_streaming/public/remoting_proto_utils.h"  // nogncheck
#endif

namespace media {
namespace remoting {

FakeRemotingDataStreamSender::FakeRemotingDataStreamSender(
    mojo::PendingReceiver<mojom::RemotingDataStreamSender> stream_sender,
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : receiver_(this, std::move(stream_sender)),
      data_pipe_reader_(std::move(consumer_handle)),
      send_frame_count_(0),
      cancel_in_flight_count_(0) {}

FakeRemotingDataStreamSender::~FakeRemotingDataStreamSender() = default;

void FakeRemotingDataStreamSender::ResetHistory() {
  send_frame_count_ = 0;
  cancel_in_flight_count_ = 0;
  next_frame_data_.resize(0);
  received_frame_list.clear();
}

bool FakeRemotingDataStreamSender::ValidateFrameBuffer(size_t index,
                                                       size_t size,
                                                       bool key_frame,
                                                       int pts_ms) {
  if (index >= received_frame_list.size()) {
    VLOG(1) << "There is no such frame";
    return false;
  }

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
  const std::vector<uint8_t>& data = received_frame_list[index];
  scoped_refptr<DecoderBuffer> media_buffer =
      cast_streaming::remoting::ByteArrayToDecoderBuffer(data.data(),
                                                         data.size());

  // Checks if pts is correct or not
  if (media_buffer->timestamp().InMilliseconds() != pts_ms) {
    VLOG(1) << "Pts should be:" << pts_ms << "("
            << media_buffer->timestamp().InMilliseconds() << ")";
    return false;
  }

  // Checks if key frame is set correct or not
  if (media_buffer->is_key_frame() != key_frame) {
    VLOG(1) << "Key frame should be:" << key_frame << "("
            << media_buffer->is_key_frame() << ")";
    return false;
  }

  // Checks if frame buffer size is correct or not
  if (media_buffer->data_size() != size) {
    VLOG(1) << "Buffer size should be:" << size << "("
            << media_buffer->data_size() << ")";
    return false;
  }

  // Checks if frame buffer is correct or not.
  bool return_value = true;
  const uint8_t* buffer = media_buffer->data();
  for (size_t i = 0; i < media_buffer->data_size(); ++i) {
    uint32_t value = static_cast<uint32_t>(i & 0xFF);
    if (value != static_cast<uint32_t>(buffer[i])) {
      VLOG(1) << "buffer index: " << i << " should be "
              << static_cast<uint32_t>(value) << " ("
              << static_cast<uint32_t>(buffer[i]) << ")";
      return_value = false;
    }
  }
  return return_value;
#else
  return true;
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
}

void FakeRemotingDataStreamSender::CloseDataPipe() {
  data_pipe_reader_.Close();
}

void FakeRemotingDataStreamSender::SendFrame(uint32_t frame_size) {
  next_frame_data_.resize(frame_size);
  data_pipe_reader_.Read(
      next_frame_data_.data(), frame_size,
      base::BindOnce(&FakeRemotingDataStreamSender::OnFrameRead,
                     base::Unretained(this)));
}

void FakeRemotingDataStreamSender::OnFrameRead(bool success) {
  if (!success)
    return;

  ++send_frame_count_;
  received_frame_list.push_back(std::move(next_frame_data_));
  EXPECT_EQ(send_frame_count_, received_frame_list.size());
}

void FakeRemotingDataStreamSender::CancelInFlightData() {
  ++cancel_in_flight_count_;
}

FakeRemoter::FakeRemoter(mojo::PendingRemote<mojom::RemotingSource> source,
                         bool start_will_fail)
    : source_(std::move(source)), start_will_fail_(start_will_fail) {}

FakeRemoter::~FakeRemoter() = default;

void FakeRemoter::Start() {
  if (start_will_fail_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeRemoter::StartFailed, weak_factory_.GetWeakPtr()));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeRemoter::Started, weak_factory_.GetWeakPtr()));
  }
}

void FakeRemoter::StartWithPermissionAlreadyGranted() {
  Start();
}

void FakeRemoter::StartDataStreams(
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<mojom::RemotingDataStreamSender> audio_sender,
    mojo::PendingReceiver<mojom::RemotingDataStreamSender> video_sender) {
  if (audio_pipe.is_valid()) {
    VLOG(2) << "Has audio";
    audio_stream_sender_ = std::make_unique<FakeRemotingDataStreamSender>(
        std::move(audio_sender), std::move(audio_pipe));
  }

  if (video_pipe.is_valid()) {
    VLOG(2) << "Has video";
    video_stream_sender_ = std::make_unique<FakeRemotingDataStreamSender>(
        std::move(video_sender), std::move(video_pipe));
  }
}

void FakeRemoter::Stop(mojom::RemotingStopReason reason) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeRemoter::Stopped,
                                weak_factory_.GetWeakPtr(), reason));
}

void FakeRemoter::SendMessageToSink(const std::vector<uint8_t>& message) {}

void FakeRemoter::EstimateTransmissionCapacity(
    mojom::Remoter::EstimateTransmissionCapacityCallback callback) {
  std::move(callback).Run(10000000 / 8.0);
}

void FakeRemoter::Started() {
  source_->OnStarted();
}

void FakeRemoter::StartFailed() {
  source_->OnStartFailed(mojom::RemotingStartFailReason::ROUTE_TERMINATED);
}

void FakeRemoter::Stopped(mojom::RemotingStopReason reason) {
  source_->OnStopped(reason);
}

FakeRemoterFactory::FakeRemoterFactory(bool start_will_fail)
    : start_will_fail_(start_will_fail) {}

FakeRemoterFactory::~FakeRemoterFactory() = default;

void FakeRemoterFactory::Create(
    mojo::PendingRemote<mojom::RemotingSource> source,
    mojo::PendingReceiver<mojom::Remoter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeRemoter>(std::move(source), start_will_fail_),
      std::move(receiver));
}

// static
std::unique_ptr<RendererController> FakeRemoterFactory::CreateController(
    bool start_will_fail) {
  mojo::PendingRemote<mojom::RemotingSource> remoting_source;
  auto remoting_source_receiver =
      remoting_source.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::Remoter> remoter;
  FakeRemoterFactory remoter_factory(start_will_fail);
  remoter_factory.Create(std::move(remoting_source),
                         remoter.InitWithNewPipeAndPassReceiver());
  return std::make_unique<RendererController>(
      std::move(remoting_source_receiver), std::move(remoter));
}

}  // namespace remoting
}  // namespace media

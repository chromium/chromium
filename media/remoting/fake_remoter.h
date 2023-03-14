// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_FAKE_REMOTER_H_
#define MEDIA_REMOTING_FAKE_REMOTER_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media::cast {
class DecoderBufferReader;
}  // namespace media::cast

namespace media::remoting {

class RendererController;

class FakeRemotingDataStreamSender : public mojom::RemotingDataStreamSender {
 public:
  FakeRemotingDataStreamSender(
      mojo::PendingReceiver<mojom::RemotingDataStreamSender> receiver,
      mojo::ScopedDataPipeConsumerHandle consumer_handle);

  FakeRemotingDataStreamSender(const FakeRemotingDataStreamSender&) = delete;
  FakeRemotingDataStreamSender& operator=(const FakeRemotingDataStreamSender&) =
      delete;

  ~FakeRemotingDataStreamSender() override;

  uint32_t send_frame_count() const { return send_frame_count_; }
  uint32_t cancel_in_flight_count() const { return cancel_in_flight_count_; }
  void ResetHistory();
  bool ValidateFrameBuffer(size_t index,
                           size_t size,
                           bool key_frame,
                           int pts_ms);
  void CloseDataPipe();

 private:
  // mojom::RemotingDataStreamSender implementation.
  void SendFrame(media::mojom::DecoderBufferPtr buffer,
                 SendFrameCallback callback) final;
  void CancelInFlightData() final;

  void OnFrameRead(scoped_refptr<media::DecoderBuffer> buffer);

  mojo::Receiver<RemotingDataStreamSender> receiver_;
  std::unique_ptr<media::cast::DecoderBufferReader> decoder_buffer_reader_;
  SendFrameCallback send_frame_callback_;

  std::vector<scoped_refptr<media::DecoderBuffer>> received_frame_list_;
  uint32_t send_frame_count_ = 0;
  uint32_t cancel_in_flight_count_ = 0;
};

class FakeRemoter final : public mojom::Remoter {
 public:
  // |start_will_fail| indicates whether starting remoting will fail.
  FakeRemoter(mojo::PendingRemote<mojom::RemotingSource> source,
              bool start_will_fail);

  FakeRemoter(const FakeRemoter&) = delete;
  FakeRemoter& operator=(const FakeRemoter&) = delete;

  ~FakeRemoter() override;

  // mojom::Remoter implementations.
  void Start() override;
  void StartWithPermissionAlreadyGranted() override;
  void StartDataStreams(mojo::ScopedDataPipeConsumerHandle audio_pipe,
                        mojo::ScopedDataPipeConsumerHandle video_pipe,
                        mojo::PendingReceiver<mojom::RemotingDataStreamSender>
                            audio_sender_receiver,
                        mojo::PendingReceiver<mojom::RemotingDataStreamSender>
                            video_sender_receiver) override;
  void Stop(mojom::RemotingStopReason reason) override;
  void SendMessageToSink(const std::vector<uint8_t>& message) override;
  void EstimateTransmissionCapacity(
      mojom::Remoter::EstimateTransmissionCapacityCallback callback) override;

 private:
  void Started();
  void StartFailed();
  void Stopped(mojom::RemotingStopReason reason);

  mojo::Remote<mojom::RemotingSource> source_;
  bool start_will_fail_;

  std::unique_ptr<FakeRemotingDataStreamSender> audio_stream_sender_;
  std::unique_ptr<FakeRemotingDataStreamSender> video_stream_sender_;

  base::WeakPtrFactory<FakeRemoter> weak_factory_{this};
};

class FakeRemoterFactory final : public mojom::RemoterFactory {
 public:
  // |start_will_fail| indicates whether starting remoting will fail.
  explicit FakeRemoterFactory(bool start_will_fail);

  FakeRemoterFactory(const FakeRemoterFactory&) = delete;
  FakeRemoterFactory& operator=(const FakeRemoterFactory&) = delete;

  ~FakeRemoterFactory() override;

  // mojom::RemoterFactory implementation.
  void Create(mojo::PendingRemote<mojom::RemotingSource> source,
              mojo::PendingReceiver<mojom::Remoter> receiver) override;

  static std::unique_ptr<RendererController> CreateController(
      bool start_will_fail);

 private:
  bool start_will_fail_;
};

}  // namespace media::remoting

#endif  // MEDIA_REMOTING_FAKE_REMOTER_H_

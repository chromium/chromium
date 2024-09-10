// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_END2END_TEST_RENDERER_H_
#define MEDIA_REMOTING_END2END_TEST_RENDERER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer_stream.h"
#include "media/base/renderer.h"
#include "media/remoting/stream_provider.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

namespace media {

class DecoderBuffer;

namespace remoting {

class RendererController;
class CourierRenderer;
class Receiver;
class ReceiverController;

// Simulates the media remoting pipeline.
class End2EndTestRenderer final : public Renderer {
 public:
  explicit End2EndTestRenderer(std::unique_ptr<Renderer> renderer);
  ~End2EndTestRenderer() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void SetPreservesPitch(bool preserves_pitch) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  RendererType GetRendererType() override;

  void OnSelectedVideoTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) override;

  void OnEnabledAudioTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) override;

 private:
  class TestRemotee;

  void InitTestApi();

  // Called to send RPC messages to |receiver_|.
  void SendMessageToSink(const std::vector<uint8_t>& message);

  // Called to send frame data to |receiver_|.
  void SendFrameToSink(uint32_t frame_count,
                       scoped_refptr<media::DecoderBuffer> decoder_buffer,
                       DemuxerStream::Type type);

  // Called when receives RPC messages from |receiver_|.
  void OnMessageFromSink(std::unique_ptr<std::vector<uint8_t>> message);

  void InitializeReceiverRenderer(PipelineStatus status);
  void OnCourierRendererInitialized(PipelineStatus status);
  void OnReceiverInitialized(PipelineStatus status);
  void CompleteInitialize();

  // Callback function when RPC message is received.
  void OnReceivedRpc(std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnAcquireRenderer(std::unique_ptr<openscreen::cast::RpcMessage> message);
  void OnAcquireRendererDone(int receiver_renderer_handle);

  PipelineStatusCallback init_cb_;

  bool courier_renderer_initialized_;
  bool receiver_initialized_;

  // Sender components.
  std::unique_ptr<RendererController> controller_;
  std::unique_ptr<CourierRenderer> courier_renderer_;

  // Receiver components.
  std::unique_ptr<TestRemotee> media_remotee_;
  raw_ptr<ReceiverController> receiver_controller_;
  std::unique_ptr<Receiver> receiver_;
  std::unique_ptr<StreamProvider> stream_provider_;
  raw_ptr<openscreen::cast::RpcMessenger> receiver_rpc_messenger_;

  // Handle of |receiver_|
  int receiver_renderer_handle_ =
      openscreen::cast::RpcMessenger::kInvalidHandle;
  // Handle of |courier_renderer_|, it would be sent with AcquireRenderer
  // message.
  int sender_renderer_handle_ = openscreen::cast::RpcMessenger::kInvalidHandle;

  base::WeakPtrFactory<End2EndTestRenderer> weak_factory_{this};
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_END2END_TEST_RENDERER_H_

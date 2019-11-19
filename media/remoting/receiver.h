// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_RECEIVER_H_
#define MEDIA_REMOTING_RECEIVER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "media/base/buffering_state.h"
#include "media/base/demuxer_stream.h"
#include "media/base/renderer_client.h"
#include "media/remoting/rpc_broker.h"

namespace media {
class Renderer;
class DecoderBuffer;
}  // namespace media

namespace media {
namespace remoting {

class RpcBroker;
class StreamProvider;

// Media remoting receiver. Media streams are rendered by |renderer|.
// |rpc_broker| outlives this class.
class Receiver final : public RendererClient {
 public:
  Receiver(std::unique_ptr<Renderer> renderer, RpcBroker* rpc_broker);
  ~Receiver();

  // RendererClient implementation.
  void OnError(PipelineStatus status) override;
  void OnEnded() override;
  void OnStatisticsUpdate(const PipelineStatistics& stats) override;
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) override;
  void OnWaiting(WaitingReason reason) override;
  void OnAudioConfigChange(const AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;

  void OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message);
  void OnReceivedBuffer(DemuxerStream::Type type,
                        scoped_refptr<DecoderBuffer> buffer);

 private:
  // RPC message handlers.
  void AcquireRenderer(std::unique_ptr<pb::RpcMessage> message);
  void Initialize(std::unique_ptr<pb::RpcMessage> message);
  void SetPlaybackRate(std::unique_ptr<pb::RpcMessage> message);
  void FlushUntil(std::unique_ptr<pb::RpcMessage> message);
  void StartPlayingFrom(std::unique_ptr<pb::RpcMessage> message);
  void SetVolume(std::unique_ptr<pb::RpcMessage> message);

  // Initialization callbacks.
  void OnStreamInitialized();
  void OnRendererInitialized(PipelineStatus status);

  void OnFlushDone();

  // Periodically send the UpdateTime RPC message to update the media time.
  void ScheduleMediaTimeUpdates();
  void SendMediaTimeUpdate();

  const std::unique_ptr<Renderer> renderer_;
  RpcBroker* const rpc_broker_;  // Outlives this class.

  // The CourierRenderer handle on sender side. Set when AcauireRenderer() is
  // called.
  int remote_handle_ = RpcBroker::kInvalidHandle;

  int rpc_handle_ = RpcBroker::kInvalidHandle;

  std::unique_ptr<StreamProvider> stream_provider_;

  // The timer to periodically update the media time.
  base::RepeatingTimer time_update_timer_;

  base::WeakPtrFactory<Receiver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Receiver);
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_RECEIVER_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_RECEIVER_H_
#define MEDIA_REMOTING_RECEIVER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "media/base/buffering_state.h"
#include "media/base/demuxer_stream.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/remoting/media_remoting_rpc.pb.h"
#include "media/remoting/rpc_broker.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
namespace remoting {

class ReceiverController;
class RpcBroker;

// Receiver runs on a remote device, and forwards the information sent from a
// CourierRenderer to |renderer_|, which actually renders the media.
//
// Receiver implements media::Renderer to be able to work with
// WebMediaPlayerImpl. However, most of the APIs of media::Renderer are dummy
// functions, because the media playback of the remoting media is not controlled
// by the local pipeline of WMPI. It should be controlled by the remoting sender
// via RPC calls. When Receiver receives RPC calls, it will call the
// corresponding functions of |renderer_| to control the media playback of
// the remoting media.
class Receiver final : public Renderer, public RendererClient {
 public:
  Receiver(int rpc_handle,
           int remote_handle,
           ReceiverController* receiver_controller,
           const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
           std::unique_ptr<Renderer> renderer,
           base::OnceCallback<void(int)> acquire_renderer_done_cb);
  ~Receiver() override;

  // Renderer implementation
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(base::Optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;

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
  void OnVideoFrameRateChange(base::Optional<int>) override;

  // Used to set |remote_handle_| after Receiver is created, because the remote
  // handle might be received after Receiver is created.
  void SetRemoteHandle(int remote_handle);

  base::WeakPtr<Receiver> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  // Send RPC message on |main_task_runner_|.
  void SendRpcMessageOnMainThread(std::unique_ptr<pb::RpcMessage> message);

  // Callback function when RPC message is received.
  void OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message);

  // RPC message handlers.
  void RpcInitialize(std::unique_ptr<pb::RpcMessage> message);
  void RpcSetPlaybackRate(std::unique_ptr<pb::RpcMessage> message);
  void RpcFlushUntil(std::unique_ptr<pb::RpcMessage> message);
  void RpcStartPlayingFrom(std::unique_ptr<pb::RpcMessage> message);
  void RpcSetVolume(std::unique_ptr<pb::RpcMessage> message);

  void ShouldInitializeRenderer();
  void OnRendererInitialized(PipelineStatus status);
  void VerifyAcquireRendererDone();
  void OnFlushDone();

  // Periodically send the UpdateTime RPC message to update the media time.
  void ScheduleMediaTimeUpdates();
  void SendMediaTimeUpdate();

  // The callback function to call when |this| is initialized.
  PipelineStatusCallback init_cb_;

  // Indicates whether |this| received RPC_R_INITIALIZE message or not.
  bool rpc_initialize_received_ = false;

  // Owns by the WebMediaPlayerImpl instance.
  MediaResource* demuxer_ = nullptr;

  // The handle of |this| for listening RPC messages.
  const int rpc_handle_;

  // The CourierRenderer handle on sender side. |remote_handle_| could be set
  // through the ctor or SetRemoteHandle().
  int remote_handle_;

  ReceiverController* const receiver_controller_;  // Outlives this class.
  RpcBroker* const rpc_broker_;                    // Outlives this class.

  // Calling SendMessageCallback() of |rpc_broker_| should be on main thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Media tasks should run on media thread.
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // |renderer_| is the real renderer to render media.
  std::unique_ptr<Renderer> renderer_;

  // The callback function to send RPC_ACQUIRE_RENDERER_DONE.
  base::OnceCallback<void(int)> acquire_renderer_done_cb_;

  // The timer to periodically update the media time.
  base::RepeatingTimer time_update_timer_;

  base::WeakPtrFactory<Receiver> weak_factory_{this};
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_RECEIVER_H_

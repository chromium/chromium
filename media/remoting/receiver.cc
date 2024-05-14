// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/receiver.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/renderer.h"
#include "media/cast/openscreen/remoting_message_factories.h"
#include "media/cast/openscreen/remoting_proto_enum_utils.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "media/remoting/receiver_controller.h"
#include "media/remoting/stream_provider.h"

using openscreen::cast::RpcMessenger;

namespace media {
namespace remoting {
namespace {

// The period to send the TimeUpdate RPC message to update the media time on
// sender side.
constexpr base::TimeDelta kTimeUpdateInterval = base::Milliseconds(250);

}  // namespace

Receiver::Receiver(
    int rpc_handle,
    int remote_handle,
    ReceiverController* receiver_controller,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    std::unique_ptr<Renderer> renderer,
    base::OnceCallback<void(int)> acquire_renderer_done_cb)
    : rpc_handle_(rpc_handle),
      remote_handle_(remote_handle),
      receiver_controller_(receiver_controller),
      rpc_messenger_(receiver_controller_->rpc_messenger()),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      media_task_runner_(media_task_runner),
      renderer_(std::move(renderer)),
      acquire_renderer_done_cb_(std::move(acquire_renderer_done_cb)) {
  DCHECK(rpc_handle_ != RpcMessenger::kInvalidHandle);
  DCHECK(receiver_controller_);
  DCHECK(rpc_messenger_);
  DCHECK(renderer_);

  // Note: The constructor is running on the main thread, but will be destroyed
  // on the media thread. Therefore, all weak pointers must be dereferenced on
  // the media thread.
  auto receive_callback = base::BindPostTask(
      media_task_runner_,
      BindRepeating(&Receiver::OnReceivedRpc, weak_factory_.GetWeakPtr()));

  // Listening all renderer rpc messages.
  rpc_messenger_->RegisterMessageReceiverCallback(
      rpc_handle_, [cb = std::move(receive_callback)](
                       std::unique_ptr<openscreen::cast::RpcMessage> message) {
        cb.Run(std::move(message));
      });

  VerifyAcquireRendererDone();
}

Receiver::~Receiver() {
  rpc_messenger_->UnregisterMessageReceiverCallback(rpc_handle_);
}

// Receiver::Initialize() will be called by the local pipeline, it would only
// keep the |init_cb| in order to continue the initialization once it receives
// RPC_R_INITIALIZE, which means Receiver::OnRpcInitialize() is called.
void Receiver::Initialize(MediaResource* media_resource,
                          RendererClient* client,
                          PipelineStatusCallback init_cb) {
  demuxer_ = media_resource;
  init_cb_ = std::move(init_cb);
  ShouldInitializeRenderer();
}

/* CDM is not supported for remoting media */
void Receiver::SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) {
  NOTREACHED_IN_MIGRATION();
}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::SetLatencyHint(std::optional<base::TimeDelta> latency_hint) {}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::Flush(base::OnceClosure flush_cb) {}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::StartPlayingFrom(base::TimeDelta time) {}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::SetPlaybackRate(double playback_rate) {}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::SetVolume(float volume) {}

// No-op. Controlled by sender via RPC calls instead.
base::TimeDelta Receiver::GetMediaTime() {
  return base::TimeDelta();
}

RendererType Receiver::GetRendererType() {
  return RendererType::kRemoting;
}

void Receiver::SendRpcMessageOnMainThread(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  // |rpc_messenger_| is owned by |receiver_controller_| which is a singleton
  // per process, so it's safe to use Unretained() here.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RpcMessenger::SendMessageToRemote,
                                base::Unretained(rpc_messenger_), *message));
}

void Receiver::OnReceivedRpc(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);

  media::cast::DispatchRendererRpcCall(message.get(), this);
}

void Receiver::SetRemoteHandle(int remote_handle) {
  DCHECK_NE(remote_handle, RpcMessenger::kInvalidHandle);
  DCHECK_EQ(remote_handle_, RpcMessenger::kInvalidHandle);
  remote_handle_ = remote_handle;
  VerifyAcquireRendererDone();
}

void Receiver::VerifyAcquireRendererDone() {
  if (remote_handle_ == RpcMessenger::kInvalidHandle)
    return;

  DCHECK(acquire_renderer_done_cb_);
  std::move(acquire_renderer_done_cb_).Run(rpc_handle_);
}

void Receiver::OnRpcInitialize() {
  DCHECK(renderer_);
  rpc_initialize_received_ = true;
  ShouldInitializeRenderer();
}

void Receiver::ShouldInitializeRenderer() {
  // ShouldInitializeRenderer() will be called from Initialize() and
  // OnRpcInitialize() in different orders.
  //
  // |renderer_| must be initialized when both Initialize() and
  // OnRpcInitialize() are called.
  if (!rpc_initialize_received_ || !init_cb_)
    return;

  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(renderer_);
  DCHECK(demuxer_);
  renderer_->Initialize(demuxer_, this,
                        base::BindOnce(&Receiver::OnRendererInitialized,
                                       weak_factory_.GetWeakPtr()));
}

void Receiver::OnRendererInitialized(PipelineStatus status) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(init_cb_);
  std::move(init_cb_).Run(status);

  auto rpc = media::cast::CreateMessageForInitializationComplete(status ==
                                                                 PIPELINE_OK);
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnRpcSetPlaybackRate(double playback_rate) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  renderer_->SetPlaybackRate(playback_rate);

  if (playback_rate == 0.0) {
    if (time_update_timer_.IsRunning()) {
      time_update_timer_.Stop();
      // Send one final media time update since the sender will not get any
      // until playback resumes.
      SendMediaTimeUpdate();
    }
  } else {
    ScheduleMediaTimeUpdates();
  }
}

void Receiver::OnRpcFlush(uint32_t audio_count, uint32_t video_count) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  receiver_controller_->OnRendererFlush(audio_count, video_count);

  time_update_timer_.Stop();
  renderer_->Flush(
      base::BindOnce(&Receiver::OnFlushDone, weak_factory_.GetWeakPtr()));
}

void Receiver::OnFlushDone() {
  auto rpc = media::cast::CreateMessageForFlushComplete();
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnRpcStartPlayingFrom(base::TimeDelta time) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  renderer_->StartPlayingFrom(time);
  ScheduleMediaTimeUpdates();
}

void Receiver::ScheduleMediaTimeUpdates() {
  if (time_update_timer_.IsRunning())
    return;
  SendMediaTimeUpdate();
  time_update_timer_.Start(FROM_HERE, kTimeUpdateInterval,
                           base::BindRepeating(&Receiver::SendMediaTimeUpdate,
                                               weak_factory_.GetWeakPtr()));
}

void Receiver::OnRpcSetVolume(double volume) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  renderer_->SetVolume(volume);
}

void Receiver::SendMediaTimeUpdate() {
  // Issues RPC_RC_ONTIMEUPDATE RPC message.
  auto rpc =
      media::cast::CreateMessageForMediaTimeUpdate(renderer_->GetMediaTime());
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnError(PipelineStatus status) {
  auto rpc = media::cast::CreateMessageForError();
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnFallback(PipelineStatus status) {
  NOTREACHED_IN_MIGRATION();
}

void Receiver::OnEnded() {
  auto rpc = media::cast::CreateMessageForMediaEnded();
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
  time_update_timer_.Stop();
}

void Receiver::OnStatisticsUpdate(const PipelineStatistics& stats) {
  auto rpc = media::cast::CreateMessageForStatisticsUpdate(stats);
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnBufferingStateChange(BufferingState state,
                                      BufferingStateChangeReason reason) {
  auto rpc = media::cast::CreateMessageForBufferingStateChange(state);
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnWaiting(WaitingReason reason) {
  // Media Remoting has not implemented this concept.
  NOTIMPLEMENTED();
}

void Receiver::OnAudioConfigChange(const AudioDecoderConfig& config) {
  auto rpc = media::cast::CreateMessageForAudioConfigChange(config);
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnVideoConfigChange(const VideoDecoderConfig& config) {
  auto rpc = media::cast::CreateMessageForVideoConfigChange(config);
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnVideoNaturalSizeChange(const gfx::Size& size) {
  auto rpc = media::cast::CreateMessageForVideoNaturalSizeChange(size);
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));

  // Notify the host.
  receiver_controller_->OnVideoNaturalSizeChange(size);
}

void Receiver::OnVideoOpacityChange(bool opaque) {
  auto rpc = media::cast::CreateMessageForVideoOpacityChange(opaque);
  rpc->set_handle(remote_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnVideoFrameRateChange(std::optional<int>) {}

}  // namespace remoting
}  // namespace media

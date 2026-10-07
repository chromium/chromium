// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_renderer_factory.h"

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/demuxer.h"
#include "media/cast/openscreen/remoting_message_factories.h"
#include "media/remoting/receiver.h"
#include "media/remoting/receiver_controller.h"
#include "media/remoting/stream_provider.h"

using openscreen::cast::RpcMessenger;

namespace media {
namespace remoting {

RemotingRendererFactory::RemotingRendererFactory(
    mojo::PendingRemote<mojom::Remotee> remotee,
    std::unique_ptr<RendererFactory> renderer_factory,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner)
    : receiver_controller_(ReceiverController::GetInstance()),
      rpc_messenger_(receiver_controller_->rpc_messenger()),
      renderer_handle_(rpc_messenger_->GetUniqueHandle()),
      real_renderer_factory_(std::move(renderer_factory)),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      media_task_runner_(media_task_runner) {
  DCHECK(receiver_controller_);

  // Register the callback to listen for RPC_ACQUIRE_RENDERER messages.
  // RpcMessenger invokes callbacks synchronously on the main sequence, where
  // RemotingRendererFactory is constructed and destroyed. The lambda captures
  // `weak_this` by value; `weak_this` is dereferenced and evaluated at call
  // time on the main sequence. If `this` is destroyed before an RPC arrives,
  // `weak_this` evaluates to false and the message is safely dropped.
  rpc_messenger_->RegisterMessageReceiverCallback(
      RpcMessenger::kAcquireRendererHandle,
      [weak_this = weak_factory_.GetWeakPtr()](
          std::unique_ptr<openscreen::cast::RpcMessage> message) {
        if (weak_this) {
          weak_this->OnReceivedRpc(std::move(message));
        }
      });
  receiver_controller_->Initialize(std::move(remotee));
}

RemotingRendererFactory::~RemotingRendererFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rpc_messenger_->UnregisterMessageReceiverCallback(
      RpcMessenger::kAcquireRendererHandle);
}

std::unique_ptr<Renderer> RemotingRendererFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Receiver::SetRemoteHandle invokes `acquire_renderer_done_cb` on the media
  // sequence, so bounce OnAcquireRendererDone back to `main_task_runner_` with
  // a main-sequence WeakPtr.
  auto acquire_renderer_done_cb = base::BindPostTask(
      main_task_runner_,
      base::BindOnce(&RemotingRendererFactory::OnAcquireRendererDone,
                     weak_factory_.GetWeakPtr()));

  auto receiver = std::make_unique<Receiver>(
      renderer_handle_, remote_renderer_handle_, receiver_controller_,
      media_task_runner,
      real_renderer_factory_->CreateRenderer(
          media_task_runner, worker_task_runner, audio_renderer_sink,
          video_renderer_sink, request_overlay_info_cb, target_color_space),
      std::move(acquire_renderer_done_cb));

  // If we haven't received an RPC_ACQUIRE_RENDERER yet, keep a reference to
  // `receiver`, and set its remote handle when we get the call to
  // OnAcquireRenderer().
  if (remote_renderer_handle_ == RpcMessenger::kInvalidHandle) {
    has_waiting_receiver_ = true;
    waiting_for_remote_handle_receiver_ = receiver->GetWeakPtr();
  }

  return std::move(receiver);
}

void RemotingRendererFactory::OnReceivedRpc(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(message);
  if (message->proc() == openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER)
    OnAcquireRenderer(std::move(message));
  else
    VLOG(1) << __func__ << ": Unknown RPC message. proc=" << message->proc();
}

void RemotingRendererFactory::OnAcquireRenderer(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(message->has_integer_value());
  DCHECK(message->integer_value() != RpcMessenger::kInvalidHandle);

  remote_renderer_handle_ = message->integer_value();

  // If CreateRenderer() was called before we had a valid
  // `remote_renderer_handle_`, set it on the already created Receiver.
  if (has_waiting_receiver_) {
    has_waiting_receiver_ = false;
    // `waiting_for_remote_handle_receiver_` holds the WeakPtr of the Receiver
    // instance and must only be dereferenced on the media sequence.
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Receiver::SetRemoteHandle,
                                  waiting_for_remote_handle_receiver_,
                                  remote_renderer_handle_));
  }
}

void RemotingRendererFactory::OnAcquireRendererDone(int receiver_rpc_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // RPC_ACQUIRE_RENDERER_DONE should be sent only once.
  //
  // WebMediaPlayerImpl might destroy and re-create the Receiver instance
  // several times for saving resources. However, RPC_ACQUIRE_RENDERER_DONE
  // shouldn't be sent multiple times whenever a Receiver instance is created.
  if (is_acquire_renderer_done_sent_)
    return;

  auto rpc =
      media::cast::CreateMessageForAcquireRendererDone(receiver_rpc_handle);
  rpc->set_handle(remote_renderer_handle_);
  rpc_messenger_->SendMessageToRemote(*rpc);

  // Once RPC_ACQUIRE_RENDERER_DONE is sent, it implies there is no Receiver
  // instance that is waiting for the remote handle.
  has_waiting_receiver_ = false;

  is_acquire_renderer_done_sent_ = true;
}

}  // namespace remoting
}  // namespace media

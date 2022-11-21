// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/receiver_controller.h"

#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"

namespace media {
namespace remoting {

// static
ReceiverController* ReceiverController::GetInstance() {
  static base::NoDestructor<ReceiverController> controller;
  return controller.get();
}

ReceiverController::ReceiverController()
    : rpc_messenger_([this](std::vector<uint8_t> message) {
        OnSendRpc(std::move(message));
      }),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

ReceiverController::~ReceiverController() = default;

void ReceiverController::Initialize(
    mojo::PendingRemote<mojom::Remotee> remotee) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!media_remotee_.is_bound());
  media_remotee_.Bind(std::move(remotee));

  // Calling NotifyRemotingSinkReady() to notify the host that RemotingSink is
  // ready.
  media_remotee_->OnRemotingSinkReady(receiver_.BindNewPipeAndPassRemote());
}

void ReceiverController::OnRendererFlush(uint32_t audio_count,
                                         uint32_t video_count) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    // |this| is a singleton per process, it would be safe to use
    // base::Unretained() here.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ReceiverController::OnRendererFlush,
                       base::Unretained(this), audio_count, video_count));
    return;
  }

  if (media_remotee_.is_bound())
    media_remotee_->OnFlushUntil(audio_count, video_count);
}

void ReceiverController::OnVideoNaturalSizeChange(const gfx::Size& size) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    // |this| is a singleton per process, it would be safe to use
    // base::Unretained() here.
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ReceiverController::OnVideoNaturalSizeChange,
                                  base::Unretained(this), size));
    return;
  }

  if (media_remotee_.is_bound())
    media_remotee_->OnVideoNaturalSizeChange(size);
}

void ReceiverController::StartDataStreams(
    mojo::PendingRemote<::media::mojom::RemotingDataStreamReceiver>
        audio_stream,
    mojo::PendingRemote<::media::mojom::RemotingDataStreamReceiver>
        video_stream) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    // |this| is a singleton per process, it would be safe to use
    // base::Unretained() here.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ReceiverController::StartDataStreams,
                       base::Unretained(this), std::move(audio_stream),
                       std::move(video_stream)));
    return;
  }
  if (media_remotee_.is_bound()) {
    media_remotee_->StartDataStreams(std::move(audio_stream),
                                     std::move(video_stream));
  }
}

void ReceiverController::OnMessageFromSource(
    const std::vector<uint8_t>& message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  auto rpc_message = std::make_unique<openscreen::cast::RpcMessage>(
      openscreen::cast::RpcMessage());
  if (!rpc_message->ParseFromArray(message.data(), message.size()))
    return;

  rpc_messenger_.ProcessMessageFromRemote(std::move(rpc_message));
}

void ReceiverController::OnSendRpc(std::vector<uint8_t> message) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    // |this| is a singleton per process, it would be safe to use
    // base::Unretained() here.
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ReceiverController::OnSendRpc,
                                  base::Unretained(this), std::move(message)));
    return;
  }

  DCHECK(media_remotee_.is_bound());
  if (media_remotee_.is_bound())
    media_remotee_->SendMessageToSource(message);
}

}  // namespace remoting
}  // namespace media

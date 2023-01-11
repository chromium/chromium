// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/flinging_renderer_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"

namespace content {

FlingingRendererClient::FlingingRendererClient(
    ClientExtentionPendingReceiver client_extension_receiver,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    std::unique_ptr<media::MojoRenderer> mojo_renderer,
    media::RemotePlayStateChangeCB remote_play_state_change_cb)
    : MojoRendererWrapper(std::move(mojo_renderer)),
      media_task_runner_(std::move(media_task_runner)),
      remote_play_state_change_cb_(remote_play_state_change_cb),
      delayed_bind_client_extension_receiver_(
          std::move(client_extension_receiver)) {}

FlingingRendererClient::~FlingingRendererClient() = default;

void FlingingRendererClient::Initialize(media::MediaResource* media_resource,
                                        media::RendererClient* client,
                                        media::PipelineStatusCallback init_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  client_ = client;

  client_extension_receiver_.Bind(
      std::move(delayed_bind_client_extension_receiver_), media_task_runner_);

  MojoRendererWrapper::Initialize(media_resource, client, std::move(init_cb));
}

media::RendererType FlingingRendererClient::GetRendererType() {
  return media::RendererType::kFlinging;
}

void FlingingRendererClient::OnRemotePlayStateChange(
    media::MediaStatus::State state) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  remote_play_state_change_cb_.Run(state);
}

}  // namespace content

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/flinging_renderer_client_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "content/renderer/media/android/flinging_renderer_client.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/clients/mojo_renderer_factory.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

FlingingRendererClientFactory::FlingingRendererClientFactory(
    std::unique_ptr<media::MojoRendererFactory> mojo_flinging_factory,
    std::unique_ptr<media::RemotePlaybackClientWrapper> remote_playback_client)
    : mojo_flinging_factory_(std::move(mojo_flinging_factory)),
      remote_playback_client_(std::move(remote_playback_client)) {}

FlingingRendererClientFactory::~FlingingRendererClientFactory() = default;

std::unique_ptr<media::Renderer> FlingingRendererClientFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::AudioRendererSink* audio_renderer_sink,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  DCHECK(IsFlingingActive());
  DCHECK(remote_play_state_change_cb_);

  // Used to send messages from the FlingingRenderer (Browser process),
  // to the FlingingRendererClient (Renderer process). The
  // |client_extension_receiver| will be bound in FlingingRendererClient.
  mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
      client_extension_remote;
  auto client_extension_receiver =
      client_extension_remote.InitWithNewPipeAndPassReceiver();

  auto mojo_renderer = mojo_flinging_factory_->CreateFlingingRenderer(
      GetActivePresentationId(), std::move(client_extension_remote),
      media_task_runner, video_renderer_sink);

  return std::make_unique<FlingingRendererClient>(
      std::move(client_extension_receiver), media_task_runner,
      std::move(mojo_renderer), remote_play_state_change_cb_);
}

void FlingingRendererClientFactory::SetRemotePlayStateChangeCB(
    media::RemotePlayStateChangeCB callback) {
  remote_play_state_change_cb_ = std::move(callback);
}

std::string FlingingRendererClientFactory::GetActivePresentationId() {
  return remote_playback_client_->GetActivePresentationId();
}

bool FlingingRendererClientFactory::IsFlingingActive() {
  return !GetActivePresentationId().empty();
}

}  // namespace content

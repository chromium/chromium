// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/flinging_renderer.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

namespace content {

FlingingRenderer::FlingingRenderer(
    std::unique_ptr<media::FlingingController> controller,
    mojo::PendingRemote<ClientExtension> client_extension)
    : client_extension_(std::move(client_extension)),
      controller_(std::move(controller)) {
  controller_->AddMediaStatusObserver(this);
}

FlingingRenderer::~FlingingRenderer() {
  controller_->RemoveMediaStatusObserver(this);
}

// static
std::unique_ptr<FlingingRenderer> FlingingRenderer::Create(
    RenderFrameHost* render_frame_host,
    const std::string& presentation_id,
    mojo::PendingRemote<ClientExtension> client_extension) {
  DVLOG(1) << __func__;

  ContentClient* content_client = GetContentClient();
  if (!content_client)
    return nullptr;

  ContentBrowserClient* browser_client = content_client->browser();
  if (!browser_client)
    return nullptr;

  ControllerPresentationServiceDelegate* presentation_delegate =
      browser_client->GetControllerPresentationServiceDelegate(
          WebContents::FromRenderFrameHost(render_frame_host));

  if (!presentation_delegate)
    return nullptr;

  auto flinging_controller = presentation_delegate->GetFlingingController(
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), presentation_id);

  if (!flinging_controller)
    return nullptr;

  return base::WrapUnique<FlingingRenderer>(new FlingingRenderer(
      std::move(flinging_controller), std::move(client_extension)));
}

// media::Renderer implementation
void FlingingRenderer::Initialize(media::MediaResource* media_resource,
                                  media::RendererClient* client,
                                  media::PipelineStatusCallback init_cb) {
  DVLOG(2) << __func__;
  client_ = client;
  std::move(init_cb).Run(media::PIPELINE_OK);
}

void FlingingRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {}

void FlingingRenderer::Flush(base::OnceClosure flush_cb) {
  DVLOG(2) << __func__;
  // There is nothing to reset, we can no-op the call.
  std::move(flush_cb).Run();
}

void FlingingRenderer::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(2) << __func__;
  controller_->GetMediaController()->Seek(time);

  // After a seek when using the FlingingRenderer, WMPI will never get back to
  // BUFFERING_HAVE_ENOUGH. This prevents Blink from getting the appropriate
  // seek completion signals, and time updates are never re-scheduled.
  //
  // The FlingingRenderer doesn't need to buffer, since playback happens on a
  // different device. This means it's ok to always send BUFFERING_HAVE_ENOUGH
  // when sending buffering state changes. That being said, sending state
  // changes here might be surprising, but the same signals are sent from
  // MediaPlayerRenderer::StartPlayingFrom(), and it has been working mostly
  // smoothly for all HLS playback.
  client_->OnBufferingStateChange(media::BUFFERING_HAVE_ENOUGH,
                                  media::BUFFERING_CHANGE_REASON_UNKNOWN);
}

void FlingingRenderer::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __func__;
  if (playback_rate == 0) {
    SetExpectedPlayState(PlayState::kPaused);
    controller_->GetMediaController()->Pause();
  } else {
    SetExpectedPlayState(PlayState::kPlaying);
    controller_->GetMediaController()->Play();
  }
}

void FlingingRenderer::SetVolume(float volume) {
  DVLOG(2) << __func__;
  controller_->GetMediaController()->SetVolume(volume);
}

base::TimeDelta FlingingRenderer::GetMediaTime() {
  return controller_->GetApproximateCurrentTime();
}

media::RendererType FlingingRenderer::GetRendererType() {
  return media::RendererType::kFlinging;
}

void FlingingRenderer::SetExpectedPlayState(PlayState state) {
  DVLOG(3) << __func__ << " : state " << static_cast<int>(state);
  DCHECK(state == PlayState::kPlaying || state == PlayState::kPaused);

  expected_play_state_ = state;
  play_state_is_stable_ = (expected_play_state_ == last_play_state_received_);
}

void FlingingRenderer::OnMediaStatusUpdated(const media::MediaStatus& status) {
  const auto& current_state = status.state;

  if (current_state == expected_play_state_)
    play_state_is_stable_ = true;

  // Because we can get a MediaStatus update at any time from the device, only
  // handle state updates after we have reached the target state.
  // If we do not, we can encounter the following scenario:
  // - A user pauses the video.
  // - While the PAUSE command is in flight, an unrelated MediaStatus with a
  //   PLAYING state is sent from the cast device.
  // - We call OnRemotePlaybackStateChange(PLAYING).
  // - As the PAUSE command completes and we receive a PlayState::PAUSE, we
  //   queue a new PLAYING.
  // - The local device enters a tick/tock feedback loop of constantly
  //   requesting the wrong state of PLAYING/PAUSED.
  if (!play_state_is_stable_)
    return;

  // Ignore all non PLAYING/PAUSED states.
  // UNKNOWN and BUFFERING states are uninteresting and can be safely ignored.
  // STOPPED normally causes the session to teardown, and |this| is destroyed
  // shortly after.
  if (current_state != PlayState::kPlaying &&
      current_state != PlayState::kPaused) {
    DVLOG(3) << __func__ << " : external state ignored: "
             << static_cast<int>(current_state);
    return;
  }

  // Save whether the remote device is currently playing or paused.
  last_play_state_received_ = current_state;

  // If the remote device's play state has toggled and we didn't initiate it,
  // notify WMPI to update it's own play/pause state.
  if (last_play_state_received_ != expected_play_state_)
    client_extension_->OnRemotePlayStateChange(current_state);
}

}  // namespace content

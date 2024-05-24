// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_service_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/browser/media/session/media_metadata_sanitizer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

MediaSessionServiceImpl::MediaSessionServiceImpl(
    RenderFrameHost* render_frame_host)
    : render_frame_host_id_(render_frame_host->GetGlobalId()),
      playback_state_(blink::mojom::MediaSessionPlaybackState::NONE) {
  MediaSessionImpl* session = GetMediaSession();
  if (session) {
    media_session_ = session->GetWeakPtr();
    media_session_->OnServiceCreated(this);
  }
}

MediaSessionServiceImpl::~MediaSessionServiceImpl() {
  if (media_session_) {
    media_session_->OnServiceDestroyed(this);
  }
}

// static
void MediaSessionServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::MediaSessionService> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique<MediaSessionServiceImpl>(
          new MediaSessionServiceImpl(render_frame_host)),
      std::move(receiver));
}

GlobalRenderFrameHostId MediaSessionServiceImpl::GetRenderFrameHostId() const {
  return render_frame_host_id_;
}

RenderFrameHost* MediaSessionServiceImpl::GetRenderFrameHost() const {
  return RenderFrameHost::FromID(GetRenderFrameHostId());
}

void MediaSessionServiceImpl::DidFinishNavigation() {
  // At this point the BrowsingContext of the frame has changed, so the members
  // need to be reset, and notify MediaSessionImpl.
  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::NONE);
  SetMetadata(nullptr);
  ClearActions();
}

void MediaSessionServiceImpl::FlushForTesting() {
  client_.FlushForTesting();
}

void MediaSessionServiceImpl::SetClient(
    mojo::PendingRemote<blink::mojom::MediaSessionClient> client) {
  client_ = mojo::Remote<blink::mojom::MediaSessionClient>(std::move(client));
}

void MediaSessionServiceImpl::SetPlaybackState(
    blink::mojom::MediaSessionPlaybackState state) {
  playback_state_ = state;
  if (media_session_) {
    media_session_->OnMediaSessionPlaybackStateChanged(this);
  }
}

void MediaSessionServiceImpl::SetPositionState(
    const std::optional<media_session::MediaPosition>& position) {
  position_ = position;
  if (media_session_) {
    media_session_->RebuildAndNotifyMediaPositionChanged();
  }
}

void MediaSessionServiceImpl::SetMetadata(
    blink::mojom::SpecMediaMetadataPtr metadata) {
  metadata_.reset();

  // When receiving a MediaMetadata, the browser process can't trust that it is
  // coming from a known and secure source. It must be processed accordingly.
  if (!metadata.is_null()) {
    if (!MediaMetadataSanitizer::CheckSanity(metadata)) {
      RenderFrameHost* rfh = GetRenderFrameHost();
      if (rfh) {
        rfh->GetProcess()->ShutdownForBadMessage(
            RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
      }
      return;
    }

    metadata_ = std::move(metadata);
  }

  if (media_session_) {
    media_session_->OnMediaSessionMetadataChanged(this);
  }
}

void MediaSessionServiceImpl::SetMicrophoneState(
    media_session::mojom::MicrophoneState microphone_state) {
  microphone_state_ = microphone_state;
  if (media_session_) {
    media_session_->OnMediaSessionInfoChanged(this);
  }
}

void MediaSessionServiceImpl::SetCameraState(
    media_session::mojom::CameraState camera_state) {
  camera_state_ = camera_state;
  if (media_session_) {
    media_session_->OnMediaSessionInfoChanged(this);
  }
}

void MediaSessionServiceImpl::EnableAction(
    media_session::mojom::MediaSessionAction action) {
  actions_.insert(action);
  if (media_session_) {
    media_session_->OnMediaSessionActionsChanged(this);
  }
}

void MediaSessionServiceImpl::DisableAction(
    media_session::mojom::MediaSessionAction action) {
  actions_.erase(action);
  if (media_session_) {
    media_session_->OnMediaSessionActionsChanged(this);
  }
}

void MediaSessionServiceImpl::ClearActions() {
  actions_.clear();
  if (media_session_) {
    media_session_->OnMediaSessionActionsChanged(this);
  }
}

MediaSessionImpl* MediaSessionServiceImpl::GetMediaSession() {
  RenderFrameHost* rfh = GetRenderFrameHost();
  if (!rfh)
    return nullptr;

  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(WebContentsImpl::FromRenderFrameHost(rfh));
  if (!contents)
    return nullptr;

  return MediaSessionImpl::Get(contents);
}

}  // namespace content

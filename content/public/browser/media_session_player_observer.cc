// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_session_player_observer.h"

#include <utility>

#include "media/base/media_content_type.h"
#include "services/media_session/public/cpp/media_position.h"

namespace content {

void MediaSessionPlayerObserver::OnRequestVisibility(
    int player_id,
    RequestVisibilityCallback request_visibility_callback) {
  std::move(request_visibility_callback).Run(false);
}

std::optional<media_session::MediaPosition>
MediaSessionPlayerObserver::GetPosition(int player_id) const {
  return std::nullopt;
}

bool MediaSessionPlayerObserver::IsPictureInPictureAvailable(
    int player_id) const {
  return false;
}

bool MediaSessionPlayerObserver::IsVideoFrameAvailable(int player_id) const {
  return false;
}

bool MediaSessionPlayerObserver::HasSufficientlyVisibleVideo(
    int player_id) const {
  return false;
}

bool MediaSessionPlayerObserver::HasAudio(int player_id) const {
  return false;
}

bool MediaSessionPlayerObserver::HasVideo(int player_id) const {
  return false;
}

bool MediaSessionPlayerObserver::IsPaused(int player_id) const {
  return false;
}

std::string MediaSessionPlayerObserver::GetAudioOutputSinkId(
    int player_id) const {
  return std::string();
}

bool MediaSessionPlayerObserver::SupportsAudioOutputDeviceSwitching(
    int player_id) const {
  return false;
}

media::MediaContentType MediaSessionPlayerObserver::GetMediaContentType()
    const {
  return media::MediaContentType::kPersistent;
}

RenderFrameHost* MediaSessionPlayerObserver::render_frame_host() const {
  return nullptr;
}

}  // namespace content

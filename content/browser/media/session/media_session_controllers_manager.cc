// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_controllers_manager.h"

#include "base/stl_util.h"
#include "content/browser/media/session/media_session_controller.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/features.h"

namespace content {

namespace {

bool IsMediaSessionEnabled() {
  return base::FeatureList::IsEnabled(
             media_session::features::kMediaSessionService) ||
         base::FeatureList::IsEnabled(media::kInternalMediaSession);
}

}  // namespace

MediaSessionControllersManager::MediaSessionControllersManager(
    MediaWebContentsObserver* media_web_contents_observer)
    : media_web_contents_observer_(media_web_contents_observer) {}

MediaSessionControllersManager::~MediaSessionControllersManager() = default;

void MediaSessionControllersManager::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (!IsMediaSessionEnabled())
    return;

  for (auto it = controllers_map_.begin(); it != controllers_map_.end();) {
    if (it->first.render_frame_host == render_frame_host)
      it = controllers_map_.erase(it);
    else
      ++it;
  }
}

bool MediaSessionControllersManager::RequestPlay(
    const MediaPlayerId& id,
    bool has_audio,
    bool is_remote,
    media::MediaContentType media_content_type) {
  if (!IsMediaSessionEnabled())
    return true;

  // If we have previously received the position for this player then we should
  // initialize the controller with it.
  media_session::MediaPosition* position = nullptr;
  auto position_it = position_map_.find(id);
  if (position_it != position_map_.end())
    position = &position_it->second;

  // Since we don't remove session instances on pause, there may be an existing
  // instance for this playback attempt.
  //
  // In this case, try to reinitialize it with the new settings.  If they are
  // the same, this is a no-op.  If the reinitialize fails, destroy the
  // controller. A later playback attempt will create a new controller.
  auto it = controllers_map_.find(id);
  if (it != controllers_map_.end()) {
    if (it->second->Initialize(has_audio, is_remote, media_content_type,
                               position)) {
      return true;
    }

    controllers_map_.erase(it);
    return false;
  }

  std::unique_ptr<MediaSessionController> controller(
      new MediaSessionController(id, media_web_contents_observer_));

  if (!controller->Initialize(has_audio, is_remote, media_content_type,
                              position)) {
    return false;
  }

  controllers_map_[id] = std::move(controller);
  return true;
}

void MediaSessionControllersManager::OnPause(const MediaPlayerId& id) {
  if (!IsMediaSessionEnabled())
    return;

  auto it = controllers_map_.find(id);
  if (it == controllers_map_.end())
    return;

  it->second->OnPlaybackPaused();
}

void MediaSessionControllersManager::OnEnd(const MediaPlayerId& id) {
  if (!IsMediaSessionEnabled())
    return;
  controllers_map_.erase(id);
}

void MediaSessionControllersManager::OnMediaPositionStateChanged(
    const MediaPlayerId& id,
    const media_session::MediaPosition& position) {
  if (!IsMediaSessionEnabled())
    return;

  base::InsertOrAssign(position_map_, id, position);

  auto it = controllers_map_.find(id);
  if (it == controllers_map_.end())
    return;

  it->second->OnMediaPositionStateChanged(position);
}

void MediaSessionControllersManager::WebContentsMutedStateChanged(bool muted) {
  if (!IsMediaSessionEnabled())
    return;

  for (auto& entry : controllers_map_)
    entry.second->WebContentsMutedStateChanged(muted);
}

}  // namespace content

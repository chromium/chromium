// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_controllers_manager.h"

#include <map>

#include "content/browser/media/session/media_session_controller.h"
#include "content/browser/web_contents/web_contents_impl.h"
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
    WebContentsImpl* web_contents)
    : web_contents_(web_contents) {}

MediaSessionControllersManager::~MediaSessionControllersManager() = default;

void MediaSessionControllersManager::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (!IsMediaSessionEnabled())
    return;

  std::erase_if(
      controllers_map_,
      [render_frame_host](const ControllersMap::value_type& id_and_controller) {
        return render_frame_host->GetGlobalId() ==
               id_and_controller.first.frame_routing_id;
      });
}

void MediaSessionControllersManager::OnMetadata(
    const MediaPlayerId& id,
    bool has_audio,
    bool has_video,
    media::MediaContentType media_content_type) {
  if (!IsMediaSessionEnabled())
    return;

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->SetMetadata(has_audio, has_video, media_content_type);
}

bool MediaSessionControllersManager::RequestPlay(const MediaPlayerId& id) {
  if (!IsMediaSessionEnabled())
    return true;

  MediaSessionController* const controller = FindOrCreateController(id);
  return controller->OnPlaybackStarted();
}

void MediaSessionControllersManager::OnPause(const MediaPlayerId& id,
                                             bool reached_end_of_stream) {
  if (!IsMediaSessionEnabled())
    return;

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnPlaybackPaused(reached_end_of_stream);
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

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnMediaPositionStateChanged(position);
}

void MediaSessionControllersManager::PictureInPictureStateChanged(
    bool is_picture_in_picture) {
  if (!IsMediaSessionEnabled())
    return;

  for (auto& entry : controllers_map_)
    entry.second->PictureInPictureStateChanged(is_picture_in_picture);
}

void MediaSessionControllersManager::WebContentsMutedStateChanged(bool muted) {
  if (!IsMediaSessionEnabled())
    return;

  for (auto& entry : controllers_map_)
    entry.second->WebContentsMutedStateChanged(muted);
}

void MediaSessionControllersManager::OnMediaMutedStatusChanged(
    const MediaPlayerId& id,
    bool mute) {
  if (!IsMediaSessionEnabled())
    return;

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnMediaMutedStatusChanged(mute);
}

void MediaSessionControllersManager::OnPictureInPictureAvailabilityChanged(
    const MediaPlayerId& id,
    bool available) {
  if (!IsMediaSessionEnabled())
    return;

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnPictureInPictureAvailabilityChanged(available);
}

void MediaSessionControllersManager::OnAudioOutputSinkChanged(
    const MediaPlayerId& id,
    const std::string& raw_device_id) {
  if (!IsMediaSessionEnabled())
    return;

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnAudioOutputSinkChanged(raw_device_id);
}

void MediaSessionControllersManager::OnAudioOutputSinkChangingDisabled(
    const MediaPlayerId& id) {
  if (!IsMediaSessionEnabled())
    return;

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnAudioOutputSinkChangingDisabled();
}

void MediaSessionControllersManager::OnRemotePlaybackMetadataChange(
    const MediaPlayerId& id,
    media_session::mojom::RemotePlaybackMetadataPtr remote_playback_metadata) {
  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnRemotePlaybackMetadataChanged(
      std::move(remote_playback_metadata));
}

void MediaSessionControllersManager::OnVideoVisibilityChanged(
    const MediaPlayerId& id,
    bool meets_visibility_threshold) {
  if (!IsMediaSessionEnabled()) {
    return;
  }

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnVideoVisibilityChanged(meets_visibility_threshold);
}

MediaSessionController* MediaSessionControllersManager::FindOrCreateController(
    const MediaPlayerId& id) {
  auto it = controllers_map_.find(id);
  if (it == controllers_map_.end()) {
    it = controllers_map_
             .emplace(id, std::make_unique<MediaSessionController>(
                              id, web_contents_))
             .first;
  }
  return it->second.get();
}

}  // namespace content

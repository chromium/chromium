// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/pepper_playback_observer.h"

#include <memory>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/media/session/pepper_player_delegate.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"

namespace content {

PepperPlaybackObserver::PepperPlaybackObserver(WebContents* contents)
    : contents_(contents) {}

PepperPlaybackObserver::~PepperPlaybackObserver() {
  // At this point WebContents is being destructed, so it's safe to
  // call this. MediaSession may decide to send further IPC messages
  // through PepperPlayerDelegates, which might be declined if the
  // RenderViewHost has been destroyed.
  for (auto it = players_played_sound_map_.begin();
       it != players_played_sound_map_.end();) {
    const PlayerId& id = (it++)->first;
    PepperInstanceDeleted(id.first, id.second);
  }
}

void PepperPlaybackObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  std::vector<PlayerId> players_to_remove;
  for (auto it = players_played_sound_map_.begin();
       it != players_played_sound_map_.end();) {
    const PlayerId& id = (it++)->first;
    if (id.first == render_frame_host)
      PepperInstanceDeleted(id.first, id.second);
  }
}

void PepperPlaybackObserver::PepperInstanceCreated(
    RenderFrameHost* render_frame_host, int32_t pp_instance) {
  PlayerId id(render_frame_host, pp_instance);
  players_played_sound_map_[id] = false;
}

void PepperPlaybackObserver::PepperInstanceDeleted(
    RenderFrameHost* render_frame_host, int32_t pp_instance) {
  PlayerId id(render_frame_host, pp_instance);

  auto iter = players_played_sound_map_.find(id);
  if (iter == players_played_sound_map_.end())
    return;

  players_played_sound_map_.erase(iter);

  PepperStopsPlayback(render_frame_host, pp_instance);
}

void PepperPlaybackObserver::PepperStartsPlayback(
    RenderFrameHost* render_frame_host, int32_t pp_instance) {
  PlayerId id(render_frame_host, pp_instance);

  players_played_sound_map_[id] = true;

  if (players_map_.count(id))
    return;

  players_map_[id] = std::make_unique<PepperPlayerDelegate>(
      render_frame_host, pp_instance,
      base::FeatureList::IsEnabled(media::kAudioFocusDuckFlash)
          ? media::MediaContentType::kPepper
          : media::MediaContentType::kOneShot);

  MediaSessionImpl::Get(contents_)->AddPlayer(players_map_[id].get(),
                                              PepperPlayerDelegate::kPlayerId);
}

void PepperPlaybackObserver::PepperStopsPlayback(
    RenderFrameHost* render_frame_host, int32_t pp_instance) {
  PlayerId id(render_frame_host, pp_instance);

  if (!players_map_.count(id))
    return;

  MediaSessionImpl::Get(contents_)->RemovePlayer(
      players_map_[id].get(), PepperPlayerDelegate::kPlayerId);

  players_map_.erase(id);
}

}  // namespace content

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_player_renderer_web_contents_observer.h"

#include "content/browser/media/android/media_player_renderer.h"

namespace content {

MediaPlayerRendererWebContentsObserver::MediaPlayerRendererWebContentsObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<MediaPlayerRendererWebContentsObserver>(
          *web_contents) {}

MediaPlayerRendererWebContentsObserver::
    ~MediaPlayerRendererWebContentsObserver() = default;

void MediaPlayerRendererWebContentsObserver::AddMediaPlayerRenderer(
    MediaPlayerRenderer* player) {
  DCHECK(player);
  DCHECK(players_.find(player) == players_.end());
  players_.insert(player);
}

void MediaPlayerRendererWebContentsObserver::RemoveMediaPlayerRenderer(
    MediaPlayerRenderer* player) {
  DCHECK(player);
  auto erase_result = players_.erase(player);
  DCHECK_EQ(1u, erase_result);
}

void MediaPlayerRendererWebContentsObserver::DidUpdateAudioMutingState(
    bool muted) {
  for (MediaPlayerRenderer* player : players_)
    player->OnUpdateAudioMutingState(muted);
}

void MediaPlayerRendererWebContentsObserver::WebContentsDestroyed() {
  for (MediaPlayerRenderer* player : players_)
    player->OnWebContentsDestroyed();
  players_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaPlayerRendererWebContentsObserver);

}  // namespace content

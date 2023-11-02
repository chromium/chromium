// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYBACK_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYBACK_OBSERVER_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"

namespace content {

class PepperPlayerDelegate;
class RenderFrameHost;
class WebContents;

// Class observing Pepper playback changes from WebContents, and update
// MediaSession accordingly. Can only be a member of WebContents and must be
// destroyed in ~WebContents().
class PepperPlaybackObserver {
 public:
  explicit PepperPlaybackObserver(WebContents* contents);

  PepperPlaybackObserver(const PepperPlaybackObserver&) = delete;
  PepperPlaybackObserver& operator=(const PepperPlaybackObserver&) = delete;

  virtual ~PepperPlaybackObserver();

  void RenderFrameDeleted(RenderFrameHost* render_frame_host);

  void PepperInstanceCreated(RenderFrameHost* render_frame_host,
                             int32_t pp_instance);
  void PepperInstanceDeleted(RenderFrameHost* render_frame_host,
                             int32_t pp_instance);
  // This method is called when a Pepper instance starts making sound.
  void PepperStartsPlayback(RenderFrameHost* render_frame_host,
                            int32_t pp_instance);
  // This method is called when a Pepper instance stops making sound.
  void PepperStopsPlayback(RenderFrameHost* render_frame_host,
                           int32_t pp_instance);

 private:
  using PlayerId = std::pair<RenderFrameHost*, int32_t>;

  // Owning PepperPlayerDelegates.
  using PlayersMap = std::map<PlayerId, std::unique_ptr<PepperPlayerDelegate>>;
  PlayersMap players_map_;

  // Map for whether Pepper players have ever played sound.
  // Used for recording UMA.
  //
  // The mapped player ids must be a super-set of player ids in |players_map_|,
  // and the map is also used for cleaning up when RenderFrame is deleted or
  // WebContents is destructed.
  using PlayersPlayedSoundMap = std::map<PlayerId, bool>;
  PlayersPlayedSoundMap players_played_sound_map_;

  // Weak reference to WebContents.
  raw_ptr<WebContents> contents_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYBACK_OBSERVER_H_

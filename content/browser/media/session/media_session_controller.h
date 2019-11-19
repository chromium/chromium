// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/cpp/media_position.h"

namespace content {

class MediaSessionImpl;
class MediaWebContentsObserver;

// Helper class for controlling a single player's MediaSession instance.  Sends
// browser side MediaSession commands back to a player hosted in the renderer
// process.
class CONTENT_EXPORT MediaSessionController
    : public MediaSessionPlayerObserver {
 public:
  MediaSessionController(const MediaPlayerId& id,
                         MediaWebContentsObserver* media_web_contents_observer);
  ~MediaSessionController() override;

  // Clients must call this after construction and destroy the controller if it
  // returns false.  May be called more than once; does nothing if none of the
  // input parameters have changed since the last call.
  //
  // Note: Once a session has been initialized with |has_audio| as true, all
  // future calls to Initialize() will retain this flag.
  // TODO(dalecurtis): Delete sticky audio once we're no longer using WMPA and
  // the BrowserMediaPlayerManagers.  Tracked by http://crbug.com/580626
  bool Initialize(bool has_audio,
                  bool is_remote,
                  media::MediaContentType media_content_type,
                  media_session::MediaPosition* position);

  // Must be called when a pause occurs on the renderer side media player; keeps
  // the MediaSession instance in sync with renderer side behavior.
  virtual void OnPlaybackPaused();

  // MediaSessionObserver implementation.
  void OnSuspend(int player_id) override;
  void OnResume(int player_id) override;
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override;
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override;
  RenderFrameHost* render_frame_host() const override;
  base::Optional<media_session::MediaPosition> GetPosition(
      int player_id) const override;

  // Test helpers.
  int get_player_id_for_testing() const { return player_id_; }

  // Called when the WebContents is either muted or unmuted.
  void WebContentsMutedStateChanged(bool muted);

  // Called when the media position state of the player has changed.
  void OnMediaPositionStateChanged(
      const media_session::MediaPosition& position);

 private:
  friend class MediaSessionControllerTest;

  const MediaPlayerId id_;

  // Non-owned pointer; |media_web_contents_observer_| is the owner of |this|.
  MediaWebContentsObserver* const media_web_contents_observer_;

  // Non-owned pointer; lifetime is the same as |media_web_contents_observer_|.
  MediaSessionImpl* const media_session_;

  base::Optional<media_session::MediaPosition> position_;

  int player_id_ = 0;
  bool has_session_ = false;
  bool has_audio_ = false;
  bool is_remote_ = false;
  media::MediaContentType media_content_type_ =
      media::MediaContentType::Persistent;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLER_H_

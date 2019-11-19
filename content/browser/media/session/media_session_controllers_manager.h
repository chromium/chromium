// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLERS_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLERS_MANAGER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"  // For MediaPlayerId.

namespace media {
enum class MediaContentType;
}  // namespace media

namespace media_session {
struct MediaPosition;
}  // namespace media_session

namespace content {

class MediaSessionController;
class MediaWebContentsObserver;
class RenderFrameHost;

// MediaSessionControllersManager is a delegate of MediaWebContentsObserver that
// handles MediaSessionController instances.
class CONTENT_EXPORT MediaSessionControllersManager {
 public:
  using ControllersMap =
      std::map<MediaPlayerId, std::unique_ptr<MediaSessionController>>;
  using PositionMap = std::map<MediaPlayerId, media_session::MediaPosition>;

  explicit MediaSessionControllersManager(
      MediaWebContentsObserver* media_web_contents_observer);
  ~MediaSessionControllersManager();

  // Clear all the MediaSessionController associated with the given
  // |render_frame_host|.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host);

  // Called before a player starts playing. It will be added to the media
  // session and will have a controller associated with it.
  // Returns whether the player was added to the session and can start playing.
  bool RequestPlay(const MediaPlayerId& id,
                   bool has_audio,
                   bool is_remote,
                   media::MediaContentType media_content_type);

  // Called when the given player |id| has paused.
  void OnPause(const MediaPlayerId& id);

  // Called when the given player |id| has ended.
  void OnEnd(const MediaPlayerId& id);

  // Called when the media position state for the player |id| has changed.
  void OnMediaPositionStateChanged(
      const MediaPlayerId& id,
      const media_session::MediaPosition& position);

  // Called when the WebContents was muted or unmuted.
  void WebContentsMutedStateChanged(bool muted);

 private:
  friend class MediaSessionControllersManagerTest;

  // Weak pointer because |this| is owned by |media_web_contents_observer_|.
  MediaWebContentsObserver* const media_web_contents_observer_;

  ControllersMap controllers_map_;

  // Stores the last position for each player. This is because a controller
  // may be created after we have already received the position state.
  PositionMap position_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionControllersManager);
};

}  // namespace content

#endif // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLERS_MANAGER_H_

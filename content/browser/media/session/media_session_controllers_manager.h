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
class RenderFrameHost;
class WebContents;

// MediaSessionControllersManager is a delegate of MediaWebContentsObserver that
// handles MediaSessionController instances.
class CONTENT_EXPORT MediaSessionControllersManager {
 public:
  explicit MediaSessionControllersManager(WebContents* web_contents);
  ~MediaSessionControllersManager();

  // Clear all the MediaSessionController associated with the given
  // |render_frame_host|.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host);

  // Called whenever a player's metadata changes.
  void OnMetadata(const MediaPlayerId& id,
                  bool has_audio,
                  bool has_video,
                  media::MediaContentType media_content_type);

  // Called before a player starts playing. It will be added to the media
  // session and will have a controller associated with it.
  // Returns whether the player was added to the session and can start playing.
  bool RequestPlay(const MediaPlayerId& id);

  // Called when the given player |id| has paused.
  void OnPause(const MediaPlayerId& id, bool reached_end_of_stream);

  // Called when the given player |id| has been destroyed.
  void OnEnd(const MediaPlayerId& id);

  // Called when the media position state for the player |id| has changed.
  void OnMediaPositionStateChanged(
      const MediaPlayerId& id,
      const media_session::MediaPosition& position);

  // Called when entering/leaving Picture-in-Picture for the associated
  // WebContents.
  void PictureInPictureStateChanged(bool is_picture_in_picture);

  // Called when the WebContents was muted or unmuted.
  void WebContentsMutedStateChanged(bool muted);

  // Called when picture-in-picture availability for the player |id| has
  // changed.
  void OnPictureInPictureAvailabilityChanged(const MediaPlayerId& id,
                                             bool available);

  // Called when the audio output device for the player |id| has changed.
  void OnAudioOutputSinkChanged(const MediaPlayerId& id,
                                const std::string& raw_device_id);

  // Called when the ability to switch audio output devices for the player |id|
  // has been disabled.
  void OnAudioOutputSinkChangingDisabled(const MediaPlayerId& id);

 private:
  using ControllersMap =
      std::map<MediaPlayerId, std::unique_ptr<MediaSessionController>>;

  // Returns the controller for the player identified by |id|, creating a new
  // one and placing it in |controllers_map_| if necessary.
  MediaSessionController* FindOrCreateController(const MediaPlayerId& id);

  WebContents* const web_contents_;

  ControllersMap controllers_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionControllersManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLERS_MANAGER_H_

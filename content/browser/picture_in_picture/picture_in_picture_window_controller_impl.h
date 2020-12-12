// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include <map>
#include <set>

#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

namespace content {

class PictureInPictureServiceImpl;
class PictureInPictureSession;
class WebContents;
class WebContentsImpl;
enum class PictureInPictureResult;

// PictureInPictureWindowControllerImpl is the corner stone of the
// Picture-in-Picture feature in the //content layer. It handles the session
// creation requests (sent by the PictureInPictureServiceImpl), owns the session
// object and therefore handles its lifetime, and communicate with the rest of
// the browser. Requests to the WebContents are sent by the controller and it
// gets notified when the browser needs it to update the Picture-in-Picture
// session.
// The PictureInPictureWindowControllerImpl is managing Picture-in-Picture at a
// WebContents level. If multiple calls request a Picture-in-Picture session
// either in the same frame or in different frames, the controller will handle
// creating the new session, stopping the current one and making sure the window
// is kept around when possible.
class CONTENT_EXPORT PictureInPictureWindowControllerImpl
    : public PictureInPictureWindowController,
      public WebContentsUserData<PictureInPictureWindowControllerImpl>,
      public WebContentsObserver {
 public:
  // Gets a reference to the controller associated with |web_contents| and
  // creates one if it does not exist. The returned pointer is guaranteed to be
  // non-null.
  static PictureInPictureWindowControllerImpl* GetOrCreateForWebContents(
      WebContents* web_contents);

  ~PictureInPictureWindowControllerImpl() override;

  using PlayerSet = std::set<int>;

  // PictureInPictureWindowController:
  void Show() override;
  void Close(bool should_pause_video) override;
  void CloseAndFocusInitiator() override;
  void OnWindowDestroyed() override;
  OverlayWindow* GetWindowForTesting() override;
  void UpdateLayerBounds() override;
  bool IsPlayerActive() override;
  WebContents* GetWebContents() override;
  bool TogglePlayPause() override;
  void UpdatePlaybackState(bool is_playing,
                           bool reached_end_of_stream) override;
  void SkipAd() override;
  void NextTrack() override;
  void PreviousTrack() override;

  void MediaSessionActionsChanged(
      const std::set<media_session::mojom::MediaSessionAction>& actions);

  gfx::Size GetSize();

  // WebContentsObserver:
  void MediaStartedPlaying(const MediaPlayerInfo&,
                           const MediaPlayerId&) override;
  void MediaStoppedPlaying(const MediaPlayerInfo&,
                           const MediaPlayerId&,
                           WebContentsObserver::MediaStoppedReason) override;
  void WebContentsDestroyed() override;

  // Embeds a surface in the Picture-in-Picture window.
  void EmbedSurface(const viz::SurfaceId& surface_id,
                    const gfx::Size& natural_size);

  void SetShowPlayPauseButton(bool show_play_pause_button);

  // Called by PictureInPictureServiceImpl when a session request is received.
  // The call should return the |session_remote| and |window_size| as out
  // params. A failure to create the session should be expressed with an empty
  // |window_size| and uninitialized |session_remote|.
  // Returns whether the session creation was successful.
  PictureInPictureResult StartSession(
      PictureInPictureServiceImpl* service,
      const MediaPlayerId&,
      const viz::SurfaceId& surface_id,
      const gfx::Size& natural_size,
      bool show_play_pause_button,
      mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>,
      mojo::PendingRemote<blink::mojom::PictureInPictureSession>*
          session_remote,
      gfx::Size* window_size);

  // Called by PictureInPictureServiceImpl when the service is about to be
  // destroyed. It allows |this| to close the |active_session_| if it is
  // associated with the service.
  void OnServiceDeleted(PictureInPictureServiceImpl* service);

  PictureInPictureSession* active_session_for_testing() const {
    return active_session_.get();
  }

 private:
  friend class WebContentsUserData<PictureInPictureWindowControllerImpl>;

  // Use PictureInPictureWindowControllerImpl::GetOrCreateForWebContents() to
  // create an instance.
  explicit PictureInPictureWindowControllerImpl(WebContents* web_contents);

  // Signal to the media player that |this| is leaving Picture-in-Picture mode.
  void OnLeavingPictureInPicture(bool should_pause_video);

  // Internal method to set the states after the window was closed, whether via
  // the system or Chromium.
  void CloseInternal(bool should_pause_video);

  // Creates a new window if the previous one was destroyed. It can happen
  // because of the system control of the window.
  void EnsureWindow();

  // Allow play/pause button to be visible if Media Session actions "play" and
  // "pause" are both handled by the website or if
  // always_hide_play_pause_button_ is false.
  void UpdatePlayPauseButtonVisibility();

  // Returns the web_contents() as a WebContentsImpl*.
  WebContentsImpl* GetWebContentsImpl();

  std::unique_ptr<OverlayWindow> window_;

  viz::SurfaceId surface_id_;

  // Used to show/hide some actions in Picture-in-Picture window. These are set
  // to true when website handles some Media Session actions.
  bool media_session_action_play_handled_ = false;
  bool media_session_action_pause_handled_ = false;
  bool media_session_action_skip_ad_handled_ = false;
  bool media_session_action_next_track_handled_ = false;
  bool media_session_action_previous_track_handled_ = false;

  // Used to hide play/pause button if video is a MediaStream or has infinite
  // duration. Play/pause button visibility can be overridden by the Media
  // Session API in UpdatePlayPauseButtonVisibility().
  bool always_show_play_pause_button_ = false;

  // Session currently associated with the Picture-in-Picture window. The
  // session object makes the bridge with the renderer process by handling
  // requests and holding states such as the active player id.
  // The session will be nullptr when there is no active session.
  std::unique_ptr<PictureInPictureSession> active_session_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureWindowControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

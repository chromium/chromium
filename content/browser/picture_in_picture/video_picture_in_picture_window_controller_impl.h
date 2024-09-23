// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include <map>
#include <set>

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

namespace content {

class PictureInPictureServiceImpl;
class PictureInPictureSession;
class WebContents;
class WebContentsImpl;
enum class PictureInPictureResult;

// VideoPictureInPictureWindowControllerImpl is the corner stone of the video
// Picture-in-Picture feature in the //content layer. It handles the session
// creation requests (sent by the PictureInPictureServiceImpl), owns the session
// object and therefore handles its lifetime, and communicate with the rest of
// the browser. Requests to the WebContents are sent by the controller and it
// gets notified when the browser needs it to update the Picture-in-Picture
// session.
// The VideoPictureInPictureWindowControllerImpl is managing Picture-in-Picture
// at a WebContents level. If multiple calls request a Picture-in-Picture
// session either in the same frame or in different frames, the controller will
// handle creating the new session, stopping the current one and making sure the
// window is kept around when possible.
class CONTENT_EXPORT VideoPictureInPictureWindowControllerImpl
    : public VideoPictureInPictureWindowController,
      public WebContentsUserData<VideoPictureInPictureWindowControllerImpl>,
      public WebContentsObserver {
 public:
  // Gets a reference to the controller associated with |web_contents| and
  // creates one if it does not exist. The returned pointer is guaranteed to be
  // non-null.
  static VideoPictureInPictureWindowControllerImpl* GetOrCreateForWebContents(
      WebContents* web_contents);

  VideoPictureInPictureWindowControllerImpl(
      const VideoPictureInPictureWindowControllerImpl&) = delete;
  VideoPictureInPictureWindowControllerImpl& operator=(
      const VideoPictureInPictureWindowControllerImpl&) = delete;

  ~VideoPictureInPictureWindowControllerImpl() override;

  using PlayerSet = std::set<int>;

  // PictureInPictureWindowController:
  void Show() override;
  void FocusInitiator() override;
  void Close(bool should_pause_video) override;
  void CloseAndFocusInitiator() override;
  void OnWindowDestroyed(bool should_pause_video) override;
  VideoOverlayWindow* GetWindowForTesting() override;
  void UpdateLayerBounds() override;
  bool IsPlayerActive() override;
  WebContents* GetWebContents() override;
  WebContents* GetChildWebContents() override;
  bool TogglePlayPause() override;
  void SkipAd() override;
  void NextTrack() override;
  void PreviousTrack() override;
  void ToggleMicrophone() override;
  void ToggleCamera() override;
  void HangUp() override;
  void PreviousSlide() override;
  void NextSlide() override;
  void SetOnWindowCreatedNotifyObserversCallback(
      base::OnceClosure on_window_created_notify_observers_callback) override;

  const gfx::Rect& GetSourceBounds() const override;
  std::optional<gfx::Rect> GetWindowBounds() override;

  std::optional<url::Origin> GetOrigin() override;
  void SetOrigin(std::optional<url::Origin> origin);

  // Called by the MediaSessionImpl when the MediaSessionInfo changes.
  void MediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& info);

  void MediaSessionActionsChanged(
      const std::set<media_session::mojom::MediaSessionAction>& actions);

  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& media_position);

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
      mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
      const viz::SurfaceId& surface_id,
      const gfx::Size& natural_size,
      bool show_play_pause_button,
      mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>,
      const gfx::Rect& source_bounds,
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

 protected:
  // Use VideoPictureInPictureWindowControllerImpl::GetOrCreateForWebContents()
  // to create an instance.
  explicit VideoPictureInPictureWindowControllerImpl(WebContents* web_contents);

 private:
  friend class WebContentsUserData<VideoPictureInPictureWindowControllerImpl>;

  // Recompute the playback state and update the window accordingly.
  void UpdatePlaybackState();

  // Signal to the media player that |this| is leaving Picture-in-Picture mode.
  void OnLeavingPictureInPicture(bool should_pause_video);

  // Internal method to set the states after the window was closed, whether via
  // the system or by the browser.
  void CloseInternal(bool should_pause_video);

  // Allow play/pause button to be visible if Media Session actions "play" and
  // "pause" are both handled by the website or if
  // always_hide_play_pause_button_ is false.
  void UpdatePlayPauseButtonVisibility();

  // Returns the web_contents() as a WebContentsImpl*.
  WebContentsImpl* GetWebContentsImpl();

  std::unique_ptr<VideoOverlayWindow> window_;

  viz::SurfaceId surface_id_;

  // Used to show/hide some actions in Picture-in-Picture window. These are set
  // to true when website handles some Media Session actions.
  bool media_session_action_play_handled_ = false;
  bool media_session_action_pause_handled_ = false;
  bool media_session_action_skip_ad_handled_ = false;
  bool media_session_action_next_track_handled_ = false;
  bool media_session_action_previous_track_handled_ = false;
  bool media_session_action_toggle_microphone_handled_ = false;
  bool media_session_action_toggle_camera_handled_ = false;
  bool media_session_action_hang_up_handled_ = false;
  bool media_session_action_previous_slide_handled_ = false;
  bool media_session_action_next_slide_handled_ = false;

  // Tracks the current microphone state.
  bool microphone_muted_ = false;

  // Tracks the current camera state.
  bool camera_turned_on_ = false;

  // Used to hide play/pause button if video is a MediaStream or has infinite
  // duration. Play/pause button visibility can be overridden by the Media
  // Session API in UpdatePlayPauseButtonVisibility().
  bool always_show_play_pause_button_ = false;

  // Session currently associated with the Picture-in-Picture window. The
  // session object makes the bridge with the renderer process by handling
  // requests and holding states such as the active player id.
  // The session will be nullptr when there is no active session.
  std::unique_ptr<PictureInPictureSession> active_session_;

  // The media position info as last reported to us by MediaSessionImpl.
  std::optional<media_session::MediaPosition> media_position_;

  // Coordinates of the video element in WebContents coordinates.
  gfx::Rect source_bounds_;

  // The origin of the initiator.
  std::optional<url::Origin> origin_;

  // Callback to notify the observers about the video PiP window creation event.
  base::OnceClosure on_window_created_notify_observers_callback_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

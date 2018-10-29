// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class OverlaySurfaceEmbedder;
class WebContents;
class MediaWebContentsObserver;

// TODO(thakis,mlamouri): PictureInPictureWindowControllerImpl isn't
// CONTENT_EXPORT'd because it creates complicated build issues with
// WebContentsUserData being a non-exported template. As a result, the class
// uses CONTENT_EXPORT for methods that are being used from tests.
// CONTENT_EXPORT should be moved back to the class when the Windows build will
// work with it. https://crbug.com/589840.
class PictureInPictureWindowControllerImpl
    : public PictureInPictureWindowController,
      public WebContentsUserData<PictureInPictureWindowControllerImpl> {
 public:
  // Gets a reference to the controller associated with |initiator| and creates
  // one if it does not exist. The returned pointer is guaranteed to be
  // non-null.
  CONTENT_EXPORT static PictureInPictureWindowControllerImpl*
  GetOrCreateForWebContents(WebContents* initiator);

  ~PictureInPictureWindowControllerImpl() override;

  // PictureInPictureWindowController:
  CONTENT_EXPORT gfx::Size Show() override;
  CONTENT_EXPORT void Close(bool should_pause_video,
                            bool should_reset_pip_player) override;
  CONTENT_EXPORT void OnWindowDestroyed() override;
  CONTENT_EXPORT void SetPictureInPictureCustomControls(
      const std::vector<blink::PictureInPictureControlInfo>& controls) override;
  CONTENT_EXPORT void EmbedSurface(const viz::SurfaceId& surface_id,
                                   const gfx::Size& natural_size) override;
  CONTENT_EXPORT OverlayWindow* GetWindowForTesting() override;
  CONTENT_EXPORT void UpdateLayerBounds() override;
  CONTENT_EXPORT bool IsPlayerActive() override;
  CONTENT_EXPORT WebContents* GetInitiatorWebContents() override;
  CONTENT_EXPORT bool TogglePlayPause() override;
  CONTENT_EXPORT void CustomControlPressed(
      const std::string& control_id) override;
  CONTENT_EXPORT void UpdatePlaybackState(bool is_playing,
                                          bool reached_end_of_stream) override;
  CONTENT_EXPORT void SetAlwaysHidePlayPauseButton(bool is_visible) override;

 private:
  friend class WebContentsUserData<PictureInPictureWindowControllerImpl>;

  // Use PictureInPictureWindowControllerImpl::GetOrCreateForWebContents() to
  // create an instance.
  CONTENT_EXPORT explicit PictureInPictureWindowControllerImpl(
      WebContents* initiator);

  // Signal to the media player that |this| is leaving Picture-in-Picture mode.
  void OnLeavingPictureInPicture(bool should_pause_video,
                                 bool should_reset_pip_player);

  // Internal method to set the states after the window was closed, whether via
  // the system or Chromium.
  void CloseInternal(bool should_pause_video, bool should_reset_pip_player);

  // Creates a new window if the previous one was destroyed. It can happen
  // because of the system control of the window.
  void EnsureWindow();

  std::unique_ptr<OverlayWindow> window_;
  std::unique_ptr<OverlaySurfaceEmbedder> embedder_;
  WebContentsImpl* const initiator_;

  // Used to determine the state of the media player and route messages to
  // the corresponding media player with id |media_player_id_|.
  MediaWebContentsObserver* media_web_contents_observer_;
  base::Optional<WebContentsObserver::MediaPlayerId> media_player_id_;

  viz::SurfaceId surface_id_;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureWindowControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

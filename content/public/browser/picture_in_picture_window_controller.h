// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

#include "content/common/content_export.h"

namespace content {
class OverlayWindow;
class WebContents;

// Interface for Picture in Picture window controllers. This is currently tied
// to a WebContents |initiator| and created when a Picture in Picture window is
// to be shown. This allows creation of a single window for the initiator
// WebContents.
class PictureInPictureWindowController {
 public:
  // Gets a reference to the controller associated with |initiator| and creates
  // one if it does not exist. The returned pointer is guaranteed to be
  // non-null.
  CONTENT_EXPORT static PictureInPictureWindowController*
  GetOrCreateForWebContents(WebContents* initiator);

  virtual ~PictureInPictureWindowController() = default;

  // Shows the Picture-in-Picture window.
  virtual void Show() = 0;

  // Called to notify the controller that the window was requested to be closed
  // by the user or the content.
  virtual void Close(bool should_pause_video) = 0;

  // Called to notify the controller that the window was requested to be closed
  // by the content and that initiator should be focused.
  virtual void CloseAndFocusInitiator() = 0;

  // Called by the window implementation to notify the controller that the
  // window was requested to be closed and destroyed by the system.
  virtual void OnWindowDestroyed() = 0;

  virtual OverlayWindow* GetWindowForTesting() = 0;
  virtual void UpdateLayerBounds() = 0;
  virtual bool IsPlayerActive() = 0;
  virtual WebContents* GetInitiatorWebContents() = 0;
  virtual void UpdatePlaybackState(bool is_playing,
                                   bool reached_end_of_stream) = 0;
  virtual void SetAlwaysHidePlayPauseButton(bool is_visible) = 0;

  // Called when the user interacts with the "Skip Ad" control.
  virtual void SkipAd() = 0;

  // Called when the user interacts with the "Next Track" control.
  virtual void NextTrack() = 0;

  // Called when the user interacts with the "Previous Track" control.
  virtual void PreviousTrack() = 0;

  // Commands.
  // Returns true if the player is active (i.e. currently playing) after this
  // call.
  virtual bool TogglePlayPause() = 0;

 protected:
  // Use PictureInPictureWindowController::GetOrCreateForWebContents() to
  // create an instance.
  PictureInPictureWindowController() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

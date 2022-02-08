// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

#include "content/common/content_export.h"
#include "content/public/browser/picture_in_picture_window_controller.h"

namespace content {
class VideoOverlayWindow;

class VideoPictureInPictureWindowController
    : public PictureInPictureWindowController {
 public:
  virtual VideoOverlayWindow* GetWindowForTesting() = 0;
  virtual void UpdateLayerBounds() = 0;
  virtual bool IsPlayerActive() = 0;

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

  // Called when the user interacts with the "Toggle Microphone" control.
  virtual void ToggleMicrophone() = 0;

  // Called when the user interacts with the "Toggle Camera" control.
  virtual void ToggleCamera() = 0;

  // Called when the user interacts with the "Hang Up" control.
  virtual void HangUp() = 0;

 protected:
  // Use PictureInPictureWindowController::GetOrCreateForWebContents() to
  // create an instance.
  VideoPictureInPictureWindowController() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

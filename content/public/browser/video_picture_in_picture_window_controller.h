// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "ui/gfx/geometry/rect.h"

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

  // Called when the user interacts with the "Previous Slide" control.
  virtual void PreviousSlide() = 0;

  // Called when the user interacts with the "Next Slide" control.
  virtual void NextSlide() = 0;

  // Returns the source bounds of the video, in the WebContents top-level
  // coordinate space, of the video before it enters picture in picture.
  virtual const gfx::Rect& GetSourceBounds() const = 0;

  // Called to set the callback to notify the observers that window has been
  // created.
  virtual void SetOnWindowCreatedNotifyObserversCallback(
      base::OnceClosure on_window_created_notify_observers_callback) = 0;

 protected:
  // Use PictureInPictureWindowController::GetOrCreateForWebContents() to
  // create an instance.
  VideoPictureInPictureWindowController() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

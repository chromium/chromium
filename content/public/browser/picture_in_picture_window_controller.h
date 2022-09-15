// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

#include "content/common/content_export.h"

namespace content {
class WebContents;
class DocumentPictureInPictureWindowController;
class VideoPictureInPictureWindowController;

// Interface for Picture in Picture window controllers. This is currently tied
// to a WebContents |web_contents| and created when a Picture in Picture window
// is to be shown. This allows creation of a single window for the WebContents
// WebContents.
class PictureInPictureWindowController {
 public:
  // Gets a reference to the controller of the appropriate type associated with
  // |web_contents| and creates one if it does not exist. If there is an
  // existing controller, it is reused if it's of the correct type, but is
  // recreated if the existing instance was for a different type. The returned
  // pointer is guaranteed to be non-null.
  CONTENT_EXPORT static VideoPictureInPictureWindowController*
  GetOrCreateVideoPictureInPictureController(WebContents* web_contents);
  CONTENT_EXPORT static DocumentPictureInPictureWindowController*
  GetOrCreateDocumentPictureInPictureController(WebContents* web_contents);

  virtual ~PictureInPictureWindowController() = default;

  // Shows the Picture-in-Picture window.
  virtual void Show() = 0;

  // Called to notify the controller that initiator should be focused.
  virtual void FocusInitiator() = 0;

  // Called to notify the controller that the window was requested to be closed
  // by the user or the content.
  virtual void Close(bool should_pause_video) = 0;

  // Called to notify the controller that the window was requested to be closed
  // by the content and that initiator should be focused.
  virtual void CloseAndFocusInitiator() = 0;

  // Called by the window implementation to notify the controller that the
  // window was requested to be closed and destroyed by the system.
  virtual void OnWindowDestroyed(bool should_pause_video) = 0;

  virtual WebContents* GetWebContents() = 0;

 protected:
  // Use PictureInPictureWindowController::GetOrCreateForWebContents() to
  // create an instance.
  PictureInPictureWindowController() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

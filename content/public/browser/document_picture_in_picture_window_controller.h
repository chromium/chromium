// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

#include "content/common/content_export.h"
#include "content/public/browser/picture_in_picture_window_controller.h"

namespace content {
class WebContents;

class DocumentPictureInPictureWindowController
    : public PictureInPictureWindowController {
 public:
  // Sets the contents inside the Picture in Picture window.
  virtual void SetChildWebContents(WebContents* child_contents) = 0;

 protected:
  // Use PictureInPictureWindowController::GetOrCreateForWebContents() to
  // create an instance.
  DocumentPictureInPictureWindowController() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

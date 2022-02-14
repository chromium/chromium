// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

#include <memory>  // for std::unique_ptr

#include "content/common/content_export.h"
#include "content/public/browser/picture_in_picture_window_controller.h"

namespace content {
class DocumentOverlayWindow;
class WebContents;

class DocumentPictureInPictureWindowController
    : public PictureInPictureWindowController {
 public:
  // Takes ownership of the WebContents for Document Picture-in-Picture.
  virtual void SetChildWebContents(
      std::unique_ptr<WebContents> child_contents) = 0;

  // Returns the child WebContents for DocumentPip
  virtual WebContents* GetChildWebContents() = 0;

  virtual DocumentOverlayWindow* GetWindowForTesting() = 0;

 protected:
  // Use PictureInPictureWindowController::GetOrCreateForWebContents() to
  // create an instance.
  DocumentPictureInPictureWindowController() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

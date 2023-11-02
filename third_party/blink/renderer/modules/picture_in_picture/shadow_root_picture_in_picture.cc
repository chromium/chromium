// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/shadow_root_picture_in_picture.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"

namespace blink {

// static
Element* ShadowRootPictureInPicture::pictureInPictureElement(TreeScope& scope) {
  return PictureInPictureController::From(scope.GetDocument())
      .PictureInPictureElement(scope);
}

}  // namespace blink

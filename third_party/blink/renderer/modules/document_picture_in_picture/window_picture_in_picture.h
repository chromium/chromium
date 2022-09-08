// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_PICTURE_IN_PICTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_PICTURE_IN_PICTURE_H_

#include "third_party/blink/renderer/core/dom/qualified_name.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class PictureInPictureWindowOptions;
class ScriptPromise;
class ScriptState;

class WindowPictureInPicture {
  STATIC_ONLY(WindowPictureInPicture);

 public:
  static ScriptPromise requestPictureInPictureWindow(
      ScriptState*,
      LocalDOMWindow&,
      PictureInPictureWindowOptions*,
      ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_PICTURE_IN_PICTURE_H_

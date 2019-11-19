// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class ScriptPromise;
class ScriptState;
class TreeScope;

class DocumentPictureInPicture {
  STATIC_ONLY(DocumentPictureInPicture);

 public:
  static bool pictureInPictureEnabled(Document&);

  static ScriptPromise exitPictureInPicture(ScriptState*,
                                            Document&,
                                            ExceptionState&);

  static Element* pictureInPictureElement(TreeScope&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_H_

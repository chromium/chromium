// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_DOCUMENT_VIDEO_PICTURE_IN_PICTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_DOCUMENT_VIDEO_PICTURE_IN_PICTURE_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class ScriptPromise;
class ScriptState;
class TreeScope;

class DocumentVideoPictureInPicture {
  STATIC_ONLY(DocumentVideoPictureInPicture);

 public:
  static bool pictureInPictureEnabled(Document&);

  static ScriptPromise exitPictureInPicture(ScriptState*,
                                            Document&,
                                            ExceptionState&);

  static Element* pictureInPictureElement(TreeScope&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_DOCUMENT_VIDEO_PICTURE_IN_PICTURE_H_

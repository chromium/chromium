// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture_session.h"

namespace blink {

DocumentPictureInPictureSession::DocumentPictureInPictureSession(
    LocalDOMWindow* window)
    : window_(window) {}

void DocumentPictureInPictureSession::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

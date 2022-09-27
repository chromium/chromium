// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_picture_in_picture/window_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
ScriptPromise WindowPictureInPicture::requestPictureInPictureWindow(
    ScriptState* script_state,
    LocalDOMWindow& window,
    PictureInPictureWindowOptions* options,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kAbortError,
      "Please use navigator.documentPictureInPicture.requestWindow() instead.");
  return ScriptPromise();
}

}  // namespace blink

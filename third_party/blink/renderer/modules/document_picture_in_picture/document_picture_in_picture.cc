// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture_session.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/window_picture_in_picture.h"

namespace blink {

// static
const char DocumentPictureInPicture::kSupplementName[] =
    "DocumentPictureInPicture";

DocumentPictureInPicture::DocumentPictureInPicture(
    ExecutionContext* execution_context,
    Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

// static
DocumentPictureInPicture* DocumentPictureInPicture::From(
    ExecutionContext* execution_context,
    Navigator& navigator) {
  DocumentPictureInPicture* pip =
      Supplement<Navigator>::From<DocumentPictureInPicture>(navigator);
  if (!pip) {
    pip = MakeGarbageCollected<DocumentPictureInPicture>(execution_context,
                                                         navigator);
    ProvideTo(navigator, pip);
  }
  return pip;
}

// static
DocumentPictureInPicture* DocumentPictureInPicture::documentPictureInPicture(
    ScriptState* script_state,
    Navigator& navigator) {
  return From(ExecutionContext::From(script_state), navigator);
}

ScriptPromise DocumentPictureInPicture::requestWindow(
    ScriptState* script_state,
    PictureInPictureWindowOptions* options,
    ExceptionState& exception_state) {
  LocalDOMWindow* dom_window = LocalDOMWindow::From(script_state);
  if (!dom_window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Internal error: no window");
    return ScriptPromise();
  }

  // TODO(crbug.com/1360443): When this call is inlined here, be sure to
  // replace `PictureInPictureWindowOptions` with a new
  // `DocumentPictureInPictureOptions` type.
  return WindowPictureInPicture::requestPictureInPictureWindow(
      script_state, *dom_window, options, exception_state);
}

DocumentPictureInPictureSession* DocumentPictureInPicture::session(
    ScriptState* script_state) const {
  LocalDOMWindow* dom_window = LocalDOMWindow::From(script_state);
  if (!dom_window)
    return nullptr;
  Document* document = dom_window->document();
  if (!document)
    return nullptr;
  return PictureInPictureControllerImpl::From(*document)
      .documentPictureInPictureSession();
}

void DocumentPictureInPicture::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink

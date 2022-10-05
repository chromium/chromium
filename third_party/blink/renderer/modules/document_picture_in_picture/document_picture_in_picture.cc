// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture_session.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.h"

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
    DocumentPictureInPictureOptions* options,
    ExceptionState& exception_state) {
  LocalDOMWindow* dom_window = LocalDOMWindow::From(script_state);
  if (!dom_window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Internal error: no window");
    return ScriptPromise();
  }

  // TODO(https://crbug.com/1253970): Check if PiP is allowed (e.g. user
  // gesture, permissions, etc).
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "Document not attached");
    return promise;
  }

  // |dom_window->document()| should always exist after document construction.
  auto* document = dom_window->document();
  DCHECK(document);

  PictureInPictureControllerImpl::From(*document)
      .CreateDocumentPictureInPictureWindow(script_state, *dom_window, options,
                                            resolver, exception_state);

  return promise;
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

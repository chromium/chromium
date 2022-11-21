// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.h"

namespace blink {

// static
const char DocumentPictureInPicture::kSupplementName[] =
    "DocumentPictureInPicture";

DocumentPictureInPicture::DocumentPictureInPicture(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

// static
DocumentPictureInPicture* DocumentPictureInPicture::From(
    LocalDOMWindow& window) {
  DocumentPictureInPicture* pip =
      Supplement<LocalDOMWindow>::From<DocumentPictureInPicture>(window);
  if (!pip) {
    pip = MakeGarbageCollected<DocumentPictureInPicture>(window);
    ProvideTo(window, pip);
  }
  return pip;
}

// static
DocumentPictureInPicture* DocumentPictureInPicture::documentPictureInPicture(
    LocalDOMWindow& window) {
  return From(window);
}

const AtomicString& DocumentPictureInPicture::InterfaceName() const {
  return event_target_names::kDocumentPictureInPicture;
}

ExecutionContext* DocumentPictureInPicture::GetExecutionContext() const {
  return GetSupplementable();
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

DOMWindow* DocumentPictureInPicture::window(ScriptState* script_state) const {
  LocalDOMWindow* dom_window = LocalDOMWindow::From(script_state);
  if (!dom_window)
    return nullptr;
  Document* document = dom_window->document();
  if (!document)
    return nullptr;
  return PictureInPictureControllerImpl::From(*document)
      .documentPictureInPictureWindow();
}

void DocumentPictureInPicture::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink

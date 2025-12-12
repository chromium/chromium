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

ScriptPromise<DOMWindow> DocumentPictureInPicture::requestWindow(
    ScriptState* script_state,
    DocumentPictureInPictureOptions* options,
    ExceptionState& exception_state) {
  LocalDOMWindow* dom_window = LocalDOMWindow::From(script_state);
  if (!dom_window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Internal error: no window");
    return EmptyPromise();
  }

  if (dom_window->GetFrame() &&
      !dom_window->GetFrame()->IsOutermostMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Opening a PiP window is only allowed "
                                      "from a top-level browsing context");
    return EmptyPromise();
  }

  if (dom_window->IsPictureInPictureWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Opening a PiP window from a PiP window is not allowed");
    return EmptyPromise();
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "Document not attached");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<DOMWindow>>(
      script_state, exception_state.GetContext());
  // |dom_window->document()| should always exist after document construction.
  auto* document = dom_window->document();
  DCHECK(document);

  auto promise = resolver->Promise();
  PictureInPictureControllerImpl::From(*document)
      .CreateDocumentPictureInPictureWindow(script_state, *dom_window, options,
                                            resolver);

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
  EventTarget::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void DocumentPictureInPicture::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  if (event_type == event_type_names::kEnter) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kDocumentPictureInPictureEnterEvent);
  }
  EventTarget::AddedEventListener(event_type, registered_listener);
}

}  // namespace blink

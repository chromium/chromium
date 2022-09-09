// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_picture_in_picture/window_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
ScriptPromise WindowPictureInPicture::requestPictureInPictureWindow(
    ScriptState* script_state,
    LocalDOMWindow& window,
    PictureInPictureWindowOptions* options,
    ExceptionState& exception_state) {
  // TODO(https://crbug.com/1253970): Check if PiP is allowed (e.g. user
  // gesture, permissions, etc).
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "Document not attached");
    resolver->Reject();
    return promise;
  }

  // |window.document()| should always exist after document construction.
  auto* document = window.document();
  DCHECK(document);

  PictureInPictureControllerImpl::From(*document)
      .CreateDocumentPictureInPictureWindow(script_state, window, options,
                                            resolver, exception_state);

  return promise;
}

}  // namespace blink

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/delegated_ink/ink.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ink_presenter_param.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/delegated_ink/delegated_ink_trail_presenter.h"

namespace blink {

Ink* Ink::ink(Navigator& navigator) {
  Ink* ink = navigator.GetInk();
  if (!ink) {
    ink = MakeGarbageCollected<Ink>(navigator);
    navigator.SetInk(ink);
  }
  return ink;
}

Ink::Ink(Navigator& navigator) : navigator_(navigator) {}

ScriptPromise<DelegatedInkTrailPresenter> Ink::requestPresenter(
    ScriptState* state,
    InkPresenterParam* presenter_param) {
  if (!state->ContextIsValid()) {
    V8ThrowException::ThrowError(
        state->GetIsolate(),
        "The object is no longer associated with a window.");
    return EmptyPromise();
  }

  if (presenter_param->presentationArea() &&
      (presenter_param->presentationArea()->GetDocument() !=
       navigator_->DomWindow()->GetFrame()->GetDocument())) {
    V8ThrowDOMException::Throw(
        state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "Presentation area element does not belong to the document.");
    return EmptyPromise();
  }

  return ToResolvedPromise<DelegatedInkTrailPresenter>(
      state, MakeGarbageCollected<DelegatedInkTrailPresenter>(
                 presenter_param->presentationArea(),
                 navigator_->DomWindow()->GetFrame()));
}

void Ink::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(navigator_);
}

}  // namespace blink

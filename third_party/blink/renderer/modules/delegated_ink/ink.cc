// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/delegated_ink/ink.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ink_presenter_param.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/delegated_ink/delegated_ink_trail_presenter.h"

namespace blink {

const char Ink::kSupplementName[] = "Ink";

Ink* Ink::ink(Navigator& navigator) {
  Ink* ink = Supplement<Navigator>::From<Ink>(navigator);
  if (!ink) {
    ink = MakeGarbageCollected<Ink>(navigator);
    ProvideTo(navigator, ink);
  }
  return ink;
}

Ink::Ink(Navigator& navigator) : Supplement<Navigator>(navigator) {}

ScriptPromise<DelegatedInkTrailPresenter> Ink::requestPresenter(
    ScriptState* state,
    InkPresenterParam* presenter_param,
    ExceptionState& exception_state) {
  if (!state->ContextIsValid()) {
    exception_state.ThrowException(
        ToExceptionCode(ESErrorType::kError),
        "The object is no longer associated with a window.");
    return EmptyPromise();
  }

  if (presenter_param->presentationArea() &&
      (presenter_param->presentationArea()->GetDocument() !=
       GetSupplementable()->DomWindow()->GetFrame()->GetDocument())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Presentation area element does not belong to the document.");
    return EmptyPromise();
  }

  return ToResolvedPromise<DelegatedInkTrailPresenter>(
      state, MakeGarbageCollected<DelegatedInkTrailPresenter>(
                 presenter_param->presentationArea(),
                 GetSupplementable()->DomWindow()->GetFrame()));
}

void Ink::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink

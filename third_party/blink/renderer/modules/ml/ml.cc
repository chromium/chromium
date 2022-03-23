// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

ML::ML(ExecutionContext* execution_context)
    : execution_context_(execution_context) {}

void ML::CreateModelLoader(ScriptState* script_state,
                           ExceptionState& exception_state) {
  NOTREACHED() << "Not Implemented yet";
}

void ML::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise ML::createContext(ScriptState* script_state,
                                MLContextOptions* option,
                                ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  auto promise = resolver->Promise();

  // Notice that currently, we just create the context in the renderer. In the
  // future we may add backend query ability to check whether a context is
  // supportable or not. At that time, this function will be truly asynced.
  auto* ml_context = MakeGarbageCollected<MLContext>(
      option->devicePreference(), option->powerPreference(),
      option->modelFormat(), option->numThreads(), this);
  resolver->Resolve(ml_context);

  return promise;
}

}  // namespace blink

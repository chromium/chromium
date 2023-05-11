// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/environment_integrity/navigator_environment_integrity.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

NavigatorEnvironmentIntegrity::NavigatorEnvironmentIntegrity(
    Navigator& navigator)
    : Supplement(navigator) {}

NavigatorEnvironmentIntegrity& NavigatorEnvironmentIntegrity::From(
    ExecutionContext* context,
    Navigator& navigator) {
  NavigatorEnvironmentIntegrity* supplement =
      Supplement<Navigator>::From<NavigatorEnvironmentIntegrity>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorEnvironmentIntegrity>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

const char NavigatorEnvironmentIntegrity::kSupplementName[] =
    "NavigatorEnvironmentIntegrity";

ScriptPromise NavigatorEnvironmentIntegrity::getEnvironmentIntegrity(
    ScriptState* script_state,
    const String& content_binding,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                   "Operation not supported");

  return promise;
}

/* static */
ScriptPromise NavigatorEnvironmentIntegrity::getEnvironmentIntegrity(
    ScriptState* script_state,
    Navigator& navigator,
    const String& content_binding,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .getEnvironmentIntegrity(script_state, content_binding, exception_state);
}

void NavigatorEnvironmentIntegrity::Trace(Visitor* visitor) const {
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink

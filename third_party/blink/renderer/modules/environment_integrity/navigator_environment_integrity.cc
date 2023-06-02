// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/environment_integrity/navigator_environment_integrity.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/environment_integrity/environment_integrity.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

#if BUILDFLAG(IS_ANDROID)
NavigatorEnvironmentIntegrity::NavigatorEnvironmentIntegrity(
    Navigator& navigator)
    : Supplement(navigator),
      remote_environment_integrity_service_(navigator.GetExecutionContext()) {
  navigator.GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_environment_integrity_service_.BindNewPipeAndPassReceiver(
          navigator.GetExecutionContext()->GetTaskRunner(
              TaskType::kMiscPlatformAPI)));
}
#else
NavigatorEnvironmentIntegrity::NavigatorEnvironmentIntegrity(
    Navigator& navigator)
    : Supplement(navigator) {}
#endif

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

#if BUILDFLAG(IS_ANDROID)
  remote_environment_integrity_service_->GetEnvironmentIntegrity(
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorEnvironmentIntegrity::ResolveEnvironmentIntegrity,
          WrapPersistent(this))));
#else
  resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                   "Operation not supported");
#endif

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
#if BUILDFLAG(IS_ANDROID)
  visitor->Trace(remote_environment_integrity_service_);
#endif
  Supplement<Navigator>::Trace(visitor);
}

#if BUILDFLAG(IS_ANDROID)
void NavigatorEnvironmentIntegrity::ResolveEnvironmentIntegrity(
    ScriptPromiseResolver* resolver) {
  Vector<uint8_t> empty_token;
  DOMArrayBuffer* buffer =
      DOMArrayBuffer::Create(empty_token.data(), empty_token.size());
  EnvironmentIntegrity* environment_integrity =
      MakeGarbageCollected<EnvironmentIntegrity>(buffer);
  resolver->Resolve(environment_integrity);
}
#endif

}  // namespace blink

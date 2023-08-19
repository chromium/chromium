// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/install_event.h"

#include "third_party/blink/public/mojom/service_worker/service_worker_router_rule.mojom-blink.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_routerrule_routerrulesequence.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_router_type_converter.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

namespace {

void DidRegisterRouter(ScriptPromiseResolver* resolver) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }
  resolver->Resolve();
}

}  // namespace

InstallEvent* InstallEvent::Create(const AtomicString& type,
                                   const ExtendableEventInit* event_init) {
  return MakeGarbageCollected<InstallEvent>(type, event_init);
}

InstallEvent* InstallEvent::Create(const AtomicString& type,
                                   const ExtendableEventInit* event_init,
                                   int event_id,
                                   WaitUntilObserver* observer) {
  return MakeGarbageCollected<InstallEvent>(type, event_init, event_id,
                                            observer);
}

InstallEvent::~InstallEvent() = default;

const AtomicString& InstallEvent::InterfaceName() const {
  return event_interface_names::kInstallEvent;
}

InstallEvent::InstallEvent(const AtomicString& type,
                           const ExtendableEventInit* initializer)
    : ExtendableEvent(type, initializer), event_id_(0) {}

InstallEvent::InstallEvent(const AtomicString& type,
                           const ExtendableEventInit* initializer,
                           int event_id,
                           WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer), event_id_(event_id) {}

ScriptPromise InstallEvent::registerRouter(
    ScriptState* script_state,
    const V8UnionRouterRuleOrRouterRuleSequence* v8_rules,
    ExceptionState& exception_state) {
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));
  if (!global_scope) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowDOMException::CreateOrDie(script_state->GetIsolate(),
                                         DOMExceptionCode::kInvalidStateError,
                                         "No ServiceWorkerGlobalScope."));
  }
  if (did_register_router_) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "registerRouter is called multiple times."));
  }

  blink::ServiceWorkerRouterRules rules;
  if (v8_rules->IsRouterRule()) {
    auto r = ConvertV8RouterRuleToBlink(
        v8_rules->GetAsRouterRule(), global_scope->BaseURL(), exception_state);
    if (!r) {
      CHECK(exception_state.HadException());
      return ScriptPromise::Reject(script_state, exception_state);
    }
    rules.rules.emplace_back(*r);
  } else {
    CHECK(v8_rules->IsRouterRuleSequence());
    for (const blink::RouterRule* rule : v8_rules->GetAsRouterRuleSequence()) {
      auto r = ConvertV8RouterRuleToBlink(rule, global_scope->BaseURL(),
                                          exception_state);
      if (!r) {
        CHECK(exception_state.HadException());
        return ScriptPromise::Reject(script_state, exception_state);
      }
      rules.rules.emplace_back(*r);
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  global_scope->GetServiceWorkerHost()->RegisterRouter(
      rules, WTF::BindOnce(&DidRegisterRouter, WrapPersistent(resolver)));
  did_register_router_ = true;
  return resolver->Promise();
}

}  // namespace blink

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/navigation_preload_manager.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"

namespace blink {

ScriptPromise<IDLUndefined> NavigationPreloadManager::enable(
    ScriptState* script_state) {
  return SetEnabled(true, script_state);
}

ScriptPromise<IDLUndefined> NavigationPreloadManager::disable(
    ScriptState* script_state) {
  return SetEnabled(false, script_state);
}

ScriptPromise<IDLUndefined> NavigationPreloadManager::setHeaderValue(
    ScriptState* script_state,
    const String& value,
    ExceptionState& exception_state) {
  if (!IsValidHTTPHeaderValue(value)) {
    exception_state.ThrowTypeError(
        "The string provided to setHeaderValue ('" + value +
        "') is not a valid HTTP header field value.");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  registration_->SetNavigationPreloadHeader(value, resolver);
  return promise;
}

ScriptPromise<NavigationPreloadState> NavigationPreloadManager::getState(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<NavigationPreloadState>>(
          script_state);
  auto promise = resolver->Promise();
  registration_->GetNavigationPreloadState(resolver);
  return promise;
}

NavigationPreloadManager::NavigationPreloadManager(
    ServiceWorkerRegistration* registration)
    : registration_(registration) {}

ScriptPromise<IDLUndefined> NavigationPreloadManager::SetEnabled(
    bool enable,
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  registration_->EnableNavigationPreload(enable, resolver);
  return promise;
}

void NavigationPreloadManager::Trace(Visitor* visitor) const {
  visitor->Trace(registration_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

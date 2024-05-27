// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_manager.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_push_subscription_options_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/push_messaging/push_error.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_bridge.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_client.h"
#include "third_party/blink/renderer/modules/push_messaging/push_provider.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_callbacks.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {
namespace {

PushProvider* GetPushProvider(
    ServiceWorkerRegistration* service_worker_registration) {
  PushProvider* push_provider = PushProvider::From(service_worker_registration);
  DCHECK(push_provider);
  return push_provider;
}

}  // namespace

PushManager::PushManager(ServiceWorkerRegistration* registration)
    : registration_(registration) {
  DCHECK(registration);
}

// static
Vector<String> PushManager::supportedContentEncodings() {
  return Vector<String>({"aes128gcm", "aesgcm"});
}

namespace {
bool ValidateOptions(blink::PushSubscriptionOptions* options,
                     ExceptionState& exception_state) {
  DOMArrayBuffer* buffer = options->applicationServerKey();
  if (!base::CheckedNumeric<wtf_size_t>(buffer->ByteLength()).IsValid()) {
    exception_state.ThrowRangeError(
        "ApplicationServerKey size exceeded the maximum supported size");
    return false;
  }
  return true;
}
}  // namespace

ScriptPromise<PushSubscription> PushManager::subscribe(
    ScriptState* script_state,
    const PushSubscriptionOptionsInit* options_init,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Window is detached.");
    return EmptyPromise();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "subscribe() is not allowed in fenced frames.");
    return EmptyPromise();
  }

  if (!registration_->active()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kAbortError,
        "Subscription failed - no active Service Worker");
    return EmptyPromise();
  }

  PushSubscriptionOptions* options =
      PushSubscriptionOptions::FromOptionsInit(options_init, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  if (!ValidateOptions(options, exception_state))
    return EmptyPromise();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PushSubscription>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // The window is the only reasonable context from which to ask the
  // user for permission to use the Push API. The embedder should persist the
  // permission so that later calls in different contexts can succeed.
  if (auto* window = LocalDOMWindow::From(script_state)) {
    PushMessagingClient* messaging_client = PushMessagingClient::From(*window);
    DCHECK(messaging_client);

    messaging_client->Subscribe(
        registration_, options,
        LocalFrame::HasTransientUserActivation(window->GetFrame()),
        std::make_unique<PushSubscriptionCallbacks>(resolver,
                                                    /*null_allowed=*/false));
  } else {
    GetPushProvider(registration_)
        ->Subscribe(options, LocalFrame::HasTransientUserActivation(nullptr),
                    std::make_unique<PushSubscriptionCallbacks>(
                        resolver, /*null_allowed=*/false));
  }

  return promise;
}

ScriptPromise<IDLNullable<PushSubscription>> PushManager::getSubscription(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<PushSubscription>>>(script_state);
  auto promise = resolver->Promise();

  GetPushProvider(registration_)
      ->GetSubscription(std::make_unique<PushSubscriptionCallbacks>(
          resolver, /*null_allowed=*/true));
  return promise;
}

ScriptPromise<V8PermissionState> PushManager::permissionState(
    ScriptState* script_state,
    const PushSubscriptionOptionsInit* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Window is detached.");
    return EmptyPromise();
  }

  return PushMessagingBridge::From(registration_)
      ->GetPermissionState(script_state, options);
}

void PushManager::Trace(Visitor* visitor) const {
  visitor->Trace(registration_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

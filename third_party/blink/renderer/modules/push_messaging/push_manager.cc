// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_manager.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/push_messaging/push_error.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_bridge.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_client.h"
#include "third_party/blink/renderer/modules/push_messaging/push_provider.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_callbacks.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options_init.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

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

ScriptPromise PushManager::subscribe(
    ScriptState* script_state,
    const PushSubscriptionOptionsInit* options_init,
    ExceptionState& exception_state) {
  if (!registration_->active()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kAbortError,
                          "Subscription failed - no active Service Worker"));
  }

  PushSubscriptionOptions* options =
      PushSubscriptionOptions::FromOptionsInit(options_init, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (!options->IsApplicationServerKeyVapid()) {
    ExecutionContext::From(script_state)
        ->AddConsoleMessage(ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "The provided application server key is not a VAPID key. Only "
            "VAPID keys will be supported in the future. For more information "
            "check https://crbug.com/979235."));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // The document context is the only reasonable context from which to ask the
  // user for permission to use the Push API. The embedder should persist the
  // permission so that later calls in different contexts can succeed.
  if (auto* document =
          DynamicTo<Document>(ExecutionContext::From(script_state))) {
    LocalFrame* frame = document->GetFrame();
    if (!document->domWindow() || !frame) {
      return ScriptPromise::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kInvalidStateError,
                            "Document is detached from window."));
    }

    PushMessagingClient* messaging_client = PushMessagingClient::From(frame);
    DCHECK(messaging_client);

    messaging_client->Subscribe(
        registration_, options, LocalFrame::HasTransientUserActivation(frame),
        std::make_unique<PushSubscriptionCallbacks>(resolver, registration_));
  } else {
    GetPushProvider(registration_)
        ->Subscribe(options, LocalFrame::HasTransientUserActivation(nullptr),
                    std::make_unique<PushSubscriptionCallbacks>(resolver,
                                                                registration_));
  }

  return promise;
}

ScriptPromise PushManager::getSubscription(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  GetPushProvider(registration_)
      ->GetSubscription(
          std::make_unique<PushSubscriptionCallbacks>(resolver, registration_));
  return promise;
}

ScriptPromise PushManager::permissionState(
    ScriptState* script_state,
    const PushSubscriptionOptionsInit* options,
    ExceptionState& exception_state) {
  if (auto* document =
          DynamicTo<Document>(ExecutionContext::From(script_state))) {
    if (!document->domWindow() || !document->GetFrame()) {
      return ScriptPromise::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kInvalidStateError,
                            "Document is detached from window."));
    }
  }

  return PushMessagingBridge::From(registration_)
      ->GetPermissionState(script_state, options);
}

void PushManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

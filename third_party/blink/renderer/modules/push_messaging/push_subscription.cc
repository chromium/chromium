// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/push_messaging/push_error.h"
#include "third_party/blink/renderer/modules/push_messaging/push_provider.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// This method and its dependencies must remain constant time, thus not branch
// based on the value of |buffer| while encoding, assuming a known length.
String ToBase64URLWithoutPadding(DOMArrayBuffer* buffer) {
  String value = WTF::Base64URLEncode(static_cast<const char*>(buffer->Data()),
                                      buffer->DeprecatedByteLengthAsUnsigned());
  DCHECK_GT(value.length(), 0u);

  unsigned padding_to_remove = 0;
  for (unsigned position = value.length() - 1; position; --position) {
    if (value[position] != '=')
      break;

    ++padding_to_remove;
  }

  DCHECK_LT(padding_to_remove, 4u);
  DCHECK_GT(value.length(), padding_to_remove);

  value.Truncate(value.length() - padding_to_remove);
  return value;
}

}  // namespace

// static
PushSubscription* PushSubscription::Create(
    mojom::blink::PushSubscriptionPtr subscription,
    ServiceWorkerRegistration* service_worker_registration) {
  return MakeGarbageCollected<PushSubscription>(
      subscription->endpoint, subscription->options->user_visible_only,
      subscription->options->application_server_key, subscription->p256dh,
      subscription->auth, service_worker_registration);
}

PushSubscription::PushSubscription(
    const KURL& endpoint,
    bool user_visible_only,
    const WTF::Vector<uint8_t>& application_server_key,
    const WTF::Vector<unsigned char>& p256dh,
    const WTF::Vector<unsigned char>& auth,
    ServiceWorkerRegistration* service_worker_registration)
    : endpoint_(endpoint),
      options_(PushSubscriptionOptions::Create(user_visible_only,
                                               application_server_key)),
      p256dh_(DOMArrayBuffer::Create(p256dh.data(),
                                     SafeCast<unsigned>(p256dh.size()))),
      auth_(
          DOMArrayBuffer::Create(auth.data(), SafeCast<unsigned>(auth.size()))),
      service_worker_registration_(service_worker_registration) {}

PushSubscription::~PushSubscription() = default;

DOMTimeStamp PushSubscription::expirationTime(bool& out_is_null) const {
  // This attribute reflects the time at which the subscription will expire,
  // which is not relevant to this implementation yet as subscription refreshes
  // are not supported.
  out_is_null = true;

  return 0;
}

DOMArrayBuffer* PushSubscription::getKey(const AtomicString& name) const {
  if (name == "p256dh")
    return p256dh_;
  if (name == "auth")
    return auth_;

  return nullptr;
}

ScriptPromise PushSubscription::unsubscribe(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  PushProvider* push_provider =
      PushProvider::From(service_worker_registration_);
  DCHECK(push_provider);
  push_provider->Unsubscribe(
      std::make_unique<CallbackPromiseAdapter<bool, DOMException*>>(resolver));
  return promise;
}

ScriptValue PushSubscription::toJSONForBinding(ScriptState* script_state) {
  DCHECK(p256dh_);

  V8ObjectBuilder result(script_state);
  result.AddString("endpoint", endpoint());
  result.AddNull("expirationTime");

  V8ObjectBuilder keys(script_state);
  keys.Add("p256dh", ToBase64URLWithoutPadding(p256dh_));
  keys.Add("auth", ToBase64URLWithoutPadding(auth_));

  result.Add("keys", keys);

  return result.GetScriptValue();
}

void PushSubscription::Trace(blink::Visitor* visitor) {
  visitor->Trace(options_);
  visitor->Trace(p256dh_);
  visitor->Trace(auth_);
  visitor->Trace(service_worker_registration_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

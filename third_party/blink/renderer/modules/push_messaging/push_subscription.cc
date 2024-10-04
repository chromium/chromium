// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"

#include <memory>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_push_encryption_key_name.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/push_messaging/push_error.h"
#include "third_party/blink/renderer/modules/push_messaging/push_provider.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// This method and its dependencies must remain constant time, thus not branch
// based on the value of |buffer| while encoding, assuming a known length.
String ToBase64URLWithoutPadding(DOMArrayBuffer* buffer) {
  String value = WTF::Base64URLEncode(buffer->ByteSpan());
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

// Converts a {std::optional<base::Time>} into a
// {std::optional<base::DOMTimeStamp>} object.
// base::Time is in milliseconds from Windows epoch (1601-01-01 00:00:00 UTC)
// while blink::DOMTimeStamp is in milliseconds from UNIX epoch (1970-01-01
// 00:00:00 UTC)
std::optional<blink::DOMTimeStamp> ToDOMTimeStamp(
    const std::optional<base::Time>& time) {
  if (time)
    return ConvertSecondsToDOMTimeStamp(time->InSecondsFSinceUnixEpoch());

  return std::nullopt;
}

}  // namespace

// static
PushSubscription* PushSubscription::Create(
    mojom::blink::PushSubscriptionPtr subscription,
    ServiceWorkerRegistration* service_worker_registration) {
  return MakeGarbageCollected<PushSubscription>(
      subscription->endpoint, subscription->options->user_visible_only,
      subscription->options->application_server_key, subscription->p256dh,
      subscription->auth, ToDOMTimeStamp(subscription->expirationTime),
      service_worker_registration);
}

PushSubscription::PushSubscription(
    const KURL& endpoint,
    bool user_visible_only,
    const WTF::Vector<uint8_t>& application_server_key,
    const WTF::Vector<unsigned char>& p256dh,
    const WTF::Vector<unsigned char>& auth,
    const std::optional<DOMTimeStamp>& expiration_time,
    ServiceWorkerRegistration* service_worker_registration)
    : endpoint_(endpoint),
      options_(MakeGarbageCollected<PushSubscriptionOptions>(
          user_visible_only,
          application_server_key)),
      p256dh_(DOMArrayBuffer::Create(p256dh)),
      auth_(DOMArrayBuffer::Create(auth)),
      expiration_time_(expiration_time),
      service_worker_registration_(service_worker_registration) {}

PushSubscription::~PushSubscription() = default;

std::optional<DOMTimeStamp> PushSubscription::expirationTime() const {
  // This attribute reflects the time at which the subscription will expire,
  // which is not relevant to this implementation yet as subscription refreshes
  // are not supported.
  return expiration_time_;
}

DOMArrayBuffer* PushSubscription::getKey(
    const V8PushEncryptionKeyName& name) const {
  switch (name.AsEnum()) {
    case V8PushEncryptionKeyName::Enum::kP256Dh:
      return p256dh_.Get();
    case V8PushEncryptionKeyName::Enum::kAuth:
      return auth_.Get();
  }
  NOTREACHED();
}

ScriptPromise<IDLBoolean> PushSubscription::unsubscribe(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);
  auto promise = resolver->Promise();

  PushProvider* push_provider =
      PushProvider::From(service_worker_registration_);
  DCHECK(push_provider);
  push_provider->Unsubscribe(
      std::make_unique<CallbackPromiseAdapter<IDLBoolean, DOMException>>(
          resolver));
  return promise;
}

ScriptValue PushSubscription::toJSONForBinding(ScriptState* script_state) {
  DCHECK(p256dh_);

  V8ObjectBuilder result(script_state);
  result.AddString("endpoint", endpoint().GetString());

  if (expiration_time_) {
    result.AddNumber("expirationTime", *expiration_time_);
  } else {
    result.AddNull("expirationTime");
  }

  V8ObjectBuilder keys(script_state);
  keys.AddString("p256dh", ToBase64URLWithoutPadding(p256dh_));
  keys.AddString("auth", ToBase64URLWithoutPadding(auth_));

  result.Add("keys", keys);

  return result.GetScriptValue();
}

void PushSubscription::Trace(Visitor* visitor) const {
  visitor->Trace(options_);
  visitor->Trace(p256dh_);
  visitor->Trace(auth_);
  visitor->Trace(service_worker_registration_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

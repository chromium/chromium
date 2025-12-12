// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_provider.h"

#include <utility>

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/push_messaging/push_error.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_utils.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/modules/push_messaging/push_type_converter.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
const char PushProvider::kSupplementName[] = "PushProvider";

PushProvider::PushProvider(ServiceWorkerRegistration& registration)
    : Supplement<ServiceWorkerRegistration>(registration),
      push_messaging_manager_(registration.GetExecutionContext()) {}

// static
PushProvider* PushProvider::From(ServiceWorkerRegistration* registration) {
  DCHECK(registration);

  PushProvider* provider =
      Supplement<ServiceWorkerRegistration>::From<PushProvider>(registration);

  if (!provider) {
    provider = MakeGarbageCollected<PushProvider>(*registration);
    ProvideTo(*registration, provider);
  }

  return provider;
}

// static
mojom::blink::PushMessaging* PushProvider::GetPushMessagingRemote() {
  if (!push_messaging_manager_.is_bound()) {
    GetSupplementable()
        ->GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .GetInterface(push_messaging_manager_.BindNewPipeAndPassReceiver(
            GetSupplementable()->GetExecutionContext()->GetTaskRunner(
                TaskType::kMiscPlatformAPI)));
  }

  return push_messaging_manager_.get();
}

void PushProvider::Subscribe(
    PushSubscriptionOptions* options,
    bool user_gesture,
    ScriptPromiseResolver<PushSubscription>* resolver) {
  DCHECK(resolver);

  mojom::blink::PushSubscriptionOptionsPtr content_options_ptr =
      mojo::ConvertTo<mojom::blink::PushSubscriptionOptionsPtr>(options);

  GetPushMessagingRemote()->Subscribe(
      GetSupplementable()->RegistrationId(), std::move(content_options_ptr),
      user_gesture,
      BindOnce(&PushProvider::DidSubscribe, WrapPersistent(this),
               WrapPersistent(resolver)));
}

void PushProvider::DidSubscribe(
    ScriptPromiseResolver<PushSubscription>* resolver,
    mojom::blink::PushRegistrationStatus status,
    mojom::blink::PushSubscriptionPtr subscription) {
  DCHECK(resolver);

  if (status ==
          mojom::blink::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE ||
      status == mojom::blink::PushRegistrationStatus::
                    SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE ||
      status == mojom::blink::PushRegistrationStatus::SUCCESS_FROM_CACHE) {
    DCHECK(subscription);

    resolver->Resolve(
        PushSubscription::Create(std::move(subscription), GetSupplementable()));
  } else {
    resolver->Reject(PushError::CreateException(
        PushRegistrationStatusToPushErrorType(status),
        PushRegistrationStatusToString(status)));
  }
}

void PushProvider::Unsubscribe(ScriptPromiseResolver<IDLBoolean>* resolver) {
  DCHECK(resolver);

  GetPushMessagingRemote()->Unsubscribe(
      GetSupplementable()->RegistrationId(),
      BindOnce(&PushProvider::DidUnsubscribe, WrapPersistent(this),
               WrapPersistent(resolver)));
}

void PushProvider::DidUnsubscribe(ScriptPromiseResolver<IDLBoolean>* resolver,
                                  mojom::blink::PushErrorType error_type,
                                  bool did_unsubscribe,
                                  const String& error_message) {
  DCHECK(resolver);

  // ErrorTypeNone indicates success.
  if (error_type == mojom::blink::PushErrorType::NONE) {
    resolver->Resolve(did_unsubscribe);
  } else {
    resolver->Reject(PushError::CreateException(error_type, error_message));
  }
}

void PushProvider::GetSubscription(
    ScriptPromiseResolver<IDLNullable<PushSubscription>>* resolver) {
  DCHECK(resolver);

  GetPushMessagingRemote()->GetSubscription(
      GetSupplementable()->RegistrationId(),
      BindOnce(&PushProvider::DidGetSubscription, WrapPersistent(this),
               WrapPersistent(resolver)));
}

void PushProvider::Trace(Visitor* visitor) const {
  visitor->Trace(push_messaging_manager_);
  Supplement::Trace(visitor);
}

void PushProvider::DidGetSubscription(
    ScriptPromiseResolver<IDLNullable<PushSubscription>>* resolver,
    mojom::blink::PushGetRegistrationStatus status,
    mojom::blink::PushSubscriptionPtr subscription) {
  DCHECK(resolver);

  if (status == mojom::blink::PushGetRegistrationStatus::SUCCESS) {
    DCHECK(subscription);

    resolver->Resolve(
        PushSubscription::Create(std::move(subscription), GetSupplementable()));
  } else {
    // We are only expecting an error if we can't find a registration.
    resolver->Resolve(nullptr);
  }
}

}  // namespace blink

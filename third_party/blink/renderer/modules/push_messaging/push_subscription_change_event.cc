// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_subscription_change_event.h"

#include "third_party/blink/renderer/modules/push_messaging/push_subscription_change_event_init.h"

namespace blink {

PushSubscriptionChangeEvent::PushSubscriptionChangeEvent(
    const AtomicString& type,
    PushSubscription* new_subscription,
    PushSubscription* old_subscription,
    WaitUntilObserver* observer)
    : ExtendableEvent(type, ExtendableEventInit::Create(), observer),
      new_subscription_(new_subscription),
      old_subscription_(old_subscription) {}

PushSubscriptionChangeEvent::PushSubscriptionChangeEvent(
    const AtomicString& type,
    PushSubscriptionChangeEventInit* initializer)
    : ExtendableEvent(type, initializer) {
  if (initializer->hasNewSubscription())
    new_subscription_ = initializer->newSubscription();
  if (initializer->hasOldSubscription())
    old_subscription_ = initializer->oldSubscription();
}

PushSubscriptionChangeEvent::~PushSubscriptionChangeEvent() = default;

PushSubscription* PushSubscriptionChangeEvent::newSubscription() const {
  return new_subscription_;
}

PushSubscription* PushSubscriptionChangeEvent::oldSubscription() const {
  return old_subscription_;
}

void PushSubscriptionChangeEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(new_subscription_);
  visitor->Trace(old_subscription_);
  ExtendableEvent::Trace(visitor);
}

}  // namespace blink

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_subscription_callbacks.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"

namespace blink {

PushSubscriptionCallbacks::PushSubscriptionCallbacks(
    ScriptPromiseResolverBase* resolver,
    bool null_allowed)
    : resolver_(resolver), null_allowed_(null_allowed) {
  DCHECK(resolver_);
}

PushSubscriptionCallbacks::~PushSubscriptionCallbacks() = default;

void PushSubscriptionCallbacks::OnSuccess(PushSubscription* push_subscription) {
  if (null_allowed_) {
    resolver_->DowncastTo<IDLNullable<PushSubscription>>()->Resolve(
        push_subscription);
  } else {
    resolver_->DowncastTo<PushSubscription>()->Resolve(push_subscription);
  }
}

void PushSubscriptionCallbacks::OnError(DOMException* error) {
  resolver_->Reject(error);
}

}  // namespace blink

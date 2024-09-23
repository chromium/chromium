// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_SUBSCRIPTION_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_SUBSCRIPTION_CALLBACKS_H_

#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class DOMException;
class PushSubscription;
class ScriptPromiseResolverBase;

// Used from PushProvider, for calls to PushMessaging::Unsubscribe().
using PushUnsubscribeCallbacks = WebCallbacks<bool, DOMException*>;

// This class is an implementation of WebCallbacks<PushSubscription*,
// DOMException*> that will resolve the underlying promise depending on the
// constructor and will pass it to the PushSubscription.
class PushSubscriptionCallbacks final
    : public WebCallbacks<PushSubscription*, DOMException*> {
  USING_FAST_MALLOC(PushSubscriptionCallbacks);

 public:
  PushSubscriptionCallbacks(ScriptPromiseResolverBase*, bool null_allowed);

  PushSubscriptionCallbacks(const PushSubscriptionCallbacks&) = delete;
  PushSubscriptionCallbacks& operator=(const PushSubscriptionCallbacks&) =
      delete;

  ~PushSubscriptionCallbacks() override;

  // WebCallbacks<S, T> interface.
  void OnSuccess(PushSubscription* push_subscription) override;
  void OnError(DOMException* error) override;

 private:
  Persistent<ScriptPromiseResolverBase> resolver_;
  bool null_allowed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_SUBSCRIPTION_CALLBACKS_H_

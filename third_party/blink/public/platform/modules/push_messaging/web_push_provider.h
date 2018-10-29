// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PUSH_MESSAGING_WEB_PUSH_PROVIDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PUSH_MESSAGING_WEB_PUSH_PROVIDER_H_

#include "third_party/blink/public/platform/modules/push_messaging/web_push_subscription.h"
#include "third_party/blink/public/platform/web_callbacks.h"

#include <memory>

namespace blink {

struct WebPushError;
struct WebPushSubscriptionOptions;

using WebPushSubscriptionCallbacks =
    WebCallbacks<std::unique_ptr<WebPushSubscription>, const WebPushError&>;
using WebPushUnsubscribeCallbacks = WebCallbacks<bool, const WebPushError&>;

class WebPushProvider {
 public:
  virtual ~WebPushProvider() = default;

  // Takes ownership of the WebPushSubscriptionCallbacks.
  virtual void Subscribe(int64_t service_worker_registration_id,
                         const WebPushSubscriptionOptions&,
                         bool user_gesture,
                         std::unique_ptr<WebPushSubscriptionCallbacks>) = 0;

  // Takes ownership of the WebPushSubscriptionCallbacks.
  virtual void GetSubscription(
      int64_t service_worker_registration_id,
      std::unique_ptr<WebPushSubscriptionCallbacks>) = 0;

  // Takes ownership if the WebPushUnsubscribeCallbacks.
  virtual void Unsubscribe(int64_t service_worker_registration_id,
                           std::unique_ptr<WebPushUnsubscribeCallbacks>) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PUSH_MESSAGING_WEB_PUSH_PROVIDER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PUSH_MESSAGING_WEB_PUSH_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PUSH_MESSAGING_WEB_PUSH_CLIENT_H_

#include <memory>
#include "third_party/blink/public/platform/modules/push_messaging/web_push_error.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_provider.h"
#include "third_party/blink/public/platform/web_callbacks.h"

namespace blink {

struct WebPushSubscriptionOptions;

class WebPushClient {
 public:
  virtual ~WebPushClient() = default;

  // Ownership of the callbacks is transferred to the client.
  virtual void Subscribe(int64_t service_worker_registration_id,
                         const WebPushSubscriptionOptions&,
                         bool user_gesture,
                         std::unique_ptr<WebPushSubscriptionCallbacks>) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PUSH_MESSAGING_WEB_PUSH_CLIENT_H_

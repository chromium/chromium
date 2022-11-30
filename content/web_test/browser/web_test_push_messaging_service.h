// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_PUSH_MESSAGING_SERVICE_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_PUSH_MESSAGING_SERVICE_H_

#include <stdint.h>

#include <map>
#include <set>

#include "content/public/browser/push_messaging_service.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"

namespace content {

class WebTestPushMessagingService : public PushMessagingService {
 public:
  WebTestPushMessagingService();

  WebTestPushMessagingService(const WebTestPushMessagingService&) = delete;
  WebTestPushMessagingService& operator=(const WebTestPushMessagingService&) =
      delete;

  ~WebTestPushMessagingService() override;

  // PushMessagingService implementation:
  void SubscribeFromDocument(const GURL& requesting_origin,
                             int64_t service_worker_registration_id,
                             int render_process_id,
                             int render_frame_id,
                             blink::mojom::PushSubscriptionOptionsPtr options,
                             bool user_gesture,
                             RegisterCallback callback) override;
  void SubscribeFromWorker(const GURL& requesting_origin,
                           int64_t service_worker_registration_id,
                           int render_process_id,
                           blink::mojom::PushSubscriptionOptionsPtr options,
                           RegisterCallback callback) override;
  void GetSubscriptionInfo(const GURL& origin,
                           int64_t service_worker_registration_id,
                           const std::string& sender_id,
                           const std::string& subscription_id,
                           SubscriptionInfoCallback callback) override;
  bool SupportNonVisibleMessages() override;
  void Unsubscribe(blink::mojom::PushUnregistrationReason reason,
                   const GURL& requesting_origin,
                   int64_t service_worker_registration_id,
                   const std::string& sender_id,
                   UnregisterCallback callback) override;
  void DidDeleteServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id) override;
  void DidDeleteServiceWorkerDatabase() override;

 private:
  GURL CreateEndpoint(const std::string& subscription_id) const;

  int64_t subscribed_service_worker_registration_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_PUSH_MESSAGING_SERVICE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_PUSH_PROVIDER_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_PUSH_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/push_messaging.mojom.h"
#include "content/public/renderer/worker_thread.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_error.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_provider.h"

class GURL;

namespace blink {
struct WebPushSubscriptionOptions;
}

namespace content {

namespace mojom {
enum class PushGetRegistrationStatus;
enum class PushRegistrationStatus;
}  // namespace mojom

struct PushSubscriptionOptions;

blink::WebPushError PushRegistrationStatusToWebPushError(
    mojom::PushRegistrationStatus status);

class PushProvider : public blink::WebPushProvider,
                     public WorkerThread::Observer {
 public:
  ~PushProvider() override;

  static PushProvider* ThreadSpecificInstance(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  // blink::WebPushProvider implementation.
  void Subscribe(
      int64_t service_worker_registration_id,
      const blink::WebPushSubscriptionOptions& options,
      bool user_gesture,
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) override;
  void Unsubscribe(
      int64_t service_worker_registration_id,
      std::unique_ptr<blink::WebPushUnsubscribeCallbacks> callbacks) override;
  void GetSubscription(
      int64_t service_worker_registration_id,
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) override;

 private:
  explicit PushProvider(const scoped_refptr<base::SingleThreadTaskRunner>&
                            main_thread_task_runner);

  static void GetInterface(mojom::PushMessagingRequest request);

  void DidSubscribe(
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
      mojom::PushRegistrationStatus status,
      const base::Optional<GURL>& endpoint,
      const base::Optional<PushSubscriptionOptions>& options,
      const base::Optional<std::vector<uint8_t>>& p256dh,
      const base::Optional<std::vector<uint8_t>>& auth);

  void DidUnsubscribe(
      std::unique_ptr<blink::WebPushUnsubscribeCallbacks> callbacks,
      blink::WebPushError::ErrorType error_type,
      bool did_unsubscribe,
      const base::Optional<std::string>& error_message);

  void DidGetSubscription(
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
      mojom::PushGetRegistrationStatus status,
      const base::Optional<GURL>& endpoint,
      const base::Optional<PushSubscriptionOptions>& options,
      const base::Optional<std::vector<uint8_t>>& p256dh,
      const base::Optional<std::vector<uint8_t>>& auth);

  mojom::PushMessagingPtr push_messaging_manager_;

  DISALLOW_COPY_AND_ASSIGN(PushProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_PUSH_PROVIDER_H_

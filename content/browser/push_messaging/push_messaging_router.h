// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_ROUTER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_ROUTER_H_

#include <stdint.h>

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-forward.h"
#include "url/gurl.h"

namespace blink {
namespace mojom {
enum class PushEventStatus;
}  // namespace mojom
}  // namespace blink

namespace content {

class BrowserContext;
class ServiceWorkerVersion;
class ServiceWorkerContextWrapper;

// All methods must be called on the UI thread.
class PushMessagingRouter {
 public:
  using PushEventCallback =
      base::OnceCallback<void(blink::mojom::PushEventStatus)>;

  PushMessagingRouter() = delete;
  PushMessagingRouter(const PushMessagingRouter&) = delete;
  PushMessagingRouter& operator=(const PushMessagingRouter&) = delete;

  // Delivers a push message with |payload| to the Service Worker identified by
  // |origin| and |service_worker_registration_id|.
  static void DeliverMessage(BrowserContext* browser_context,
                             const GURL& origin,
                             int64_t service_worker_registration_id,
                             const std::string& message_id,
                             std::optional<std::string> payload,
                             PushEventCallback deliver_message_callback);

  // TODO(crbug.com/41338108): Add the ability to trigger a push
  // subscription change event in DevTools
  // Fires a pushsubscriptionchangeevent with the arguments |new_subscription|
  // and |old_subscription| to service workers.
  static void FireSubscriptionChangeEvent(
      BrowserContext* browser_context,
      const GURL& origin,
      int64_t service_worker_registration_id,
      blink::mojom::PushSubscriptionPtr new_subscription,
      blink::mojom::PushSubscriptionPtr old_subscription,
      PushEventCallback subscription_change_callback);

 private:
  // Delivers a push message with |payload| to a specific |service_worker|.
  static void DeliverMessageToWorker(
      const std::string& message_id,
      std::optional<std::string> payload,
      PushEventCallback deliver_message_callback,
      scoped_refptr<ServiceWorkerVersion> service_worker,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      blink::ServiceWorkerStatusCode status);

  // Gets called asynchronously after the Service Worker has dispatched the push
  // event.
  static void DeliverMessageEnd(
      scoped_refptr<ServiceWorkerVersion> service_worker,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      const std::string& message_id,
      PushEventCallback deliver_message_callback,
      blink::ServiceWorkerStatusCode service_worker_status);

  // Fires a `pushsubscriptionchange` event to the |service_worker| if it is
  // ready.
  static void FireSubscriptionChangeEventToWorker(
      blink::mojom::PushSubscriptionPtr new_subscription,
      blink::mojom::PushSubscriptionPtr old_subscription,
      PushEventCallback subscription_change_callback,
      scoped_refptr<ServiceWorkerVersion> service_worker,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      blink::ServiceWorkerStatusCode status);

  // Gets called asynchronously after the Service Worker has dispatched the
  // `pushsubscriptionchange` event.
  static void FireSubscriptionChangeEventEnd(
      scoped_refptr<ServiceWorkerVersion> service_worker,
      PushEventCallback subscription_change_callback,
      blink::ServiceWorkerStatusCode service_worker_status);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_ROUTER_H_

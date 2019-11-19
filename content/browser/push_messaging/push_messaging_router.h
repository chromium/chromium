// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_ROUTER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_ROUTER_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace blink {
namespace mojom {
enum class PushDeliveryStatus;
}  // namespace mojom
}  // namespace blink

namespace content {

class BrowserContext;
class DevToolsBackgroundServicesContextImpl;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

class PushMessagingRouter {
 public:
  using DeliverMessageCallback =
      base::Callback<void(blink::mojom::PushDeliveryStatus)>;

  // Delivers a push message with |data| to the Service Worker identified by
  // |origin| and |service_worker_registration_id|. Must be called on the UI
  // thread.
  static void DeliverMessage(
      BrowserContext* browser_context,
      const GURL& origin,
      int64_t service_worker_registration_id,
      const std::string& message_id,
      base::Optional<std::string> payload,
      const DeliverMessageCallback& deliver_message_callback);

 private:
  // Attempts to find a Service Worker registration so that a push event can be
  // dispatched. Must be called on the IO thread.
  static void FindServiceWorkerRegistration(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context,
      const GURL& origin,
      int64_t service_worker_registration_id,
      const std::string& message_id,
      base::Optional<std::string> payload,
      const DeliverMessageCallback& deliver_message_callback);

  // If a registration was successfully retrieved, dispatches a push event with
  // |data| on the Service Worker identified by |service_worker_registration|.
  // Must be called on the IO thread.
  static void FindServiceWorkerRegistrationCallback(
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context,
      const std::string& message_id,
      base::Optional<std::string> payload,
      const DeliverMessageCallback& deliver_message_callback,
      blink::ServiceWorkerStatusCode service_worker_status,
      scoped_refptr<ServiceWorkerRegistration> service_worker_registration);

  // Delivers a push message with |data| to a specific |service_worker|.
  // Must be called on the IO thread.
  static void DeliverMessageToWorker(
      scoped_refptr<ServiceWorkerVersion> service_worker,
      scoped_refptr<ServiceWorkerRegistration> service_worker_registration,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context,
      const std::string& message_id,
      base::Optional<std::string> payload,
      const DeliverMessageCallback& deliver_message_callback,
      blink::ServiceWorkerStatusCode start_worker_status);

  // Gets called asynchronously after the Service Worker has dispatched the push
  // event. Must be called on the IO thread.
  static void DeliverMessageEnd(
      scoped_refptr<ServiceWorkerVersion> service_worker,
      scoped_refptr<ServiceWorkerRegistration> service_worker_registration,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context,
      const std::string& message_id,
      const DeliverMessageCallback& deliver_message_callback,
      blink::ServiceWorkerStatusCode service_worker_status);

  DISALLOW_IMPLICIT_CONSTRUCTORS(PushMessagingRouter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_ROUTER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_KEEPALIVE_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_KEEPALIVE_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_external_request_timeout_type.h"
#include "extensions/browser/activity.h"
#include "extensions/browser/service_worker/worker_id.h"

namespace extensions {

class ServiceWorkerKeepalive {
 public:
  ServiceWorkerKeepalive(
      content::BrowserContext* browser_context,
      WorkerId worker_id,
      content::ServiceWorkerExternalRequestTimeoutType timeout_type,
      Activity::Type activity_type,
      std::string activity_extra_data);
  ServiceWorkerKeepalive(const ServiceWorkerKeepalive&) = delete;
  ServiceWorkerKeepalive& operator=(const ServiceWorkerKeepalive&) = delete;
  ServiceWorkerKeepalive(ServiceWorkerKeepalive&&);
  ~ServiceWorkerKeepalive();

  static void EnsureShutdownNotifierFactoryBuilt();

  const WorkerId& worker_id() const { return worker_id_; }
  Activity::Type activity_type() const { return activity_type_; }
  const std::string& activity_extra_data() const {
    return activity_extra_data_;
  }

 private:
  void Shutdown();

  // The context associated with the extension for this keepalive.
  raw_ptr<content::BrowserContext> browser_context_;
  // The worker ID associated with the service worker for this keepalive.
  WorkerId worker_id_;
  // The type of activity that is keeping the worker alive.
  Activity::Type activity_type_;
  // Extra data associated with the activity, such as a function or event name.
  std::string activity_extra_data_;
  // A unique ID associated with the request.
  base::Uuid request_uuid_;

  // Subscription to trigger `Shutdown()` when the corresponding BrowserContext
  // is destroyed.
  base::CallbackListSubscription shutdown_subscription_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_KEEPALIVE_H_

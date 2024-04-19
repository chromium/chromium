// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_keepalive.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/uuid.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/service_worker/worker_id.h"

namespace extensions {

namespace {

class ServiceWorkerKeepaliveShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ServiceWorkerKeepaliveShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<ServiceWorkerKeepaliveShutdownNotifierFactory>
        s_factory;
    return s_factory.get();
  }

  ServiceWorkerKeepaliveShutdownNotifierFactory(
      const ServiceWorkerKeepaliveShutdownNotifierFactory&) = delete;
  ServiceWorkerKeepaliveShutdownNotifierFactory& operator=(
      const ServiceWorkerKeepaliveShutdownNotifierFactory&) = delete;

 private:
  friend class base::NoDestructor<
      ServiceWorkerKeepaliveShutdownNotifierFactory>;
  ServiceWorkerKeepaliveShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "ServiceWorkerKeepalive") {
    DependsOn(ProcessManagerFactory::GetInstance());
  }

  // Use whichever profile is associated with the Keepalive.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
        context, /*force_guest_profile=*/true);
  }
};

}  // namespace

// static
void ServiceWorkerKeepalive::EnsureShutdownNotifierFactoryBuilt() {
  ServiceWorkerKeepaliveShutdownNotifierFactory::GetInstance();
}

ServiceWorkerKeepalive::ServiceWorkerKeepalive(
    content::BrowserContext* browser_context,
    WorkerId worker_id,
    content::ServiceWorkerExternalRequestTimeoutType timeout_type,
    Activity::Type activity_type,
    std::string activity_extra_data)
    : browser_context_(browser_context),
      worker_id_(std::move(worker_id)),
      activity_type_(activity_type),
      activity_extra_data_(std::move(activity_extra_data)) {
  CHECK(browser_context_);

  ProcessManager* process_manager = ProcessManager::Get(browser_context_);
  CHECK(process_manager);
  // TODO(crbug.com/40942252): Investigate the circumstances in which
  // this CHECK can fail. It's possible the service worker called an API before
  // it fully finished initializing.
  // This isn't ideal, but the keepalive mechanism in ProcessManager doesn't
  // rely on a service worker registered in the process manager (all the data
  // is in the worker ID), so the below is not inherently unsafe (but needs to
  // be fixed).
  // CHECK(process_manager->HasServiceWorker(worker_id_));

  request_uuid_ = process_manager->IncrementServiceWorkerKeepaliveCount(
      worker_id_, timeout_type, activity_type_, activity_extra_data_);

  // base::Unretained is safe because this is a callback owned by this class.
  shutdown_subscription_ =
      ServiceWorkerKeepaliveShutdownNotifierFactory::GetInstance()
          ->Get(browser_context_)
          ->Subscribe(base::BindRepeating(&ServiceWorkerKeepalive::Shutdown,
                                          base::Unretained(this)));
}

ServiceWorkerKeepalive::ServiceWorkerKeepalive(ServiceWorkerKeepalive&&) =
    default;

ServiceWorkerKeepalive::~ServiceWorkerKeepalive() {
  if (!browser_context_) {
    // Shutdown has started. Bail.
    return;
  }

  ProcessManager* process_manager = ProcessManager::Get(browser_context_);
  // `process_manager` should always be non-null because of the shutdown check
  // above.
  CHECK(process_manager);

  if (!process_manager->HasServiceWorker(worker_id_)) {
    // The service worker may have legitimately already stopped (despite this
    // keepalive). Keepalives have different styles (whether they extend past
    // the timeout limit), and the worker may have timed out on another request.
    return;
  }

  process_manager->DecrementServiceWorkerKeepaliveCount(
      worker_id_, request_uuid_, activity_type_, activity_extra_data_);
}

void ServiceWorkerKeepalive::Shutdown() {
  browser_context_ = nullptr;
}

}  // namespace extensions

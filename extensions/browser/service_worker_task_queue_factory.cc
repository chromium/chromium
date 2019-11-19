// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_task_queue_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/service_worker_task_queue.h"

using content::BrowserContext;
namespace extensions {

// static
ServiceWorkerTaskQueue* ServiceWorkerTaskQueueFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ServiceWorkerTaskQueue*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ServiceWorkerTaskQueueFactory* ServiceWorkerTaskQueueFactory::GetInstance() {
  return base::Singleton<ServiceWorkerTaskQueueFactory>::get();
}

ServiceWorkerTaskQueueFactory::ServiceWorkerTaskQueueFactory()
    : BrowserContextKeyedServiceFactory(
          "ServiceWorkerTaskQueue",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

ServiceWorkerTaskQueueFactory::~ServiceWorkerTaskQueueFactory() {}

KeyedService* ServiceWorkerTaskQueueFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new ServiceWorkerTaskQueue(context);
}

BrowserContext* ServiceWorkerTaskQueueFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  return context;
}

}  // namespace extensions

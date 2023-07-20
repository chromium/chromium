// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_task_queue_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager_factory.h"
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
  DependsOn(ProcessManagerFactory::GetInstance());
}

ServiceWorkerTaskQueueFactory::~ServiceWorkerTaskQueueFactory() = default;

KeyedService* ServiceWorkerTaskQueueFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  ServiceWorkerTaskQueue* task_queue = new ServiceWorkerTaskQueue(context);
  BrowserContext* original_context =
      ExtensionsBrowserClient::Get()->GetOriginalContext(context);
  if (original_context != context) {
    // To let incognito context's ServiceWorkerTaskQueue know about extensions
    // that were activated (which has its own instance of
    // ServiceWorkerTaskQueue), we'd need to activate extensions from
    // |original_context|'s ServiceWorkerTaskQueue.
    task_queue->ActivateIncognitoSplitModeExtensions(
        ServiceWorkerTaskQueue::Get(original_context));
  }
  return task_queue;
}

BrowserContext* ServiceWorkerTaskQueueFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  return context;
}

}  // namespace extensions

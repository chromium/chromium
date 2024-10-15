// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_task_queue_factory.h"

#include <memory>

#include "base/notreached.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/common/extension_features.h"

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

std::unique_ptr<KeyedService>
ServiceWorkerTaskQueueFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  std::unique_ptr<ServiceWorkerTaskQueue> task_queue;
  if (base::FeatureList::IsEnabled(
          extensions_features::kUseNewServiceWorkerTaskQueue)) {
    // TODO(crbug.com/40276609): Insert new task queue once
    // ServiceWorkerTaskQueue is an abstract class.
    NOTREACHED();
  } else {
    task_queue = std::make_unique<ServiceWorkerTaskQueue>(context);
  }
  BrowserContext* original_context =
      ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
          context, /*force_guest_profile=*/true);
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
  return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
      context, /*force_guest_profile=*/true);
}

}  // namespace extensions

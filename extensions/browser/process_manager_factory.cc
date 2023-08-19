// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue_factory.h"
#include "extensions/browser/process_manager.h"

using content::BrowserContext;

namespace extensions {

// static
ProcessManager* ProcessManagerFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ProcessManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ProcessManager* ProcessManagerFactory::GetForBrowserContextIfExists(
    BrowserContext* context) {
  return static_cast<ProcessManager*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
ProcessManagerFactory* ProcessManagerFactory::GetInstance() {
  return base::Singleton<ProcessManagerFactory>::get();
}

ProcessManagerFactory::ProcessManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ProcessManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::LazyBackgroundTaskQueueFactory::GetInstance());
}

ProcessManagerFactory::~ProcessManagerFactory() {
}

KeyedService* ProcessManagerFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return ProcessManager::Create(context);
}

BrowserContext* ProcessManagerFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // ProcessManager::Create handles guest and incognito profiles, returning an
  // IncognitoProcessManager in incognito mode.
  return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
      context, /*force_guest_profile=*/true);
}

}  // namespace extensions

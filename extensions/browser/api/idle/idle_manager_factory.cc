// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/idle/idle_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/api/idle/idle_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
IdleManager* IdleManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<IdleManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
IdleManagerFactory* IdleManagerFactory::GetInstance() {
  return base::Singleton<IdleManagerFactory>::get();
}

IdleManagerFactory::IdleManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "IdleManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

IdleManagerFactory::~IdleManagerFactory() {
}

KeyedService* IdleManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  IdleManager* idle_manager = new IdleManager(context);
  idle_manager->Init();
  return idle_manager;
}

content::BrowserContext* IdleManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetRedirectedContextInIncognito(
      context, /*force_guest_profile=*/true, /*force_system_profile=*/false);
}

bool IdleManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool IdleManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions

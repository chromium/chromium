// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_map.h"

using content::BrowserContext;

namespace extensions {

// static
ProcessMap* ProcessMapFactory::GetForBrowserContext(BrowserContext* context) {
  return static_cast<ProcessMap*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ProcessMapFactory* ProcessMapFactory::GetInstance() {
  return base::Singleton<ProcessMapFactory>::get();
}

ProcessMapFactory::ProcessMapFactory()
    : BrowserContextKeyedServiceFactory(
          "ProcessMap",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

ProcessMapFactory::~ProcessMapFactory() = default;

std::unique_ptr<KeyedService>
ProcessMapFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  std::unique_ptr<ProcessMap> process_map =
      std::make_unique<ProcessMap>(context);
  process_map->set_is_lock_screen_context(
      ExtensionsBrowserClient::Get()->IsLockScreenContext(context));
  return process_map;
}

BrowserContext* ProcessMapFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

}  // namespace extensions

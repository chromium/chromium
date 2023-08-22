// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registry_factory.h"

#include "base/check.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"

using content::BrowserContext;

namespace extensions {

// static
ExtensionRegistry* ExtensionRegistryFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ExtensionRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionRegistryFactory* ExtensionRegistryFactory::GetInstance() {
  return base::Singleton<ExtensionRegistryFactory>::get();
}

ExtensionRegistryFactory::ExtensionRegistryFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionRegistry",
          BrowserContextDependencyManager::GetInstance()) {
  // No dependencies on other services.
}

ExtensionRegistryFactory::~ExtensionRegistryFactory() = default;

std::unique_ptr<KeyedService>
ExtensionRegistryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionRegistry>(context);
}

BrowserContext* ExtensionRegistryFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  auto* extension_browser_client = ExtensionsBrowserClient::Get();
  DCHECK(extension_browser_client);
  return extension_browser_client->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

}  // namespace extensions

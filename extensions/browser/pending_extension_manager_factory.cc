// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pending_extension_manager_factory.h"

#include "base/check.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pending_extension_manager.h"

using content::BrowserContext;

namespace extensions {

// static
PendingExtensionManager* PendingExtensionManagerFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<PendingExtensionManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PendingExtensionManagerFactory* PendingExtensionManagerFactory::GetInstance() {
  return base::Singleton<PendingExtensionManagerFactory>::get();
}

PendingExtensionManagerFactory::PendingExtensionManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PendingExtensionManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
}

PendingExtensionManagerFactory::~PendingExtensionManagerFactory() = default;

std::unique_ptr<KeyedService>
PendingExtensionManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<PendingExtensionManager>(context);
}

BrowserContext* PendingExtensionManagerFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  auto* extension_browser_client = ExtensionsBrowserClient::Get();
  DCHECK(extension_browser_client);
  return extension_browser_client->GetContextRedirectedToOriginal(context);
}

}  // namespace extensions

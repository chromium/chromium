// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registrar_factory.h"

#include "base/check.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager_factory.h"

using content::BrowserContext;

namespace extensions {

// static
ExtensionRegistrar* ExtensionRegistrarFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ExtensionRegistrar*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionRegistrarFactory* ExtensionRegistrarFactory::GetInstance() {
  return base::Singleton<ExtensionRegistrarFactory>::get();
}

ExtensionRegistrarFactory::ExtensionRegistrarFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionRegistrar",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ProcessManagerFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
}

ExtensionRegistrarFactory::~ExtensionRegistrarFactory() = default;

std::unique_ptr<KeyedService>
ExtensionRegistrarFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionRegistrar>(context);
}

BrowserContext* ExtensionRegistrarFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  auto* extension_browser_client = ExtensionsBrowserClient::Get();
  DCHECK(extension_browser_client);
  return extension_browser_client->GetContextRedirectedToOriginal(context);
}

}  // namespace extensions

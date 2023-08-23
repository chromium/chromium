// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_prefs_helper_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_helper.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
ExtensionPrefsHelper* ExtensionPrefsHelperFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionPrefsHelper*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionPrefsHelperFactory* ExtensionPrefsHelperFactory::GetInstance() {
  return base::Singleton<ExtensionPrefsHelperFactory>::get();
}

ExtensionPrefsHelperFactory::ExtensionPrefsHelperFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionPrefsHelper",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionPrefValueMapFactory::GetInstance());
}

ExtensionPrefsHelperFactory::~ExtensionPrefsHelperFactory() = default;

std::unique_ptr<KeyedService>
ExtensionPrefsHelperFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionPrefsHelper>(
      ExtensionPrefsFactory::GetForBrowserContext(context),
      ExtensionPrefValueMapFactory::GetForBrowserContext(context));
}

content::BrowserContext* ExtensionPrefsHelperFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

bool ExtensionPrefsHelperFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions

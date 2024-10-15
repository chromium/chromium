// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_pref_value_map_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extensions_browser_client.h"

ExtensionPrefValueMapFactory::ExtensionPrefValueMapFactory()
    : BrowserContextKeyedServiceFactory(
        "ExtensionPrefValueMap",
        BrowserContextDependencyManager::GetInstance()) {
}

ExtensionPrefValueMapFactory::~ExtensionPrefValueMapFactory() {
}

// static
ExtensionPrefValueMap* ExtensionPrefValueMapFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionPrefValueMap*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionPrefValueMapFactory* ExtensionPrefValueMapFactory::GetInstance() {
  return base::Singleton<ExtensionPrefValueMapFactory>::get();
}

std::unique_ptr<KeyedService>
ExtensionPrefValueMapFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionPrefValueMap>();
}

content::BrowserContext* ExtensionPrefValueMapFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return extensions::ExtensionsBrowserClient::Get()
      ->GetContextRedirectedToOriginal(context, /*force_guest_profile=*/true);
}

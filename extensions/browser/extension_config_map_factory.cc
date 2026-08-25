// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_config_map_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_config_map.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

ExtensionConfigMapFactory::ExtensionConfigMapFactory(
    base::PassKey<ExtensionConfigMapFactory> pass_key)
    : BrowserContextKeyedServiceFactory(
          "ExtensionConfigMap",
          BrowserContextDependencyManager::GetInstance()) {}

ExtensionConfigMapFactory::~ExtensionConfigMapFactory() = default;

// static
ExtensionConfigMap* ExtensionConfigMapFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionConfigMap*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/false));
}

// static
ExtensionConfigMap* ExtensionConfigMapFactory::GetOrCreateForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionConfigMap*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ExtensionConfigMapFactory* ExtensionConfigMapFactory::GetInstance() {
  static base::NoDestructor<ExtensionConfigMapFactory> instance(
      base::PassKey<ExtensionConfigMapFactory>{});
  return instance.get();
}

content::BrowserContext* ExtensionConfigMapFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetContextOwnInstance(context);
}

std::unique_ptr<KeyedService>
ExtensionConfigMapFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionConfigMap>();
}

}  // namespace extensions

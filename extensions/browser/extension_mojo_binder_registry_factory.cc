// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_mojo_binder_registry_factory.h"

#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_mojo_binder_registry.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
ExtensionMojoBinderRegistry*
ExtensionMojoBinderRegistryFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionMojoBinderRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ExtensionMojoBinderRegistryFactory*
ExtensionMojoBinderRegistryFactory::GetInstance() {
  static base::NoDestructor<ExtensionMojoBinderRegistryFactory> instance(
      base::PassKey<ExtensionMojoBinderRegistryFactory>{});
  return instance.get();
}

ExtensionMojoBinderRegistryFactory::ExtensionMojoBinderRegistryFactory(
    base::PassKey<ExtensionMojoBinderRegistryFactory>)
    : BrowserContextKeyedServiceFactory(
          "ExtensionMojoBinderRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

ExtensionMojoBinderRegistryFactory::~ExtensionMojoBinderRegistryFactory() =
    default;

content::BrowserContext*
ExtensionMojoBinderRegistryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetContextOwnInstance(context);
}

std::unique_ptr<KeyedService>
ExtensionMojoBinderRegistryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionMojoBinderRegistry>();
}

}  // namespace extensions

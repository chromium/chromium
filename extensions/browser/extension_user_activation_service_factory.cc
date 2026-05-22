// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_activation_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_user_activation_service.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
ExtensionUserActivationService*
ExtensionUserActivationServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionUserActivationService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionUserActivationServiceFactory*
ExtensionUserActivationServiceFactory::GetInstance() {
  static base::NoDestructor<ExtensionUserActivationServiceFactory> instance;
  return instance.get();
}

ExtensionUserActivationServiceFactory::ExtensionUserActivationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionUserActivationService",
          BrowserContextDependencyManager::GetInstance()) {}

ExtensionUserActivationServiceFactory::
    ~ExtensionUserActivationServiceFactory() = default;

std::unique_ptr<KeyedService>
ExtensionUserActivationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionUserActivationService>();
}

content::BrowserContext*
ExtensionUserActivationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetContextOwnInstance(context);
}

}  // namespace extensions

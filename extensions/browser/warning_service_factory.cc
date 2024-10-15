// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/warning_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/warning_service.h"

using content::BrowserContext;

namespace extensions {

// static
WarningService* WarningServiceFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<WarningService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
WarningServiceFactory* WarningServiceFactory::GetInstance() {
  return base::Singleton<WarningServiceFactory>::get();
}

WarningServiceFactory::WarningServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WarningService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

WarningServiceFactory::~WarningServiceFactory() {
}

std::unique_ptr<KeyedService>
WarningServiceFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<WarningService>(context);
}

BrowserContext* WarningServiceFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

}  // namespace extensions

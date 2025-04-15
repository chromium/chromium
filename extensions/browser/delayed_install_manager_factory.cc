// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/delayed_install_manager_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/delayed_install_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/extensions_browser_client.h"

using content::BrowserContext;

namespace extensions {

// static
DelayedInstallManager* DelayedInstallManagerFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<DelayedInstallManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
DelayedInstallManagerFactory* DelayedInstallManagerFactory::GetInstance() {
  static base::NoDestructor<DelayedInstallManagerFactory> instance;
  return instance.get();
}

DelayedInstallManagerFactory::DelayedInstallManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "DelayedInstallManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistrarFactory::GetInstance());
}

DelayedInstallManagerFactory::~DelayedInstallManagerFactory() = default;

std::unique_ptr<KeyedService>
DelayedInstallManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DelayedInstallManager>(context);
}

content::BrowserContext* DelayedInstallManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      browser_context);
}

}  // namespace extensions

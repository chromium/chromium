// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extension_system_factory.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/shell/browser/shell_extension_system.h"

using content::BrowserContext;

namespace extensions {

ExtensionSystem* ShellExtensionSystemFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ShellExtensionSystem*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ShellExtensionSystemFactory* ShellExtensionSystemFactory::GetInstance() {
  return base::Singleton<ShellExtensionSystemFactory>::get();
}

ShellExtensionSystemFactory::ShellExtensionSystemFactory()
    : ExtensionSystemProvider("ShellExtensionSystem",
                              BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(apps::AppLifetimeMonitorFactory::GetInstance());
}

ShellExtensionSystemFactory::~ShellExtensionSystemFactory() {
}

std::unique_ptr<KeyedService>
ShellExtensionSystemFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<ShellExtensionSystem>(context);
}

BrowserContext* ShellExtensionSystemFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Use a separate instance for incognito.
  return context;
}

bool ShellExtensionSystemFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions

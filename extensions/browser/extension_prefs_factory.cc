// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_prefs_factory.h"

#include <utility>

#include "base/command_line.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"

namespace extensions {

// static
ExtensionPrefs* ExtensionPrefsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionPrefs*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionPrefsFactory* ExtensionPrefsFactory::GetInstance() {
  return base::Singleton<ExtensionPrefsFactory>::get();
}

void ExtensionPrefsFactory::SetInstanceForTesting(
    content::BrowserContext* context,
    std::unique_ptr<ExtensionPrefs> prefs) {
  SetTestingFactory(
      context,
      base::BindOnce([](std::unique_ptr<ExtensionPrefs> prefs,
                        content::BrowserContext* context)
                         -> std::unique_ptr<KeyedService> { return prefs; },
                     std::move(prefs)));
}

ExtensionPrefsFactory::ExtensionPrefsFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionPrefs",
          BrowserContextDependencyManager::GetInstance()) {
}

ExtensionPrefsFactory::~ExtensionPrefsFactory() {
}

std::unique_ptr<KeyedService>
ExtensionPrefsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  std::vector<EarlyExtensionPrefsObserver*> prefs_observers;
  client->GetEarlyExtensionPrefsObservers(context, &prefs_observers);
  return ExtensionPrefs::Create(
      context, client->GetPrefServiceForContext(context),
      context->GetPath().AppendASCII(extensions::kInstallDirectoryName),
      ExtensionPrefValueMapFactory::GetForBrowserContext(context),
      client->AreExtensionsDisabled(*base::CommandLine::ForCurrentProcess(),
                                    context),
      prefs_observers);
}

content::BrowserContext* ExtensionPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

}  // namespace extensions

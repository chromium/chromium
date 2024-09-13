// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/permission_helper.h"

#include "base/no_destructor.h"
#include "build/android_buildflags.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/process_map_factory.h"

namespace extensions {

PermissionHelper::PermissionHelper(content::BrowserContext* context)
    : browser_context_(context),
      extension_registry_(ExtensionRegistry::Get(context)) {
  // Ensure the dependency is constructed.
  ProcessMap::Get(browser_context_);
}

PermissionHelper::~PermissionHelper() = default;

// static
PermissionHelper* PermissionHelper::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<PermissionHelper>::Get(context);
}

// static
BrowserContextKeyedAPIFactory<PermissionHelper>*
PermissionHelper::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<PermissionHelper>>
      instance;
  return instance.get();
}

bool PermissionHelper::ShouldHideBrowserNetworkRequest(
    const WebRequestInfo& request) const {
  return ExtensionsAPIClient::Get()->ShouldHideBrowserNetworkRequest(
      browser_context_, request);
}

bool PermissionHelper::CanCrossIncognito(const Extension* extension) const {
  return extensions::util::CanCrossIncognito(extension, browser_context_);
}

template <>
void BrowserContextKeyedAPIFactory<
    PermissionHelper>::DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ProcessMapFactory::GetInstance());
  // Used in CanCrossIncognito().
  DependsOn(ExtensionPrefsFactory::GetInstance());

  // `ShouldHideBrowserNetworkRequest()` relies on the ExtensionsAPIClient,
  // which itself uses varioius KeyedServices. Thus, this KeyedService
  // implicitly depends upon those.
  ExtensionsAPIClient* extensions_api_client = ExtensionsAPIClient::Get();

#if BUILDFLAG(IS_DESKTOP_ANDROID)
  // TODO(https://crbug.com/356905053): On Android, the startup and
  // initialization flow is different.
  // ExtensionsAPIClient is instantiated as part of the ExtensionsBrowserClient,
  // which in turn is created as part of BrowserProcessImpl::Init(). On
  // most desktop platforms, this happens before any profile initialization,
  // which means KeyedServices and factories can rely on the
  // ExtensionsBrowserClient and related classes existing.
  // On Android, however, because of StartupData (from
  // //chrome/browser/startup_data), the profile initialization happens *before*
  // the BrowserProcess is initialized, and as part of profile initialization,
  // we instantiate KeyedService factories. This, in turn, means that the
  // ExtensionsBrowserClient (and other global state we expect to "always exist"
  // is not ready at this point.
  // This doesn't matter at this point yet, since the desktop-android
  // implementation of the ExtensionsAPIClient has no factory dependencies. But
  // in general, this is no good, and we'll definitely need to fix it.
  if (!extensions_api_client) {
    return;
  }
#endif

  CHECK(extensions_api_client);

  for (auto* factory : ExtensionsAPIClient::Get()->GetFactoryDependencies()) {
    DependsOn(factory);
  }
}

}  // namespace extensions

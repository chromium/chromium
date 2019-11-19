// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/permission_helper.h"

#include "base/no_destructor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_map_factory.h"

namespace extensions {

PermissionHelper::PermissionHelper(content::BrowserContext* context)
    : browser_context_(context),
      process_map_(ProcessMap::Get(context)),
      extension_registry_(ExtensionRegistry::Get(context)) {}

PermissionHelper::~PermissionHelper() {}

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
  // For ShouldHideBrowserNetworkRequest().
  for (auto* factory : ExtensionsAPIClient::Get()->GetFactoryDependencies())
    DependsOn(factory);
}

}  // namespace extensions

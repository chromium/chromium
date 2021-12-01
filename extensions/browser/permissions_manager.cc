// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"

#include <memory>

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"

namespace extensions {

namespace {

class PermissionsManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  PermissionsManagerFactory();
  ~PermissionsManagerFactory() override = default;
  PermissionsManagerFactory(const PermissionsManagerFactory&) = delete;
  const PermissionsManagerFactory& operator=(const PermissionsManagerFactory&) =
      delete;

  PermissionsManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  // BrowserContextKeyedServiceFactory
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* browser_context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
};

PermissionsManagerFactory::PermissionsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PermissionsManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

PermissionsManager* PermissionsManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<PermissionsManager*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

content::BrowserContext* PermissionsManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context);
}

KeyedService* PermissionsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new PermissionsManager();
}

}  // namespace

// Implementation of UserPermissionsSettings.
PermissionsManager::UserPermissionsSettings::UserPermissionsSettings() =
    default;

PermissionsManager::UserPermissionsSettings::~UserPermissionsSettings() =
    default;

// Implementation of PermissionsManager.
PermissionsManager::PermissionsManager() = default;

PermissionsManager::~PermissionsManager() = default;

// static
PermissionsManager* PermissionsManager::Get(
    content::BrowserContext* browser_context) {
  return static_cast<PermissionsManagerFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* PermissionsManager::GetFactory() {
  static base::NoDestructor<PermissionsManagerFactory> g_factory;
  return g_factory.get();
}

void PermissionsManager::AddUserRestrictedSite(const url::Origin& origin) {
  // Origin cannot be both restricted and permitted.
  RemoveUserPermittedSite(origin);

  user_permissions_.restricted_sites.insert(origin);
}

void PermissionsManager::RemoveUserRestrictedSite(const url::Origin& origin) {
  user_permissions_.restricted_sites.erase(origin);
}

void PermissionsManager::AddUserPermittedSite(const url::Origin& origin) {
  // Origin cannot be both restricted and permitted.
  RemoveUserRestrictedSite(origin);

  user_permissions_.permitted_sites.insert(origin);
}

void PermissionsManager::RemoveUserPermittedSite(const url::Origin& origin) {
  user_permissions_.permitted_sites.erase(origin);
}

const PermissionsManager::UserPermissionsSettings&
PermissionsManager::GetUserPermissionsSettings() {
  return user_permissions_;
}

}  // namespace extensions

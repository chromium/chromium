// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_host_registry.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

namespace {

class ExtensionHostRegistryFactory : public BrowserContextKeyedServiceFactory {
 public:
  ExtensionHostRegistryFactory();
  ExtensionHostRegistryFactory(const ExtensionHostRegistryFactory&) = delete;
  ExtensionHostRegistryFactory& operator=(const ExtensionHostRegistryFactory&) =
      delete;
  ~ExtensionHostRegistryFactory() override = default;

  ExtensionHostRegistry* GetForBrowserContext(content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

ExtensionHostRegistryFactory::ExtensionHostRegistryFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionHostRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

ExtensionHostRegistry* ExtensionHostRegistryFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ExtensionHostRegistry*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

content::BrowserContext* ExtensionHostRegistryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // This seems like a service that should have its own instance in incognito
  // in order to better ensure there isn't any bleed-over from off-the-record
  // contexts. Unfortunately, other systems (I'm looking at you,
  // LazyBackgroundTaskQueue!) rely on this, and are set up to be redirect to
  // the original context. This makes it quite challenging to let this have its
  // own incognito context.
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

KeyedService* ExtensionHostRegistryFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExtensionHostRegistry();
}

}  // namespace

ExtensionHostRegistry::ExtensionHostRegistry() = default;
ExtensionHostRegistry::~ExtensionHostRegistry() = default;

// static
ExtensionHostRegistry* ExtensionHostRegistry::Get(
    content::BrowserContext* browser_context) {
  return static_cast<ExtensionHostRegistryFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* ExtensionHostRegistry::GetFactory() {
  static base::NoDestructor<ExtensionHostRegistryFactory> g_factory;
  return g_factory.get();
}

void ExtensionHostRegistry::ExtensionHostCreated(
    ExtensionHost* extension_host) {
  DCHECK(!base::Contains(extension_hosts_, extension_host));
  extension_hosts_.insert(extension_host);

  for (Observer& observer : observers_) {
    observer.OnExtensionHostCreated(extension_host->browser_context(),
                                    extension_host);
  }
}

void ExtensionHostRegistry::ExtensionHostDestroyed(
    ExtensionHost* extension_host) {
  // NOTE: We don't do
  //
  // DCHECK(base::Contains(extension_hosts, extension_host);
  //
  // because the ExtensionHostCreated() signal is only fired when the
  // renderer process has been initialized, and an ExtensionHost could
  // be destroyed before that happens.
  extension_hosts_.erase(extension_host);

  for (Observer& observer : observers_) {
    observer.OnExtensionHostDestroyed(extension_host->browser_context(),
                                      extension_host);
  }
}

void ExtensionHostRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionHostRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace extensions

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_REGISTRY_H_

#include <unordered_set>

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionHost;

// A class responsible for tracking ExtensionHosts and notifying observers of
// relevant changes.
// See also ProcessManager, which is responsible for more of the construction
// lifetime management of these hosts.
class ExtensionHostRegistry : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a new ExtensionHost is created and the associated
    // RenderProcessHost is ready. The `browser_context` is the context
    // associated with that host (which might be an incognito version of
    // ExtensionHostRegistry::browser_context_).
    virtual void OnExtensionHostCreated(
        content::BrowserContext* browser_context,
        ExtensionHost* host) {}

    // Called when an ExtensionHost is destroyed. The `browser_context` is
    // the context associated with that host (which might be an incognito
    // version of ExtensionHostRegistry::browser_context_).
    virtual void OnExtensionHostDestroyed(
        content::BrowserContext* browser_context,
        ExtensionHost* host) {}
  };

  ExtensionHostRegistry();
  ExtensionHostRegistry(const ExtensionHostRegistry&) = delete;
  ExtensionHostRegistry& operator=(const ExtensionHostRegistry&) = delete;
  ~ExtensionHostRegistry() override;

  // Retrieves the ExtensionHostRegistry for a given `browser_context`.
  // NOTE: ExtensionHostRegistry is shared between on- and off-the-record
  // contexts. See also the comment
  // ExtensionHostRegistryFactory::GetBrowserContextToUse().
  static ExtensionHostRegistry* Get(content::BrowserContext* browser_context);

  // Retrieves the factory instance for the ExtensionHostRegistry.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Called when a new ExtensionHost is created, which starts tracking the host
  // (in extension_hosts_) and notifies observers.
  void ExtensionHostCreated(ExtensionHost* extension_host);
  // Called when an ExtensionHost is destroyed. Stops tracking the host and
  // notifies observers.
  void ExtensionHostDestroyed(ExtensionHost* extension_host);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // The active set of ExtensionHosts.
  std::unordered_set<ExtensionHost*> extension_hosts_;

  base::ObserverList<Observer> observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_HOST_REGISTRY_H_

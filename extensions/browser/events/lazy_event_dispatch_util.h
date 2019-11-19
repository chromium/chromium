// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENTS_LAZY_EVENT_DISPATCH_UTIL_H_
#define EXTENSIONS_BROWSER_EVENTS_LAZY_EVENT_DISPATCH_UTIL_H_

#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"

namespace base {
class Version;
}

namespace extensions {

// Utility class to observe extension installation and loading related events
// from lazy contexts.
//
// This class observes ExtensionRegistry and uses ExtensionPrefs to detect
// whether an extension is loaded after (first time) installation or after an
// update.
class LazyEventDispatchUtil : public ExtensionRegistryObserver {
 public:
  // Helps observer with events for lazy event dispatching.
  class Observer {
   public:
    // Called when an extension is loaded after installation, for one of the
    // following scenarios:
    //   1. New extension is installed.
    //   2. An extension is updated and loaded.
    //   3. An extension is enabled after it was disabled during an update.
    virtual void OnExtensionInstalledAndLoaded(
        content::BrowserContext* browser_context,
        const Extension* extension,
        const base::Version& old_version) {}
  };

  explicit LazyEventDispatchUtil(content::BrowserContext* browser_context);
  ~LazyEventDispatchUtil() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

 private:
  bool ReadPendingOnInstallInfoFromPref(const ExtensionId& extension_id,
                                        base::Version* previous_version);
  void RemovePendingOnInstallInfoFromPref(const ExtensionId& extension_id);
  void StorePendingOnInstallInfoToPref(const Extension* extension);

  content::BrowserContext* browser_context_;
  base::ObserverList<Observer>::Unchecked observers_;
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(LazyEventDispatchUtil);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENTS_LAZY_EVENT_DISPATCH_UTIL_H_

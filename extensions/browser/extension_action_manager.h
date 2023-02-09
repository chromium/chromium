// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ACTION_MANAGER_H_
#define EXTENSIONS_BROWSER_EXTENSION_ACTION_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/extension_action/action_info.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ExtensionAction;

// Owns the ExtensionActions associated with each extension.  These actions live
// while an extension is loaded and are destroyed on unload.
class ExtensionActionManager : public KeyedService,
                               public ExtensionRegistryObserver {
 public:
  explicit ExtensionActionManager(content::BrowserContext* browser_context);
  ~ExtensionActionManager() override;

  // Returns this |browser_context|'s ExtensionActionManager. One instance is
  // shared between a BrowserContext and its off-the-record version.
  static ExtensionActionManager* Get(content::BrowserContext* browser_context);

  // Returns the action associated with the extension (specified through the
  // "action", "browser_action", or "page_action" keys), or null if none exists.
  // Since an extension can only declare one of these, this is safe to use
  // anywhere callers simply need to get at the action and don't care about
  // the manifest key.
  ExtensionAction* GetExtensionAction(const Extension& extension) const;

  static void EnsureFactoryBuilt();

 private:
  // Implement ExtensionRegistryObserver.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  raw_ptr<content::BrowserContext> browser_context_;

  // Listen to extension unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // Keyed by Extension ID.  These maps are populated lazily when their
  // ExtensionAction is first requested, and the entries are removed when the
  // extension is unloaded.  Not every extension has an action.
  using ExtIdToActionMap =
      std::map<std::string, std::unique_ptr<ExtensionAction>>;
  mutable ExtIdToActionMap actions_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ACTION_MANAGER_H_

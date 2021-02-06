// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DECLARATIVE_USER_SCRIPT_MANAGER_H_
#define EXTENSIONS_BROWSER_DECLARATIVE_USER_SCRIPT_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/host_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class DeclarativeUserScriptSet;

// Manages a set of DeclarativeUserScriptSet objects for script injections.
class DeclarativeUserScriptManager : public KeyedService,
                                     public ExtensionRegistryObserver {
 public:
  explicit DeclarativeUserScriptManager(
      content::BrowserContext* browser_context);
  ~DeclarativeUserScriptManager() override;

  // Convenience method to return the DeclarativeUserScriptManager for a given
  // |context|.
  static DeclarativeUserScriptManager* Get(content::BrowserContext* context);

  // Gets the user script set for declarative scripts by the given HostId.
  // If one does not exist, a new object will be created.
  DeclarativeUserScriptSet* GetDeclarativeUserScriptSetByID(
      const HostID& host_id);

 private:
  using UserScriptSetMap =
      std::map<HostID, std::unique_ptr<DeclarativeUserScriptSet>>;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Creates a DeclarativeUserScriptSet object.
  DeclarativeUserScriptSet* CreateDeclarativeUserScriptSet(
      const HostID& host_id);

  // A map of DeclarativeUserScriptSets for each host; each set is lazily
  // initialized.
  UserScriptSetMap declarative_user_script_sets_;

  content::BrowserContext* browser_context_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(DeclarativeUserScriptManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_DECLARATIVE_USER_SCRIPT_MANAGER_H_

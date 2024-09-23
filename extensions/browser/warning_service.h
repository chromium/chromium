// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_WARNING_SERVICE_H_
#define EXTENSIONS_BROWSER_WARNING_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/extension_id.h"

// TODO(battre) Remove the Extension prefix.

namespace content {
class BrowserContext;
}

namespace extensions {

// Manages a set of warnings caused by extensions. These warnings (e.g.
// conflicting modifications of network requests by extensions, slow extensions,
// etc.) trigger a warning badge in the UI and and provide means to resolve
// them. This class must be used on the UI thread only.
class WarningService : public KeyedService, public ExtensionRegistryObserver {
 public:
  class Observer {
   public:
    virtual void ExtensionWarningsChanged(
        const ExtensionIdSet& affected_extensions) = 0;
  };

  // |browser_context| may be NULL for testing. In this case, be sure to not
  // insert any warnings.
  explicit WarningService(content::BrowserContext* browser_context);

  WarningService(const WarningService&) = delete;
  WarningService& operator=(const WarningService&) = delete;

  ~WarningService() override;

  // Get the instance of the WarningService for |browser_context|.
  // Redirected in incognito.
  static WarningService* Get(content::BrowserContext* browser_context);

  // Clears all warnings of types contained in |types| and notifies observers
  // of the changed warnings.
  void ClearWarnings(const std::set<Warning::WarningType>& types);

  // Returns all types of warnings effecting extension |extension_id|.
  std::set<Warning::WarningType> GetWarningTypesAffectingExtension(
      const ExtensionId& extension_id) const;

  // Returns all localized warnings for extension |extension_id| in |result|.
  std::vector<std::string> GetWarningMessagesForExtension(
      const ExtensionId& extension_id) const;

  const WarningSet& warnings() const { return warnings_; }

  // Adds a set of warnings and notifies observers if any warning is new.
  void AddWarnings(const WarningSet& warnings);

  // Notifies the WarningService of browser_context |browser_context_id| that
  // new |warnings| occurred and triggers a warning badge.
  static void NotifyWarningsOnUI(void* browser_context_id,
                                 const WarningSet& warnings);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void NotifyWarningsChanged(const ExtensionIdSet& affected_extensions);

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Currently existing warnings.
  WarningSet warnings_;

  const raw_ptr<content::BrowserContext> browser_context_;

  // Listen to extension unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_WARNING_SERVICE_H_

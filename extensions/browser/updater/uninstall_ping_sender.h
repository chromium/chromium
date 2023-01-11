// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UNINSTALL_PING_SENDER_H_
#define EXTENSIONS_BROWSER_UPDATER_UNINSTALL_PING_SENDER_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A class that watches the ExtensionRegistry for uninstall events, and
// uses the UpdateService to send uninstall pings.
class UninstallPingSender : public ExtensionRegistryObserver {
 public:
  enum FilterResult { SEND_PING, DO_NOT_SEND_PING };

  // A callback function that will be called each time an extension is
  // uninstalled, with the result used to determine if a ping should be
  // sent or not.
  using Filter =
      base::RepeatingCallback<FilterResult(const Extension* extension,
                                           UninstallReason reason)>;

  UninstallPingSender(ExtensionRegistry* registry, Filter filter);

  UninstallPingSender(const UninstallPingSender&) = delete;
  UninstallPingSender& operator=(const UninstallPingSender&) = delete;

  ~UninstallPingSender() override;

 protected:
  // ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

 private:
  // Callback for determining whether to send uninstall pings.
  Filter filter_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      observer_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UNINSTALL_PING_SENDER_H_

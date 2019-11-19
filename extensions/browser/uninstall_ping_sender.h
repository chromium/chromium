// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UNINSTALL_PING_SENDER_H_
#define EXTENSIONS_BROWSER_UNINSTALL_PING_SENDER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
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
  using Filter = base::Callback<FilterResult(const Extension* extension,
                                             UninstallReason reason)>;

  UninstallPingSender(ExtensionRegistry* registry, const Filter& filter);
  ~UninstallPingSender() override;

 protected:
  // ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

 private:
  // Callback for determining whether to send uninstall pings.
  Filter filter_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(UninstallPingSender);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UNINSTALL_PING_SENDER_H_

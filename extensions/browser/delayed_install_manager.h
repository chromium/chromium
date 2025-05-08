// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DELAYED_INSTALL_MANAGER_H_
#define EXTENSIONS_BROWSER_DELAYED_INSTALL_MANAGER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/install_gate.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionPrefs;
class ExtensionRegistrar;
class InstallGate;

// Manages a set of extension installs delayed for various reasons.  The reason
// for delayed install is stored in ExtensionPrefs. These are not part of
// ExtensionRegistry because they are not yet installed.
class DelayedInstallManager : public KeyedService {
 public:
  explicit DelayedInstallManager(content::BrowserContext* context);
  DelayedInstallManager(const DelayedInstallManager&) = delete;
  DelayedInstallManager& operator=(const DelayedInstallManager&) = delete;
  ~DelayedInstallManager() override;

  static DelayedInstallManager* Get(content::BrowserContext* context);

  // KeyedService:
  void Shutdown() override;

  // Returns true if an extension is in the delayed install set.
  bool Contains(const ExtensionId& id) const;

  // Adds an extension to the delayed install set.
  void Insert(scoped_refptr<const Extension> extension);

  // Removes an extension from the delayed install set.
  void Remove(const ExtensionId& id);

  // Returns an update for an extension with the specified id, if installation
  // of that update was previously delayed because the extension was in use. If
  // no updates are pending for the extension returns null.
  const Extension* GetPendingExtensionUpdate(const ExtensionId& id) const;

  // Finish install (if possible) of extensions that were still delayed while
  // the browser was shut down.
  void FinishInstallationsDelayedByShutdown();

  // Checks for delayed installation for all pending installs.
  void MaybeFinishDelayedInstallations();

  // Attempts finishing installation of an update for an extension with the
  // specified id, when installation of that extension was previously delayed.
  // `install_immediately` - Whether the extension should be installed if it's
  // currently in use.
  // Returns whether the extension installation was finished.
  bool FinishDelayedInstallationIfReady(const ExtensionId& extension_id,
                                        bool install_immediately);

  // Register/unregister an InstallGate with the service.
  void RegisterInstallGate(ExtensionPrefs::DelayReason reason,
                           InstallGate* install_delayer);
  void UnregisterInstallGate(InstallGate* install_delayer);

  // Helper to determine if installing an extensions should proceed immediately,
  // or if we should delay the install until further notice, or if the install
  // should be aborted. A pending install is delayed or aborted when any of the
  // delayers say so and only proceeds when all delayers return INSTALL.
  // `extension` is the extension to be installed. `install_immediately` is the
  // install flag set with the install. `reason` is the reason associated with
  // the install delayer that wants to defer or abort the install.
  InstallGate::Action ShouldDelayExtensionInstall(
      const Extension* extension,
      bool install_immediately,
      ExtensionPrefs::DelayReason* reason) const;

  const ExtensionSet& delayed_installs() const { return delayed_installs_; }

 private:
  raw_ptr<ExtensionPrefs> extension_prefs_;
  raw_ptr<ExtensionRegistrar> extension_registrar_;

  ExtensionSet delayed_installs_;

  using InstallGateRegistry = std::map<ExtensionPrefs::DelayReason,
                                       raw_ptr<InstallGate, CtnExperimental>>;
  InstallGateRegistry install_delayer_registry_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_DELAYED_INSTALL_MANAGER_H_

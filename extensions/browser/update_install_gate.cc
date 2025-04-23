// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/update_install_gate.h"

#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

UpdateInstallGate::UpdateInstallGate(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

InstallGate::Action UpdateInstallGate::ShouldDelay(const Extension* extension,
                                                   bool install_immediately) {
  // Allow installation when |install_immediately| is set or ExtensionSystem
  // is not ready. UpdateInstallGate blocks update when the old version of
  // the extension is not idle (i.e. in use). When ExtensionSystem is not
  // ready, the old version is definitely idle, so the installation is allowed
  // to proceeded. This essentially allows the delayed installation to happen
  // during the initialization of ExtensionService.
  if (install_immediately ||
      !ExtensionSystem::Get(browser_context_)->is_ready()) {
    return INSTALL;
  }

  const Extension* old = ExtensionRegistry::Get(browser_context_)
                             ->GetInstalledExtension(extension->id());
  // If there is no old extension, this is not an update, so don't delay.
  if (!old) {
    return INSTALL;
  }

  if (extensions::BackgroundInfo::HasPersistentBackgroundPage(old)) {
    const char kOnUpdateAvailableEvent[] = "runtime.onUpdateAvailable";
    // Delay installation if the extension listens for the onUpdateAvailable
    // event.
    return EventRouter::Get(browser_context_)
                   ->ExtensionHasEventListener(extension->id(),
                                               kOnUpdateAvailableEvent)
               ? DELAY
               : INSTALL;
  } else {
    // Delay installation if the extension is not idle.
    return !util::IsExtensionIdle(extension->id(), browser_context_) ? DELAY
                                                                     : INSTALL;
  }
}

}  // namespace extensions

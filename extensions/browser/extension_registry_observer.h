// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_

#include "extensions/browser/uninstall_reason.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionRegistry;
enum class UnloadedExtensionReason;

// Observer for ExtensionRegistry. Exists in a separate header file to reduce
// the include file burden for typical clients of ExtensionRegistry.
//
// There are separate event categories for loading (the OnExtensionLoaded,
// OnExtensionReady and OnExtensionUnloaded events) and installing (the
// OnExtensionWillBeInstalled, OnExtensionInstalled and OnExtensionUninstalled)
// extensions.
//
// For example, comparing OnExtensionLoaded and OnExtensionInstalled,
// OnExtensionLoaded is called whenever an extension is added to the "enabled"
// set of the extension registry. This includes:
//
//  - Extensions being loaded at Chrome startup.
//  - Extensions being reloaded:
//    * as part of an update.
//    * from a crash.
//    * from a disabled state (if the user toggled disabled -> enabled).
//    * as part of internal bookkeeping (we reload extensions on file access
//      being granted, for instance).
//    * if the extension requested it (chrome.runtime.reload()).
//    * probably others.
//  - New extensions being loaded for the first time (as part of installation).
//
// OnExtensionInstalled is called when a *new* extension is added, *or* when an
// extension is updated to a *new* version. It is not called for existing
// extensions being loaded at startup, etc. In a common run of Chrome, you
// probably won't get many "OnInstalled" events.
//
// As a general rule, most sites should observe OnExtensionLoaded, because they
// want to see "what are the enabled extensions".
class ExtensionRegistryObserver {
 public:
  virtual ~ExtensionRegistryObserver() = default;

  // Called after an extension is loaded. The extension will exclusively exist
  // in the enabled_extensions set of ExtensionRegistry.
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const Extension* extension) {}

  // Called after an extension is loaded and all necessary browser state is
  // initialized to support the start of the extension's child process.
  virtual void OnExtensionReady(content::BrowserContext* browser_context,
                                const Extension* extension) {}

  // Called after an extension is unloaded. The extension no longer exists in
  // the set |ExtensionRegistry::enabled_extensions()|, but it can still be a
  // member of one of the other sets, like disabled, blocklisted or terminated.
  virtual void OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   UnloadedExtensionReason reason) {}

  // Called when |extension| is about to be installed. |is_update| is true if
  // the installation is the result of it updating, in which case |old_name| is
  // the name of the extension's previous version.
  // The ExtensionRegistry will not be tracking |extension| at the time this
  // event is fired, but will be immediately afterwards (note: not necessarily
  // enabled; it might be installed in the disabled or even blocklisted sets,
  // for example).
  // Note that it's much more common to care about extensions being loaded
  // (OnExtensionLoaded).
  //
  // TODO(tmdiep): We should stash the state of the previous extension version
  // somewhere and have observers retrieve it. |is_update|, and |old_name| can
  // be removed when this is done.
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      const std::string& old_name) {}

  // Called when the installation of |extension| is complete. At this point the
  // extension is tracked in one of the ExtensionRegistry sets, but is not
  // necessarily enabled.
  virtual void OnExtensionInstalled(content::BrowserContext* browser_context,
                                    const Extension* extension,
                                    bool is_update) {}

  // Called after an extension is uninstalled. The extension no longer exists in
  // any of the ExtensionRegistry sets (enabled, disabled, etc.).
  virtual void OnExtensionUninstalled(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UninstallReason reason) {}

  // Called after the uninstallation of an extension is denied.
  virtual void OnExtensionUninstallationDenied(
      content::BrowserContext* browser_context,
      const Extension* extension) {}

  // Notifies observers that the observed object is going away.
  virtual void OnShutdown(ExtensionRegistry* registry) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_

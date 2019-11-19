// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
class DevToolsAgentHost;
}  // namespace content

namespace extensions {

class Extension;
class ExtensionHost;
class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionSystem;
class RendererStartupHelper;

// ExtensionRegistrar drives the stages of registering and unregistering
// extensions for a BrowserContext. It uses the ExtensionRegistry to track
// extension states. Other classes may query the ExtensionRegistry directly,
// but eventually only ExtensionRegistrar will be able to make changes to it.
class ExtensionRegistrar {
 public:
  // How to surface an extension load error, e.g. showing an error dialog. The
  // actual behavior is up to the embedder.
  enum class LoadErrorBehavior {
    kQuiet = 0,  // Just log the error.
    kNoisy,      // Show an error dialog.
  };

  // Delegate for embedder-specific functionality like policy and permissions.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Called before |extension| is added. |old_extension| is the extension
    // being replaced, in the case of a reload or upgrade.
    virtual void PreAddExtension(const Extension* extension,
                                 const Extension* old_extension) = 0;

    // Handles updating the browser context when an extension is activated
    // (becomes enabled).
    virtual void PostActivateExtension(
        scoped_refptr<const Extension> extension) = 0;

    // Handles updating the browser context when an enabled extension is
    // deactivated (whether disabled or removed).
    virtual void PostDeactivateExtension(
        scoped_refptr<const Extension> extension) = 0;

    // Given an extension ID and/or path, loads that extension as a reload.
    virtual void LoadExtensionForReload(
        const ExtensionId& extension_id,
        const base::FilePath& path,
        LoadErrorBehavior load_error_behavior) = 0;

    // Returns true if the extension is allowed to be enabled or disabled,
    // respectively.
    virtual bool CanEnableExtension(const Extension* extension) = 0;
    virtual bool CanDisableExtension(const Extension* extension) = 0;

    // Returns true if the extension should be blocked.
    virtual bool ShouldBlockExtension(const Extension* extension) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // The provided Delegate should outlive this object.
  ExtensionRegistrar(content::BrowserContext* browser_context,
                     Delegate* delegate);
  virtual ~ExtensionRegistrar();

  // Adds the extension to the ExtensionRegistry. The extension will be added to
  // the enabled, disabled, blacklisted or blocked set. If the extension is
  // added as enabled, it will be activated.
  void AddExtension(scoped_refptr<const Extension> extension);

  // Removes |extension| from the extension system by deactivating it if it is
  // enabled and removing references to it from the ExtensionRegistry's
  // enabled or disabled sets.
  // Note: Extensions will not be removed from other sets (terminated,
  // blacklisted or blocked). ExtensionService handles that, since it also adds
  // it to those sets. TODO(michaelpg): Make ExtensionRegistrar the sole mutator
  // of ExtensionRegsitry to simplify this usage.
  void RemoveExtension(const ExtensionId& extension_id,
                       UnloadedExtensionReason reason);

  // If the extension is disabled, marks it as enabled and activates it for use.
  // Otherwise, simply updates the ExtensionPrefs. (Blacklisted or blocked
  // extensions cannot be enabled.)
  void EnableExtension(const ExtensionId& extension_id);

  // Marks |extension| as disabled and deactivates it. The ExtensionRegistry
  // retains a reference to it, so it can be enabled later.
  void DisableExtension(const ExtensionId& extension_id, int disable_reasons);

  // Attempts to reload the specified extension by disabling it if it is enabled
  // and requesting the Delegate load it again.
  // NOTE: Reloading an extension can invalidate |extension_id| and Extension
  // pointers for the given extension. Consider making a copy of |extension_id|
  // first and retrieving a new Extension pointer afterwards.
  void ReloadExtension(const ExtensionId extension_id,
                       LoadErrorBehavior load_error_behavior);

  // TODO(michaelpg): Add methods for blacklisting and blocking extensions.

  // Deactivates the extension, adding its id to the list of terminated
  // extensions.
  void TerminateExtension(const ExtensionId& extension_id);

  // Removes the extension from the terminated list. TODO(michaelpg): Make a
  // private implementation detail when no longer called from ExtensionService.
  void UntrackTerminatedExtension(const ExtensionId& extension_id);

  // Returns true if the extension is enabled (including terminated), or if it
  // is not loaded but isn't explicitly disabled in preferences.
  bool IsExtensionEnabled(const ExtensionId& extension_id) const;

  // Called after the render view for the background page with the associated
  // host is created.
  void DidCreateRenderViewForBackgroundPage(ExtensionHost* host);

 private:
  // Adds the extension to the appropriate registry set, based on ExtensionPrefs
  // and our |delegate_|. Activates the extension if it's added to the enabled
  // set.
  void AddNewExtension(scoped_refptr<const Extension> extension);

  // Activates |extension| by marking it enabled and notifying other components
  // about it.
  void ActivateExtension(const Extension* extension, bool is_newly_added);

  // Triggers the unloaded notifications to deactivate an extension.
  void DeactivateExtension(const Extension* extension,
                           UnloadedExtensionReason reason);

  // Given an extension that was disabled for reloading, completes the reload
  // by replacing the old extension with the new version and enabling it.
  // Returns true on success.
  bool ReplaceReloadedExtension(scoped_refptr<const Extension> extension);

  // Marks the extension ready after URLRequestContexts have been updated on
  // the IO thread.
  void OnExtensionRegisteredWithRequestContexts(
      scoped_refptr<const Extension> extension);

  // Upon reloading an extension, spins up its lazy background page if
  // necessary.
  void MaybeSpinUpLazyBackgroundPage(const Extension* extension);

  content::BrowserContext* const browser_context_;

  // Delegate provided in the constructor. Should outlive this object.
  Delegate* const delegate_;

  // Keyed services we depend on. Cached here for repeated access.
  ExtensionSystem* const extension_system_;
  ExtensionPrefs* const extension_prefs_;
  ExtensionRegistry* const registry_;
  RendererStartupHelper* const renderer_helper_;

  // Map of DevToolsAgentHost instances that are detached,
  // waiting for an extension to be reloaded.
  using OrphanedDevTools =
      std::map<std::string, scoped_refptr<content::DevToolsAgentHost>>;
  OrphanedDevTools orphaned_dev_tools_;

  // Map unloaded extensions' ids to their paths. When a temporarily loaded
  // extension is unloaded, we lose the information about it and don't have
  // any in the extension preferences file.
  using UnloadedExtensionPathMap = std::map<ExtensionId, base::FilePath>;
  UnloadedExtensionPathMap unloaded_extension_paths_;

  // Store the ids of reloading extensions. We use this to re-enable extensions
  // which were disabled for a reload.
  ExtensionIdSet reloading_extensions_;

  base::WeakPtrFactory<ExtensionRegistrar> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionRegistrar);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_

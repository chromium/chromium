// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_

#include <memory>
#include <set>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace base {
class CommandLine;
}  // namespace base

namespace content {
class BrowserContext;
class DevToolsAgentHost;
}  // namespace content

namespace extensions {
class DelayedInstallManager;
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
class ExtensionRegistrar : public KeyedService, public ProcessManagerObserver {
 public:
  // Delegate for embedder-specific functionality like policy and permissions.
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    // Called before `extension` is added. `old_extension` is the extension
    // being replaced, in the case of a reload or upgrade.
    virtual void PreAddExtension(const Extension* extension,
                                 const Extension* old_extension) = 0;

    // Handles extension install tasks before AddExtension.
    virtual void OnAddNewOrUpdatedExtension(const Extension* extension) = 0;

    // Handles updating the browser context when an extension is activated
    // (becomes enabled).
    virtual void PostActivateExtension(
        scoped_refptr<const Extension> extension) = 0;

    // Handles updating the browser context when an enabled extension is
    // deactivated (whether disabled or removed).
    virtual void PostDeactivateExtension(
        scoped_refptr<const Extension> extension) = 0;

    // Called before `extension` is uninstalled. Performs the operations
    // necessary before `extension` is uninstalled.
    virtual void PreUninstallExtension(
        scoped_refptr<const Extension> extension) = 0;

    // Called after `extension` is uninstalled. Performs the operations to
    // clean up the extensions files, etc.
    virtual void PostUninstallExtension(
        scoped_refptr<const Extension> extension,
        base::OnceClosure done_callback) = 0;

    // Given an extension ID and/or path, loads that extension as a reload with
    // noisy load error behavior.
    virtual void LoadExtensionForReload(const ExtensionId& extension_id,
                                        const base::FilePath& path) = 0;
    // Given an extension ID and/or path, loads that extension as a reload with
    // quiet load error behavior.
    virtual void LoadExtensionForReloadWithQuietFailure(
        const ExtensionId& extension_id,
        const base::FilePath& path) = 0;

    // Informs the user that an extension was disabled after upgrading to higher
    // permissions. If `is_remote_install` is true, the extension was disabled
    // because it was installed remotely.
    virtual void ShowExtensionDisabledError(const Extension* extension,
                                            bool is_remote_install) = 0;

    // Returns true if the extension is allowed to be enabled or disabled,
    // respectively.
    virtual bool CanEnableExtension(const Extension* extension) = 0;
    virtual bool CanDisableExtension(const Extension* extension) = 0;

    // Updates the `extension`s granted permissions lists to include all
    // permissions in the `extensions`s manifest.
    virtual void GrantActivePermissions(const Extension* extension) = 0;

    // Checks if there are any new external extensions to notify the user about.
    virtual void UpdateExternalExtensionAlert() = 0;

    // Informs the service that an extension's files are in place for loading.
    //
    // `extension`                the extension
    // `page_ordinal`             the location of the extension in the app
    //                            launcher
    // `install_flags`            a bitmask of InstallFlags
    // `ruleset_install_prefs`    Install prefs needed for the Declarative Net
    //                            Request API.
    virtual void OnExtensionInstalled(
        const Extension* extension,
        const syncer::StringOrdinal& page_ordinal,
        int install_flags,
        base::Value::Dict ruleset_install_prefs) = 0;
  };

  explicit ExtensionRegistrar(content::BrowserContext* browser_context);

  ExtensionRegistrar(const ExtensionRegistrar&) = delete;
  ExtensionRegistrar& operator=(const ExtensionRegistrar&) = delete;

  ~ExtensionRegistrar() override;

  // Returns the instance for the given `browser_context`.
  static ExtensionRegistrar* Get(content::BrowserContext* browser_context);

  // The provided `delegate` should outlive this object. May be called multiple
  // times, for example to reset the delegate in tests.
  void Init(Delegate* delegate,
            bool extensions_enabled,
            const base::CommandLine* command_line,
            const base::FilePath& install_directory,
            const base::FilePath& unpacked_install_directory);

  // Returns true if the registrar has been initialized.
  bool IsInitialized() const;

  // Returns a weak pointer to `this`.
  base::WeakPtr<ExtensionRegistrar> GetWeakPtr();

  // KeyedService overrides:
  // Called when the associated Profile is going to be destroyed.
  void Shutdown() override;

  // Adds the extension to the ExtensionRegistry. The extension will be added to
  // the enabled, disabled, blocklisted or blocked set. If the extension is
  // added as enabled, it will be activated.
  void AddExtension(scoped_refptr<const Extension> extension);

  // Updates preferences for a new or updated extension; notifies observers that
  // the extension is installed, e.g., to update event handlers on background
  // pages; and performs other extension install tasks before calling
  // AddExtension.
  // `install_flags` is a bitmask of InstallFlags.
  void AddNewOrUpdatedExtension(const Extension* extension,
                                const base::flat_set<int>& disable_reasons,
                                int install_flags,
                                const syncer::StringOrdinal& page_ordinal,
                                const std::string& install_parameter,
                                base::Value::Dict ruleset_install_prefs);

  // Informs the service that an extension's files are in place for loading.
  //
  // `extension`                the extension
  // `page_ordinal`             the location of the extension in the app
  //                            launcher
  // `install_flags`            a bitmask of InstallFlags
  // `ruleset_install_prefs`    Install prefs needed for the Declarative Net
  //                            Request API.
  void OnExtensionInstalled(const Extension* extension,
                            const syncer::StringOrdinal& page_ordinal,
                            int install_flags,
                            base::Value::Dict ruleset_install_prefs = {});
  void OnExtensionInstalled(const Extension* extension,
                            const syncer::StringOrdinal& page_ordinal) {
    OnExtensionInstalled(extension, page_ordinal,
                         static_cast<int>(kInstallFlagNone));
  }

  // Removes `extension` from the extension system by deactivating it if it is
  // enabled and removing references to it from the ExtensionRegistry's
  // enabled, disabled or terminated sets.
  // Note: Extensions will not be removed from other sets (blocklisted or
  // blocked). ExtensionService handles that, since it also adds it to those
  // sets. TODO(michaelpg): Make ExtensionRegistrar the sole mutator of
  // ExtensionRegistry to simplify this usage.
  void RemoveExtension(const ExtensionId& extension_id,
                       UnloadedExtensionReason reason);

  // If the extension is disabled, marks it as enabled and activates it for use.
  // Otherwise, simply updates the ExtensionPrefs. (Blocklisted or blocked
  // extensions cannot be enabled.)
  void EnableExtension(const ExtensionId& extension_id);

  // Marks `extension` as disabled and deactivates it. The ExtensionRegistry
  // retains a reference to it, so it can be enabled later.
  void DisableExtension(const ExtensionId& extension_id,
                        const DisableReasonSet& disable_reasons);

  // Any code which needs to write unknown reasons should use the
  // methods below, which operate on raw integers. This is needed for scenarios
  // like Sync where unknown reasons can be synced from newer versions of the
  // browser to older versions. The method above will trigger undefined behavior
  // when unknown values are casted to DisableReason while constructing
  // DisableReasonSet. Most code should use the method above. We want to limit
  // the usage of the method below, so it is guarded by a passkey.
  void DisableExtensionWithRawReasons(
      ExtensionPrefs::DisableReasonRawManipulationPasskey,
      const ExtensionId& extension_id,
      base::flat_set<int> disable_reasons);

  // Same as `DisableExtension`, but assumes that the request to disable
  // `extension_id` originates from `source_extension` when evaluating whether
  // the extension can be disabled. Please see `ExtensionMayModifySettings`
  // for details.
  void DisableExtensionWithSource(const Extension* source_extension,
                                  const ExtensionId& extension_id,
                                  disable_reason::DisableReason disable_reason);

  // Helper to get the disable reasons for an installed (or upgraded) extension.
  // Returning an empty set indicates that we should enable this extension
  // initially.
  base::flat_set<int> GetDisableReasonsOnInstalled(const Extension* extension);

  // Attempts to enable all disabled extensions which the only disabled reason
  // is reloading.
  void EnabledReloadableExtensions();

  // Check if we have preferences for the component extension and, if not or if
  // the stored version differs, install the extension (without requirements
  // checking) before calling AddExtension.
  void AddComponentExtension(const Extension* extension);

  // Removes the specified component extension.
  void RemoveComponentExtension(const std::string& extension_id);

  // Removes the disable reason and enable the extension if there are no disable
  // reasons left and is not blocked for another reason.
  void RemoveDisableReasonAndMaybeEnable(const std::string& extension_id,
                                         disable_reason::DisableReason reason);

  // Attempts to reload extension with noisy failures.
  void ReloadExtension(const ExtensionId& extension_id);
  // Attempts to reload extension with suppressing noisy failures.
  void ReloadExtensionWithQuietFailure(const ExtensionId& extension_id);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist. `reason` lets the caller specify why the
  // extension is uninstalled.
  // Note: this method synchronously removes the extension from the
  // set of installed extensions stored in the ExtensionRegistry, but will
  // asynchronously remove site-related data and the files stored on disk.
  // Returns true if an uninstall was successfully triggered; this can fail if
  // the extension cannot be uninstalled (such as a policy force-installed
  // extension).
  // `done_callback` is synchronously invoked once the site-related data and the
  // files stored on disk are removed. If such a callback is not needed, pass in
  // a null callback (base::NullCallback()).
  bool UninstallExtension(
      const std::string& extension_id,
      UninstallReason reason,
      std::u16string* error,
      base::OnceClosure done_callback = base::NullCallback());

  // Uninstalls extensions that have been migrated to component extensions.
  void UninstallMigratedExtensions(base::span<const char* const> migrated_ids);

  // Finishes installing `extension` and notifying the observers.
  void FinishInstallation(const Extension* extension);

  // Helper method to determine if an extension can be blocked.
  bool CanBlockExtension(const Extension* extension) const;

  // Puts all extensions in a blocked state: Unloading every extension, and
  // preventing them from ever loading until UnblockAllExtensions is called.
  // This state is stored in preferences, so persists until Chrome restarts.
  //
  // Component, external component and allowlisted policy installed extensions
  // are exempt from being Blocked (see CanBlockExtension in .cc file).
  void BlockAllExtensions();

  // All blocked extensions are reverted to their previous state, and are
  // reloaded. Newly added extensions are no longer automatically blocked.
  void UnblockAllExtensions();

  // Takes Safe Browsing and Omaha malware blocklist states into account and
  // decides whether to remove the extension from the blocklist and reload it.
  // Called when a blocklisted extension is removed from the Safe Browsing
  // malware blocklist or Omaha malware blocklist. Also clears the acknowledged
  // state if the extension is reloaded.
  void OnBlocklistStateRemoved(const std::string& extension_id);

  // Takes acknowledged malware blocklist state into account and decides whether
  // to add the extension to the blocklist and unload it. Called when the
  // extension is added to the Safe Browsing malware blocklist or the Omaha
  // malware blocklist.
  void OnBlocklistStateAdded(const std::string& extension_id);

  // Takes Safe Browsing and Omaha blocklist states into account and decides
  // whether to remove greylist disabled reason. Called when a greylisted
  // state is removed from the Safe Browsing blocklist or Omaha blocklist. Also
  // clears all acknowledged states if the greylist disabled reason is removed.
  void OnGreylistStateRemoved(const std::string& extension_id);

  // Takes acknowledged blocklist states into account and decides whether to
  // disable the greylisted extension. Called when a new greylisted state is
  // added to the Safe Browsing blocklist or Omaha blocklist.
  void OnGreylistStateAdded(const std::string& extension_id,
                            BitMapBlocklistState new_state);

  // Simulates an extension being blocklisted for tests.
  void BlocklistExtensionForTest(const std::string& extension_id);

  // Simulates an extension being greylisted for tests.
  void GreylistExtensionForTest(const std::string& extension_id,
                                const BitMapBlocklistState& state);

  // Disables the automatic spin-up of lazy contexts. This should only be used
  // in tests.
  [[nodiscard]]
  static base::AutoReset<bool> DisableLazyContextSpinupForTest();

  // Deactivates the extension, adding its id to the list of terminated
  // extensions.
  void TerminateExtension(const ExtensionId& extension_id);

  // Removes the extension from the terminated list. TODO(michaelpg): Make a
  // private implementation detail when no longer called from ExtensionService.
  void UntrackTerminatedExtension(const ExtensionId& extension_id);

  // Returns true if the extension is enabled (including terminated), or if it
  // is not loaded but isn't explicitly disabled in preferences.
  bool IsExtensionEnabled(const ExtensionId& extension_id) const;

  // Called after the renderer main frame for the background page with the
  // associated host is created.
  void DidCreateMainFrameForBackgroundPage(ExtensionHost* host);

  void OnUnpackedExtensionReloadFailed(const base::FilePath& path);

  // Updates the `extension`s granted permissions lists to include all
  // permissions in the `extension`s manifest and re-enables the
  // extension.
  void GrantPermissionsAndEnableExtension(const Extension& extension);

  // Adds to the set of allowlisted enabled extensions loaded from the
  // --disable-extensions-except command line flag.
  void AddDisableFlagExemptedExtension(const ExtensionId& extension_id);

  // Simple accessors.
  bool extensions_enabled() const { return extensions_enabled_; }
  bool block_extensions() const { return block_extensions_; }
  const base::FilePath& install_directory() const { return install_directory_; }
  const base::FilePath& unpacked_install_directory() const {
    return unpacked_install_directory_;
  }

  void set_extensions_enabled_for_test(bool value) {
    extensions_enabled_ = value;
  }

 private:
  // How to surface an extension load error, e.g. showing an error dialog. The
  // actual behavior is up to the embedder.
  enum class LoadErrorBehavior {
    kQuiet = 0,  // Just log the error.
    kNoisy,      // Show an error dialog.
  };

  // Adds the extension to the appropriate registry set, based on ExtensionPrefs
  // and our `delegate_`. Activates the extension if it's added to the enabled
  // set.
  void AddNewExtension(scoped_refptr<const Extension> extension);

  // Activates `extension` by marking it enabled and notifying other components
  // about it.
  void ActivateExtension(const Extension* extension, bool is_newly_added);

  // Triggers the unloaded notifications to deactivate an extension.
  void DeactivateExtension(const Extension* extension,
                           UnloadedExtensionReason reason);

  // Attempts to reload the specified extension by disabling it if it is enabled
  // and requesting the Delegate load it again.
  // Because reloading can invalidate a reference to the ID. So make a copy of
  // `extension_id` first by passing value and retrieve a new Extension pointer
  // afterwards.
  void DoReloadExtension(ExtensionId extension_id,
                         LoadErrorBehavior load_error_behavior);

  // Unregister the service worker that is not from manifest and has extension
  // root scope.
  void UnregisterServiceWorkerWithRootScope(const Extension* extension);
  void NotifyServiceWorkerUnregistered(const ExtensionId& extension_id,
                                       bool worker_previously_registered,
                                       blink::ServiceWorkerStatusCode status);

  // Given an extension that was disabled for reloading, completes the reload
  // by replacing the old extension with the new version and enabling it.
  // Returns true on success.
  bool ReplaceReloadedExtension(scoped_refptr<const Extension> extension);

  // Upon reloading an extension, spins up its context if necessary.
  void MaybeSpinUpLazyContext(const Extension* extension, bool is_newly_added);

  // ProcessManagerObserver overrides
  void OnStartedTrackingServiceWorkerInstance(
      const WorkerId& worker_id) override;

  // Returns true if `extension` can be added.
  bool CanAddExtension(const Extension* extension) const;

  // Returns true if `extension` should be blocked.
  bool ShouldBlockExtension(const Extension* extension) const;

  const raw_ptr<content::BrowserContext> browser_context_;

  // Delegate provided by SetDelegate. Should outlive this object.
  raw_ptr<Delegate> delegate_;

  // Whether or not extensions are enabled.
  bool extensions_enabled_ = true;

  // The full path to the directory where extensions are installed.
  base::FilePath install_directory_;

  // The full path to the directory where unpacked (e.g. from .zip files)
  // extensions are installed.
  base::FilePath unpacked_install_directory_;

  // Keyed services we depend on. Cached here for repeated access.
  // TODO(crbug.com/398014892): Figure out a way to break the dependency
  // between ExtensionRegistrar and ExtensionSystem.
  raw_ptr<ExtensionSystem> extension_system_;
  const raw_ptr<ExtensionPrefs> extension_prefs_;
  const raw_ptr<ExtensionRegistry> registry_;
  const raw_ptr<RendererStartupHelper> renderer_helper_;
  raw_ptr<DelayedInstallManager> delayed_install_manager_ = nullptr;

  // Map of DevToolsAgentHost instances that are detached,
  // waiting for an extension to be reloaded.
  using OrphanedDevTools =
      std::map<std::string,
               std::vector<scoped_refptr<content::DevToolsAgentHost>>>;
  OrphanedDevTools orphaned_dev_tools_;

  // Map unloaded extensions' ids to their paths. When a temporarily loaded
  // extension is unloaded, we lose the information about it and don't have
  // any in the extension preferences file.
  using UnloadedExtensionPathMap = std::map<ExtensionId, base::FilePath>;
  UnloadedExtensionPathMap unloaded_extension_paths_;

  // Store the ids of reloading extensions. We use this to re-enable extensions
  // which were disabled for a reload.
  ExtensionIdSet reloading_extensions_;

  // Store the paths of extensions that failed to reload. We use this to retry
  // reload.
  std::set<base::FilePath> failed_to_reload_unpacked_extensions_;

  // Set of allowlisted enabled extensions loaded from the
  // --disable-extensions-except command line flag.
  std::set<ExtensionId> disable_flag_exempted_extensions_;

  // Set to true if extensions are all to be blocked.
  bool block_extensions_ = false;

  base::ScopedObservation<ProcessManager, ProcessManagerObserver>
      process_manager_observation_{this};
  base::WeakPtrFactory<ExtensionRegistrar> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_

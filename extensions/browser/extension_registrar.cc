// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registrar.h"

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/delayed_install_manager.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/task_queue_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

using content::DevToolsAgentHost;
using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

BASE_FEATURE(kExtensionUpdatesImmediatelyUnregisterWorker,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool g_disable_lazy_context_spinup_for_test = false;

}  // namespace

ExtensionRegistrar::ExtensionRegistrar(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      extension_system_(ExtensionSystem::Get(browser_context)),
      extension_prefs_(ExtensionPrefs::Get(browser_context)),
      registry_(ExtensionRegistry::Get(browser_context)),
      renderer_helper_(
          RendererStartupHelperFactory::GetForBrowserContext(browser_context)) {
  // ExtensionRegistrar is created by ExtensionSystem via ExtensionService, and
  // ChromeExtensionSystemFactory depends on ProcessManager, so this should be
  // safe.
  auto* process_manager = ProcessManager::Get(browser_context_);
  DCHECK(process_manager);
  process_manager_observation_.Observe(process_manager);
}

ExtensionRegistrar::~ExtensionRegistrar() = default;

// static
ExtensionRegistrar* ExtensionRegistrar::Get(content::BrowserContext* context) {
  return ExtensionRegistrarFactory::GetForBrowserContext(context);
}

void ExtensionRegistrar::Init(
    Delegate* delegate,
    bool extensions_enabled,
    const base::CommandLine* command_line,
    const base::FilePath& install_directory,
    const base::FilePath& unpacked_install_directory) {
  delegate_ = delegate;
  // Figure out if extension installation should be enabled.
  if (ExtensionsBrowserClient::Get()->AreExtensionsDisabled(*command_line,
                                                            browser_context_)) {
    extensions_enabled = false;
  }

  extensions_enabled_ = extensions_enabled;
  install_directory_ = install_directory;
  unpacked_install_directory_ = unpacked_install_directory;

  // TODO(https://crbug.com/410635478): We can't put this in ctor because
  // there's a KeyedService cycle between DelayedInstallManager and
  // ExtensionRegistrar.
  delayed_install_manager_ = DelayedInstallManager::Get(browser_context_);
}

bool ExtensionRegistrar::IsInitialized() const {
  // The registrar is initialized if a delegate has been assigned.
  return !!delegate_;
}

base::WeakPtr<ExtensionRegistrar> ExtensionRegistrar::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ExtensionRegistrar::Shutdown() {
  // Setting to `nullptr`, because this raw pointer may become dangling once
  // the `ExtensionSystem` keyed service is destroyed.
  extension_system_ = nullptr;
  delegate_ = nullptr;
  delayed_install_manager_ = nullptr;
}

void ExtensionRegistrar::AddExtension(
    scoped_refptr<const Extension> extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!Manifest::IsValidLocation(extension->location())) {
    // TODO(devlin): We should *never* add an extension with an invalid
    // location, but some bugs (e.g. crbug.com/692069) seem to indicate we do.
    // Track down the cases when this can happen, and remove this
    // DumpWithoutCrashing() (possibly replacing it with a CHECK).
    DEBUG_ALIAS_FOR_CSTR(extension_id_copy, extension->id().c_str(), 33);
    ManifestLocation location = extension->location();
    int creation_flags = extension->creation_flags();
    Manifest::Type type = extension->manifest()->type();
    base::debug::Alias(&location);
    base::debug::Alias(&creation_flags);
    base::debug::Alias(&type);
    NOTREACHED();
  }

  if (!CanAddExtension(extension.get())) {
    return;
  }

  bool is_extension_loaded = false;
  const Extension* old = registry_->GetInstalledExtension(extension->id());
  if (old) {
    is_extension_loaded = true;
    int version_compare_result = extension->version().CompareTo(old->version());
    // Other than for unpacked extensions, we should not be downgrading.
    if (!Manifest::IsUnpackedLocation(extension->location()) &&
        version_compare_result < 0) {
      // TODO(crbug.com/41369768): It would be awfully nice to CHECK this,
      // but that's caused problems. There are apparently times when this
      // happens that we aren't accounting for. We should track those down and
      // fix them, but it can be tricky.
      DUMP_WILL_BE_NOTREACHED()
          << "Attempted to downgrade extension." << "\nID: " << extension->id()
          << "\nOld Version: " << old->version()
          << "\nNew Version: " << extension->version()
          << "\nLocation: " << extension->location();
      return;
    }
  }

  // If the extension was disabled for a reload, we will enable it.
  bool was_reloading = reloading_extensions_.erase(extension->id()) > 0;

  // The extension is now loaded; remove its data from unloaded extension map.
  unloaded_extension_paths_.erase(extension->id());

  // If a terminated extension is loaded, remove it from the terminated list.
  UntrackTerminatedExtension(extension->id());

  // Notify the delegate we will add the extension.
  CHECK(delegate_);
  delegate_->PreAddExtension(extension.get(), old);

  if (was_reloading) {
    failed_to_reload_unpacked_extensions_.erase(extension->path());
    ReplaceReloadedExtension(extension);
  } else {
    if (is_extension_loaded) {
      // To upgrade an extension in place, remove the old one and then activate
      // the new one. ReloadExtension disables the extension, which is
      // sufficient.
      RemoveExtension(extension->id(), UnloadedExtensionReason::UPDATE);
      UnregisterServiceWorkerWithRootScope(extension.get());
    }
    AddNewExtension(extension);
  }

  if (registry_->disabled_extensions().Contains(extension->id())) {
    // Show the extension disabled error if a permissions increase or a remote
    // installation is the reason it was disabled, and no other reasons exist.
    DisableReasonSet reasons =
        extension_prefs_->GetDisableReasons(extension->id());
    const DisableReasonSet error_reasons = {
        disable_reason::DISABLE_PERMISSIONS_INCREASE,
        disable_reason::DISABLE_REMOTE_INSTALL};
    DisableReasonSet other_reasons =
        base::STLSetDifference<DisableReasonSet>(reasons, error_reasons);

    if (!reasons.empty() && other_reasons.empty()) {
      delegate_->ShowExtensionDisabledError(
          extension.get(),
          extension_prefs_->HasDisableReason(
              extension->id(), disable_reason::DISABLE_REMOTE_INSTALL));
    }
  }
}

void ExtensionRegistrar::AddNewExtension(
    scoped_refptr<const Extension> extension) {
  if (blocklist_prefs::IsExtensionBlocklisted(extension->id(),
                                              extension_prefs_)) {
    DCHECK(!Manifest::IsComponentLocation(extension->location()));
    // Only prefs is checked for the blocklist. We rely on callers to check the
    // blocklist before calling into here, e.g. CrxInstaller checks before
    // installation then threads through the install and pending install flow
    // of this class, and ExtensionService checks when loading installed
    // extensions.
    registry_->AddBlocklisted(extension);
  } else if (ShouldBlockExtension(extension.get())) {
    DCHECK(!Manifest::IsComponentLocation(extension->location()));
    registry_->AddBlocked(extension);
  } else if (extension_prefs_->IsExtensionDisabled(extension->id())) {
    registry_->AddDisabled(extension);
  } else {  // Extension should be enabled.
    registry_->AddEnabled(extension);
    ActivateExtension(extension.get(), true);
  }
}

void ExtensionRegistrar::AddNewOrUpdatedExtension(
    const Extension* extension,
    const base::flat_set<int>& disable_reasons,
    int install_flags,
    const syncer::StringOrdinal& page_ordinal,
    const std::string& install_parameter,
    base::Value::Dict ruleset_install_prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  extension_prefs_->OnExtensionInstalled(
      extension, disable_reasons, page_ordinal, install_flags,
      install_parameter, std::move(ruleset_install_prefs));

  delayed_install_manager_->Remove(extension->id());

  delegate_->OnAddNewOrUpdatedExtension(extension);

  FinishInstallation(extension);
}

void ExtensionRegistrar::OnExtensionInstalled(
    const Extension* extension,
    const syncer::StringOrdinal& page_ordinal,
    int install_flags,
    base::Value::Dict ruleset_install_prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_->OnExtensionInstalled(extension, page_ordinal, install_flags,
                                  std::move(ruleset_install_prefs));
}

void ExtensionRegistrar::RemoveExtension(const ExtensionId& extension_id,
                                         UnloadedExtensionReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int include_mask = ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
                     ExtensionRegistry::TERMINATED;
  scoped_refptr<const Extension> extension(
      registry_->GetExtensionById(extension_id, include_mask));

  // If the extension is blocked/blocklisted, no need to notify again.
  if (!extension) {
    return;
  }

  if (registry_->terminated_extensions().Contains(extension_id)) {
    // The extension was already deactivated from the call to
    // TerminateExtension(), which also should have added it to
    // unloaded_extension_paths_ if necessary.
    registry_->RemoveTerminated(extension->id());
    return;
  }

  // Keep information about the extension so that we can reload it later
  // even if it's not permanently installed.
  unloaded_extension_paths_[extension->id()] = extension->path();

  // Stop tracking whether the extension was meant to be enabled after a reload.
  reloading_extensions_.erase(extension->id());

  if (registry_->enabled_extensions().Contains(extension_id)) {
    // Put the pending removal extension in disabled set because underlying
    // code of `DeactivateExtension` needs to access it.
    // See https://crbug.com/443038597
    registry_->AddDisabled(extension);

    registry_->RemoveEnabled(extension_id);
    DeactivateExtension(extension.get(), reason);

    registry_->RemoveDisabled(extension_id);
  } else {
    // The extension was already deactivated from the call to
    // DisableExtension().
    bool removed = registry_->RemoveDisabled(extension->id());
    DCHECK(removed);
  }
}

void ExtensionRegistrar::EnableExtension(const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the extension is currently reloading, it will be enabled once the reload
  // is complete.
  if (reloading_extensions_.count(extension_id) > 0)
    return;

  // First, check that the extension can be enabled.
  if (IsExtensionEnabled(extension_id) ||
      blocklist_prefs::IsExtensionBlocklisted(extension_id, extension_prefs_) ||
      registry_->blocked_extensions().Contains(extension_id)) {
    return;
  }

  const Extension* extension =
      registry_->disabled_extensions().GetByID(extension_id);
  if (extension && !delegate_->CanEnableExtension(extension))
    return;

  // Now that we know the extension can be enabled, update the prefs.
  extension_prefs_->ClearDisableReasons(extension_id);

  // This can happen if sync enables an extension that is not installed yet.
  if (!extension)
    return;

  // Actually enable the extension.
  registry_->AddEnabled(extension);
  registry_->RemoveDisabled(extension->id());
  ActivateExtension(extension, false);
}

void ExtensionRegistrar::DisableExtension(
    const ExtensionId& extension_id,
    const DisableReasonSet& disable_reasons) {
  auto passkey = ExtensionPrefs::DisableReasonRawManipulationPasskey();
  DisableExtensionWithRawReasons(passkey, extension_id,
                                 DisableReasonSetToIntegerSet(disable_reasons));
}

void ExtensionRegistrar::DisableExtensionWithRawReasons(
    ExtensionPrefs::DisableReasonRawManipulationPasskey,
    const ExtensionId& extension_id,
    base::flat_set<int> disable_reasons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!disable_reasons.empty());

  scoped_refptr<const Extension> extension =
      registry_->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

  CHECK(delegate_);
  bool is_controlled_extension =
      !delegate_->CanDisableExtension(extension.get());

  if (is_controlled_extension) {
    // Remove disallowed disable reasons.
    // Certain disable reasons are always allowed, since they are more internal
    // to the browser (rather than the user choosing to disable the extension).
    base::flat_set<int> internal_disable_reasons = {
        extensions::disable_reason::DISABLE_RELOAD,
        extensions::disable_reason::DISABLE_CORRUPTED,
        extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY,
        extensions::disable_reason::
            DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY,
        extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY,
        extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED,
        extensions::disable_reason::DISABLE_REINSTALL,
        extensions::disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION,
        extensions::disable_reason::DISABLE_NOT_VERIFIED,
        extensions::disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION,
    };

    disable_reasons = base::STLSetIntersection<base::flat_set<int>>(
        disable_reasons, internal_disable_reasons);

    if (disable_reasons.empty()) {
      return;
    }
  }

  auto passkey = ExtensionPrefs::DisableReasonRawManipulationPasskey();

  // The extension may have been disabled already. Just add the disable reasons.
  if (!IsExtensionEnabled(extension_id)) {
    extension_prefs_->AddRawDisableReasons(passkey, extension_id,
                                           disable_reasons);
    return;
  }

  extension_prefs_->ReplaceRawDisableReasons(passkey, extension_id,
                                             disable_reasons);

  int include_mask =
      ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::DISABLED;
  extension = registry_->GetExtensionById(extension_id, include_mask);
  if (!extension)
    return;

  // The extension is either enabled or terminated.
  DCHECK(registry_->enabled_extensions().Contains(extension->id()) ||
         registry_->terminated_extensions().Contains(extension->id()));

  // Move the extension to the disabled list.
  registry_->AddDisabled(extension);
  if (registry_->enabled_extensions().Contains(extension->id())) {
    registry_->RemoveEnabled(extension->id());
    DeactivateExtension(extension.get(), UnloadedExtensionReason::DISABLE);
  } else {
    // The extension must have been terminated. Don't send additional
    // notifications for it being disabled.
    bool removed = registry_->RemoveTerminated(extension->id());
    DCHECK(removed);
  }
}

void ExtensionRegistrar::DisableExtensionWithSource(
    const Extension* source_extension,
    const ExtensionId& extension_id,
    disable_reason::DisableReason disable_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(disable_reason == disable_reason::DISABLE_USER_ACTION ||
         disable_reason == disable_reason::DISABLE_BLOCKED_BY_POLICY);
  if (disable_reason == disable_reason::DISABLE_BLOCKED_BY_POLICY) {
    DCHECK(Manifest::IsPolicyLocation(source_extension->location()) ||
           Manifest::IsComponentLocation(source_extension->location()));
  }

  const Extension* extension =
      registry_->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  CHECK(extension_system_->management_policy()->ExtensionMayModifySettings(
      source_extension, extension, nullptr));
  DisableExtension(extension_id, {disable_reason});
}

void ExtensionRegistrar::EnabledReloadableExtensions() {
  std::vector<std::string> extensions_to_enable;
  for (const auto& e : registry_->disabled_extensions()) {
    if (extension_prefs_->HasOnlyDisableReason(
            e->id(), disable_reason::DISABLE_RELOAD)) {
      extensions_to_enable.push_back(e->id());
    }
  }
  for (const std::string& extension : extensions_to_enable) {
    EnableExtension(extension);
  }
}

base::flat_set<int> ExtensionRegistrar::GetDisableReasonsOnInstalled(
    const Extension* extension) {
  bool is_update_from_same_type = false;
  {
    const Extension* existing_extension =
        registry_->GetInstalledExtension(extension->id());
    is_update_from_same_type =
        existing_extension &&
        existing_extension->manifest()->type() == extension->manifest()->type();
  }
  disable_reason::DisableReason disable_reason = disable_reason::DISABLE_NONE;
  // Extensions disabled by management policy should always be disabled, even
  // if it's force-installed.
  if (extension_system_->management_policy()->MustRemainDisabled(
          extension, &disable_reason)) {
    // A specified reason is required to disable the extension.
    DCHECK(disable_reason != disable_reason::DISABLE_NONE);
    return {disable_reason};
  }

  // Extensions installed by policy can't be disabled. So even if a previous
  // installation disabled the extension, make sure it is now enabled.
  if (extension_system_->management_policy()->MustRemainEnabled(extension,
                                                                nullptr)) {
    return {};
  }

  // An already disabled extension should inherit the disable reasons and
  // remain disabled. We must get the raw reasons to retain unknown reasons.
  if (extension_prefs_->IsExtensionDisabled(extension->id())) {
    auto passkey = ExtensionPrefs::DisableReasonRawManipulationPasskey();
    base::flat_set<int> disable_reasons =
        extension_prefs_->GetRawDisableReasons(passkey, extension->id());
    // If an extension was disabled without specified reason, presume it's
    // disabled by user.
    return disable_reasons.empty()
               ? base::flat_set<int>({disable_reason::DISABLE_USER_ACTION})
               : disable_reasons;
  }

  if (util::IsPromptingEnabled()) {
    // External extensions are initially disabled. We prompt the user before
    // enabling them. Hosted apps are excepted because they are not dangerous
    // (they need to be launched by the user anyway). We also don't prompt for
    // extensions updating; this is because the extension will be disabled from
    // the initial install if it is supposed to be, and this allows us to turn
    // this on for other platforms without disabling already-installed
    // extensions.
    if (extension->GetType() != Manifest::Type::kHostedApp &&
        Manifest::IsExternalLocation(extension->location()) &&
        !extension_prefs_->IsExternalExtensionAcknowledged(extension->id()) &&
        !is_update_from_same_type) {
      return {disable_reason::DISABLE_EXTERNAL_EXTENSION};
    }
  }

  return {};
}

void ExtensionRegistrar::AddComponentExtension(const Extension* extension) {
  extension_prefs_->ClearInapplicableDisableReasonsForComponentExtension(
      extension->id());
  const std::string old_version_string(
      extension_prefs_->GetVersionString(extension->id()));
  const base::Version old_version(old_version_string);

  VLOG(1) << "AddComponentExtension " << extension->name();
  if (!old_version.IsValid() || old_version != extension->version()) {
    VLOG(1) << "Component extension " << extension->name() << " ("
            << extension->id() << ") installing/upgrading from '"
            << old_version_string << "' to "
            << extension->version().GetString();

    // If there was a previous installation, we need to clear the extension
    // service worker. This is a workaround to ensure component extension
    // updates are applied. See crbug.com/425464855.
    if (old_version.IsValid()) {
      UnregisterServiceWorkerWithRootScope(extension);
    }
    // TODO(crbug.com/40508457): If needed, add support for Declarative Net
    // Request to component extensions and pass the ruleset install prefs here.
    AddNewOrUpdatedExtension(extension, {}, kInstallFlagNone,
                             syncer::StringOrdinal(), std::string(),
                             /*ruleset_install_prefs=*/{});
    return;
  }

  AddExtension(extension);
}

void ExtensionRegistrar::RemoveComponentExtension(
    const std::string& extension_id) {
  scoped_refptr<const Extension> extension(
      registry_->enabled_extensions().GetByID(extension_id));
  RemoveExtension(extension_id, UnloadedExtensionReason::UNINSTALL);
  if (extension.get()) {
    registry_->TriggerOnUninstalled(extension.get(),
                                    UNINSTALL_REASON_COMPONENT_REMOVED);
  }
}

void ExtensionRegistrar::RemoveDisableReasonAndMaybeEnable(
    const std::string& extension_id,
    disable_reason::DisableReason reason_to_remove) {
  DisableReasonSet disable_reasons =
      extension_prefs_->GetDisableReasons(extension_id);
  if (!disable_reasons.contains(reason_to_remove)) {
    return;
  }

  extension_prefs_->RemoveDisableReason(extension_id, reason_to_remove);
  if (disable_reasons.size() == 1) {
    EnableExtension(extension_id);
  }
}

namespace {
std::vector<scoped_refptr<DevToolsAgentHost>> GetDevToolsAgentHostsFor(
    ProcessManager* process_manager,
    const Extension* extension) {
  std::vector<scoped_refptr<DevToolsAgentHost>> result;
  if (!BackgroundInfo::IsServiceWorkerBased(extension)) {
    ExtensionHost* host =
        process_manager->GetBackgroundHostForExtension(extension->id());
    if (host) {
      content::WebContents* const wc = host->host_contents();
      if (auto tab_host = content::DevToolsAgentHost::GetForTab(wc)) {
        result.push_back(tab_host);
      }
      if (content::DevToolsAgentHost::HasFor(wc)) {
        result.push_back(content::DevToolsAgentHost::GetOrCreateFor(wc));
      }
    }
  } else {
    content::ServiceWorkerContext* context =
        util::GetServiceWorkerContextForExtensionId(
            extension->id(), process_manager->browser_context());
    std::vector<WorkerId> service_worker_ids =
        process_manager->GetServiceWorkersForExtension(extension->id());
    for (const auto& worker_id : service_worker_ids) {
      auto devtools_host =
          DevToolsAgentHost::GetForServiceWorker(context, worker_id.version_id);
      if (devtools_host)
        result.push_back(std::move(devtools_host));
    }
  }
  return result;
}
}  // namespace

void ExtensionRegistrar::ReloadExtension(const ExtensionId& extension_id) {
  DoReloadExtension(extension_id, LoadErrorBehavior::kNoisy);
}

void ExtensionRegistrar::ReloadExtensionWithQuietFailure(
    const ExtensionId& extension_id) {
  DoReloadExtension(extension_id, LoadErrorBehavior::kQuiet);
}

bool ExtensionRegistrar::UninstallExtension(
    // "transient" because the process of uninstalling may cause the reference
    // to become invalid. Instead, use |extension->id()|.
    const std::string& transient_extension_id,
    UninstallReason reason,
    std::u16string* error,
    base::OnceClosure done_callback) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_refptr<const Extension> extension =
      registry_->GetInstalledExtension(transient_extension_id);

  // Callers should not send us nonexistent extensions.
  CHECK(extension.get());

  ManagementPolicy* by_policy = extension_system_->management_policy();
  // Policy change which triggers an uninstall will always set
  // |external_uninstall| to true so this is the only way to uninstall
  // managed extensions.
  // Shared modules being uninstalled will also set |external_uninstall| to true
  // so that we can guarantee users don't uninstall a shared module.
  // (crbug.com/273300)
  // TODO(rdevlin.cronin): This is probably not right. We should do something
  // else, like include an enum IS_INTERNAL_UNINSTALL or IS_USER_UNINSTALL so
  // we don't do this.
  bool external_uninstall =
      (reason == UNINSTALL_REASON_INTERNAL_MANAGEMENT) ||
      (reason == UNINSTALL_REASON_COMPONENT_REMOVED) ||
      (reason == UNINSTALL_REASON_MIGRATED) ||
      (reason == UNINSTALL_REASON_REINSTALL) ||
      (reason == UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION) ||
      (reason == UNINSTALL_REASON_ORPHANED_SHARED_MODULE);
  if (!external_uninstall &&
      (!by_policy->UserMayModifySettings(extension.get(), error) ||
       by_policy->MustRemainInstalled(extension.get(), error))) {
    registry_->TriggerOnUninstallationDenied(extension.get());
    return false;
  }

  // Prepare to uninstall the extension.
  delegate_->PreUninstallExtension(extension.get());

  UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallType", extension->GetType(),
                            100);

  // Unload before doing more cleanup to ensure that nothing is hanging on to
  // any of these resources.
  RemoveExtension(extension->id(), UnloadedExtensionReason::UNINSTALL);

  // `UnloadExtension` ignores extensions that are `BLOCKLISTED` or `BLOCKED`
  if (registry_->blocklisted_extensions().Contains(extension->id())) {
    registry_->RemoveBlocklisted(extension->id());
  }
  if (registry_->blocked_extensions().Contains(extension->id())) {
    registry_->RemoveBlocked(extension->id());
  }

  // Perform the necessary clean up after the extension is un-registered.
  delegate_->PostUninstallExtension(extension, std::move(done_callback));

  UntrackTerminatedExtension(extension->id());

  // Notify interested parties that we've uninstalled this extension.
  registry_->TriggerOnUninstalled(extension.get(), reason);

  // Perform the necessary clean up work after extension un-installation event
  // has been notified to all observers.
  delayed_install_manager_->Remove(extension->id());

  extension_prefs_->OnExtensionUninstalled(
      extension->id(), extension->location(), external_uninstall);

  return true;
}

void ExtensionRegistrar::UninstallMigratedExtensions(
    base::span<const char* const> migrated_ids) {
  const ExtensionSet installed_extensions =
      registry_->GenerateInstalledExtensionsSet();
  for (const auto* extension_id : migrated_ids) {
    auto* extension = installed_extensions.GetByID(extension_id);
    if (extension) {
      UninstallExtension(extension_id, UNINSTALL_REASON_COMPONENT_REMOVED,
                         nullptr);
      extension_prefs_->MarkObsoleteComponentExtensionAsRemoved(
          extension->id(), extension->location());
    }
  }
}

void ExtensionRegistrar::FinishInstallation(const Extension* extension) {
  const Extension* existing_extension =
      registry_->GetInstalledExtension(extension->id());
  bool is_update = false;
  std::string old_name;
  if (existing_extension) {
    is_update = true;
    old_name = existing_extension->name();
  }
  registry_->TriggerOnWillBeInstalled(extension, is_update, old_name);

  // Unpacked extensions default to allowing file access, but if that has been
  // overridden, don't reset the value.
  if (Manifest::ShouldAlwaysAllowFileAccess(extension->location()) &&
      !extension_prefs_->HasAllowFileAccessSetting(extension->id())) {
    extension_prefs_->SetAllowFileAccess(extension->id(), true);
  }

  AddExtension(extension);

  // Notify observers that need to know when an installation is complete.
  registry_->TriggerOnInstalled(extension, is_update);

  // Check extensions that may have been delayed only because this shared module
  // was not available.
  if (SharedModuleInfo::IsSharedModule(extension)) {
    delayed_install_manager_->MaybeFinishDelayedInstallations();
  }
}

bool ExtensionRegistrar::CanBlockExtension(const Extension* extension) const {
  DCHECK(extension);
  return extension->location() != ManifestLocation::kComponent &&
         extension->location() != ManifestLocation::kExternalComponent &&
         !extension_system_->management_policy()->MustRemainEnabled(extension,
                                                                    nullptr);
}

// Extensions that are not locked, components or forced by policy should be
// locked. Extensions are no longer considered enabled or disabled. Blocklisted
// extensions are now considered both blocklisted and locked.
void ExtensionRegistrar::BlockAllExtensions() {
  if (block_extensions_) {
    return;
  }
  block_extensions_ = true;

  // Blocklisted extensions are already unloaded, need not be blocked.
  const ExtensionSet extensions = registry_->GenerateInstalledExtensionsSet(
      ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
      ExtensionRegistry::TERMINATED);

  for (const auto& extension : extensions) {
    const std::string& id = extension->id();

    if (!CanBlockExtension(extension.get())) {
      continue;
    }

    registry_->AddBlocked(extension.get());
    RemoveExtension(id, UnloadedExtensionReason::LOCK_ALL);
  }
}

// All locked extensions should revert to being either enabled or disabled
// as appropriate.
void ExtensionRegistrar::UnblockAllExtensions() {
  if (!block_extensions_) {
    return;
  }

  block_extensions_ = false;

  const ExtensionSet to_unblock =
      registry_->GenerateInstalledExtensionsSet(ExtensionRegistry::BLOCKED);

  for (const auto& extension : to_unblock) {
    registry_->RemoveBlocked(extension->id());
    AddExtension(extension.get());
  }

  // While extensions are blocked, we won't display any external install
  // warnings. Now that they are unblocked, we should update the error.
  delegate_->UpdateExternalExtensionAlert();
}

void ExtensionRegistrar::OnBlocklistStateRemoved(
    const std::string& extension_id) {
  if (blocklist_prefs::IsExtensionBlocklisted(extension_id, extension_prefs_)) {
    return;
  }

  // Clear acknowledged state.
  blocklist_prefs::RemoveAcknowledgedBlocklistState(
      extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs_);

  scoped_refptr<const Extension> extension =
      registry_->blocklisted_extensions().GetByID(extension_id);
  DCHECK(extension);
  registry_->RemoveBlocklisted(extension_id);
  AddExtension(extension.get());
}

void ExtensionRegistrar::OnBlocklistStateAdded(
    const std::string& extension_id) {
  DCHECK(
      blocklist_prefs::IsExtensionBlocklisted(extension_id, extension_prefs_));
  // The extension was already acknowledged by the user, it should already be in
  // the unloaded state.
  if (blocklist_prefs::HasAcknowledgedBlocklistState(
          extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
          extension_prefs_)) {
    DCHECK(base::Contains(registry_->blocklisted_extensions().GetIDs(),
                          extension_id));
    return;
  }

  scoped_refptr<const Extension> extension =
      registry_->GetInstalledExtension(extension_id);
  registry_->AddBlocklisted(extension);
  RemoveExtension(extension_id, UnloadedExtensionReason::BLOCKLIST);
}

void ExtensionRegistrar::OnGreylistStateRemoved(
    const std::string& extension_id) {
  bool is_on_sb_list = (blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
                            extension_id, extension_prefs_) !=
                        BitMapBlocklistState::NOT_BLOCKLISTED);
  bool is_on_omaha_list =
      blocklist_prefs::HasAnyOmahaGreylistState(extension_id, extension_prefs_);
  if (is_on_sb_list || is_on_omaha_list) {
    return;
  }
  // Clear all acknowledged states so the extension will still get disabled if
  // it is added to the greylist again.
  blocklist_prefs::ClearAcknowledgedGreylistStates(extension_id,
                                                   extension_prefs_);
  RemoveDisableReasonAndMaybeEnable(extension_id,
                                    disable_reason::DISABLE_GREYLIST);

  // A user can enable and disable a force-installed extension while it is
  // greylisted. If a user disables an extension while greylisted, the
  // extension gets a DISABLE_USER_ACTION disable reason assigned to it. So
  // remove the DISABLE_USER_ACTION disable reason as well when a
  // force-installed extension gets "un-greylisted" to allow the extension
  // to be re-enabled.
  const Extension* extension = registry_->GetInstalledExtension(extension_id);
  if (extension && extension_system_->management_policy()->MustRemainEnabled(
                       extension, nullptr)) {
    RemoveDisableReasonAndMaybeEnable(extension_id,
                                      disable_reason::DISABLE_USER_ACTION);
  }
}

void ExtensionRegistrar::OnGreylistStateAdded(const std::string& extension_id,
                                              BitMapBlocklistState new_state) {
#if DCHECK_IS_ON()
  bool has_new_state_on_sb_list =
      (blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
           extension_id, extension_prefs_) == new_state);
  bool has_new_state_on_omaha_list = blocklist_prefs::HasOmahaBlocklistState(
      extension_id, new_state, extension_prefs_);
  DCHECK(has_new_state_on_sb_list || has_new_state_on_omaha_list);
#endif
  if (blocklist_prefs::HasAcknowledgedBlocklistState(extension_id, new_state,
                                                     extension_prefs_)) {
    // If the extension is already acknowledged, don't disable it again
    // because it can be already re-enabled by the user. This could happen if
    // the extension is added to the SafeBrowsing blocklist, and then
    // subsequently marked by Omaha. In this case, we don't want to disable the
    // extension twice.
    return;
  }

  // Set the current greylist states to acknowledge immediately because the
  // extension is disabled silently. Clear the other acknowledged state because
  // when the state changes to another greylist state in the future, we'd like
  // to disable the extension again.
  blocklist_prefs::UpdateCurrentGreylistStatesAsAcknowledged(extension_id,
                                                             extension_prefs_);
  DisableExtension(extension_id, {disable_reason::DISABLE_GREYLIST});
}

void ExtensionRegistrar::BlocklistExtensionForTest(
    const std::string& extension_id) {
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs_);
  OnBlocklistStateAdded(extension_id);
}

void ExtensionRegistrar::GreylistExtensionForTest(
    const std::string& extension_id,
    const BitMapBlocklistState& state) {
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(extension_id, state,
                                                          extension_prefs_);
  if (state == BitMapBlocklistState::NOT_BLOCKLISTED) {
    OnGreylistStateRemoved(extension_id);
  } else {
    OnGreylistStateAdded(extension_id, state);
  }
}

// static
base::AutoReset<bool> ExtensionRegistrar::DisableLazyContextSpinupForTest() {
  CHECK_IS_TEST();
  return base::AutoReset<bool>(&g_disable_lazy_context_spinup_for_test, true);
}

void ExtensionRegistrar::OnUnpackedExtensionReloadFailed(
    const base::FilePath& path) {
  failed_to_reload_unpacked_extensions_.insert(path);
}

void ExtensionRegistrar::GrantPermissionsAndEnableExtension(
    const Extension& extension) {
  delegate_->GrantActivePermissions(&extension);
  EnableExtension(extension.id());
}

void ExtensionRegistrar::AddDisableFlagExemptedExtension(
    const ExtensionId& extension_id) {
  disable_flag_exempted_extensions_.insert(extension_id);
}

void ExtensionRegistrar::TerminateExtension(const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<const Extension> extension =
      registry_->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return;

  // Keep information about the extension so that we can reload it later
  // even if it's not permanently installed.
  unloaded_extension_paths_[extension->id()] = extension->path();

  DCHECK(!base::Contains(reloading_extensions_, extension->id()))
      << "Enabled extension shouldn't be marked for reloading";

  registry_->AddTerminated(extension);
  registry_->RemoveEnabled(extension_id);
  DeactivateExtension(extension.get(), UnloadedExtensionReason::TERMINATE);
}

void ExtensionRegistrar::UntrackTerminatedExtension(
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<const Extension> extension =
      registry_->terminated_extensions().GetByID(extension_id);
  if (!extension)
    return;

  registry_->RemoveTerminated(extension_id);
}

bool ExtensionRegistrar::IsExtensionEnabled(
    const ExtensionId& extension_id) const {
  if (registry_->enabled_extensions().Contains(extension_id) ||
      registry_->terminated_extensions().Contains(extension_id)) {
    return true;
  }

  if (registry_->disabled_extensions().Contains(extension_id) ||
      registry_->blocklisted_extensions().Contains(extension_id) ||
      registry_->blocked_extensions().Contains(extension_id)) {
    return false;
  }

  if (ShouldBlockExtension(nullptr)) {
    return false;
  }

  // If the extension hasn't been loaded yet, check the prefs for it. Assume
  // enabled unless otherwise noted.
  return !extension_prefs_->IsExtensionDisabled(extension_id) &&
         !blocklist_prefs::IsExtensionBlocklisted(extension_id,
                                                  extension_prefs_) &&
         !extension_prefs_->IsExternalExtensionUninstalled(extension_id);
}

void ExtensionRegistrar::DidCreateMainFrameForBackgroundPage(
    ExtensionHost* host) {
  auto iter = orphaned_dev_tools_.find(host->extension_id());
  if (iter == orphaned_dev_tools_.end())
    return;
  // Keepalive count is reset on extension reload. This re-establishes the
  // keepalive that was added when the DevTools agent was initially attached.
  ProcessManager::Get(browser_context_)
      ->IncrementLazyKeepaliveCount(host->extension(), Activity::DEV_TOOLS,
                                    std::string());
  // TODO(caseq): do we need to handle the case when the extension changed
  // from SW-based to WC-based during reload?
  for (auto& dev_tools_host : iter->second) {
    dev_tools_host->ConnectWebContents(host->host_contents());
  }
  orphaned_dev_tools_.erase(iter);
}

void ExtensionRegistrar::ActivateExtension(const Extension* extension,
                                           bool is_newly_added) {
  // Activate the extension before calling
  // RendererStartupHelper::OnExtensionLoaded() below, so that we have
  // activation information ready while we send ExtensionMsg_Load IPC.
  //
  // TODO(lazyboy): We should move all logic that is required to start up an
  // extension to a separate class, instead of calling adhoc methods like
  // service worker ones below.
  ActivateTaskQueueForExtension(browser_context_, extension);

  renderer_helper_->OnExtensionLoaded(*extension);

  // Tell subsystems that use the ExtensionRegistryObserver::OnExtensionLoaded
  // about the new extension.
  //
  // NOTE: It is important that this happen after notifying the renderers about
  // the new extensions so that if we navigate to an extension URL in
  // ExtensionRegistryObserver::OnExtensionLoaded the renderer is guaranteed to
  // know about it.
  registry_->TriggerOnLoaded(extension);

  delegate_->PostActivateExtension(extension);

  // When an extension is activated, and it is either event page-based or
  // service worker-based, it may be necessary to spin up its context.
  if (BackgroundInfo::HasLazyContext(extension))
    MaybeSpinUpLazyContext(extension, is_newly_added);

  registry_->AddReady(extension);
  if (registry_->enabled_extensions().Contains(extension->id())) {
    registry_->TriggerOnReady(extension);
  }
}

void ExtensionRegistrar::DeactivateExtension(const Extension* extension,
                                             UnloadedExtensionReason reason) {
  // NOTE: Call `TriggerOnUnloaded` before `DeactivateTaskQueueForExtension`.
  // If an extension service worker is running, this stops it, which triggers a
  // synchronous notification. This notification updates the
  // `ServiceWorkerState` and untracks the worker from `ProcessManager`.
  // `ServiceWorkerTaskQueue` can then operate in a consistent state, safely
  // assuming the worker is no longer active.
  registry_->TriggerOnUnloaded(extension, reason);
  renderer_helper_->OnExtensionUnloaded(*extension);
  DeactivateTaskQueueForExtension(browser_context_, extension);

  delegate_->PostDeactivateExtension(extension);
}

void ExtensionRegistrar::DoReloadExtension(
    ExtensionId extension_id,
    LoadErrorBehavior load_error_behavior) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath path;

  const Extension* disabled_extension =
      registry_->disabled_extensions().GetByID(extension_id);

  if (disabled_extension) {
    path = disabled_extension->path();
  }

  // If the extension is already reloading, don't reload again.
  if (extension_prefs_->HasDisableReason(extension_id,
                                         disable_reason::DISABLE_RELOAD)) {
    DCHECK(disabled_extension);
    // If an unpacked extension previously failed to reload, it will still be
    // marked as disabled, but we can try to reload it again - the developer
    // may have fixed the issue.
    if (failed_to_reload_unpacked_extensions_.count(path) == 0) {
      return;
    }
    failed_to_reload_unpacked_extensions_.erase(path);
  }
  // Ignore attempts to reload a blocklisted or blocked extension. Sometimes
  // this can happen in a convoluted reload sequence triggered by the
  // termination of a blocklisted or blocked extension and a naive attempt to
  // reload it. For an example see http://crbug.com/373842.
  if (registry_->blocklisted_extensions().Contains(extension_id) ||
      registry_->blocked_extensions().Contains(extension_id)) {
    return;
  }

  const Extension* enabled_extension =
      registry_->enabled_extensions().GetByID(extension_id);

  // Disable the extension if it's loaded. It might not be loaded if it crashed.
  if (enabled_extension) {
    // If the extension has an inspector open for its background page, detach
    // the inspector and hang onto a cookie for it, so that we can reattach
    // later.
    // TODO(yoz): this is not incognito-safe!
    ProcessManager* manager = ProcessManager::Get(browser_context_);
    auto agent_hosts = GetDevToolsAgentHostsFor(manager, enabled_extension);
    if (!agent_hosts.empty()) {
      for (auto& host : agent_hosts) {
        // Let DevTools know we'll be back once extension is reloaded.
        host->DisconnectWebContents();
      }
      // Retain DevToolsAgentHosts for the extension being reloaded to prevent
      // client disconnecting. We will re-attach later, when the extension is
      // loaded.
      // TODO(crbug.com/40196582): clean up upon failure to reload.
      orphaned_dev_tools_[extension_id] = std::move(agent_hosts);
    }
    path = enabled_extension->path();
    DisableExtension(extension_id, {disable_reason::DISABLE_RELOAD});
    DCHECK(registry_->disabled_extensions().Contains(extension_id));
    reloading_extensions_.insert(extension_id);
  } else if (!disabled_extension) {
    std::map<ExtensionId, base::FilePath>::const_iterator iter =
        unloaded_extension_paths_.find(extension_id);
    if (iter == unloaded_extension_paths_.end()) {
      return;
    }
    path = unloaded_extension_paths_[extension_id];
  }

  if (delayed_install_manager_->Contains(extension_id) &&
      delayed_install_manager_->FinishDelayedInstallationIfReady(
          extension_id, true /*install_immediately*/)) {
    return;
  }

  if (load_error_behavior == LoadErrorBehavior::kQuiet) {
    delegate_->LoadExtensionForReloadWithQuietFailure(extension_id, path);
  } else {
    delegate_->LoadExtensionForReload(extension_id, path);
  }
}

void ExtensionRegistrar::UnregisterServiceWorkerWithRootScope(
    const Extension* new_extension) {
  // Only cleanup the old service worker if the new extension is
  // service-worker-based.
  if (!BackgroundInfo::IsServiceWorkerBased(new_extension)) {
    return;
  }

  // Non service-worker based extensions could register root-scope service
  // workers using regular web APIs. These service workers are not tracked by
  // extension ServiceWorkerTaskQueue and would prevent newer service worker
  // version from installing (crbug/1340341).
  content::ServiceWorkerContext* context =
      util::GetServiceWorkerContextForExtensionId(new_extension->id(),
                                                  browser_context_);
  bool worker_previously_registered =
      ServiceWorkerTaskQueue::Get(browser_context_)
          ->IsWorkerRegistered(new_extension->id());
  // Even though the unregistration process for a service worker is
  // asynchronous, we begin the process before the new extension is added, so
  // the old worker will be unregistered before the new one is registered.
  if (base::FeatureList::IsEnabled(
          kExtensionUpdatesImmediatelyUnregisterWorker)) {
    context->UnregisterServiceWorkerImmediately(
        new_extension->url(),
        blink::StorageKey::CreateFirstParty(new_extension->origin()),
        base::BindOnce(&ExtensionRegistrar::NotifyServiceWorkerUnregistered,
                       weak_factory_.GetWeakPtr(), new_extension->id(),
                       worker_previously_registered));
  } else {
    context->UnregisterServiceWorker(
        new_extension->url(),
        blink::StorageKey::CreateFirstParty(new_extension->origin()),
        base::BindOnce(&ExtensionRegistrar::NotifyServiceWorkerUnregistered,
                       weak_factory_.GetWeakPtr(), new_extension->id(),
                       worker_previously_registered));
  }
}

void ExtensionRegistrar::NotifyServiceWorkerUnregistered(
    const ExtensionId& extension_id,
    bool worker_previously_registered,
    blink::ServiceWorkerStatusCode status) {
  bool success =
      ServiceWorkerTaskQueue::Get(browser_context_)
          ->IsWorkerUnregistrationSuccess(status, worker_previously_registered);
  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState", success);
  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "AddExtension",
      success);

  if (!success) {
    // TODO(crbug.com/346732739): Handle this case.
    LOG(ERROR) << "Failed to unregister service worker for extension "
               << extension_id;
    base::UmaHistogramEnumeration(
        "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus",
        status);
    base::UmaHistogramEnumeration(
        "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus_"
        "AddExtension",
        status);
  }
}

bool ExtensionRegistrar::ReplaceReloadedExtension(
    scoped_refptr<const Extension> extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The extension must already be disabled, and the original extension has
  // been unloaded.
  CHECK(registry_->disabled_extensions().Contains(extension->id()));
  if (!delegate_->CanEnableExtension(extension.get()))
    return false;

  // TODO(michaelpg): Other disable reasons might have been added after the
  // reload started. We may want to keep the extension disabled and just remove
  // the DISABLE_RELOAD reason in that case.
  extension_prefs_->ClearDisableReasons(extension->id());

  // Move it over to the enabled list.
  CHECK(registry_->RemoveDisabled(extension->id()));
  CHECK(registry_->AddEnabled(extension));

  ActivateExtension(extension.get(), false);

  return true;
}

void ExtensionRegistrar::MaybeSpinUpLazyContext(const Extension* extension,
                                                bool is_newly_added) {
  DCHECK(BackgroundInfo::HasLazyContext(extension));

  if (g_disable_lazy_context_spinup_for_test) {
    return;
  }

  // For orphaned devtools, we will reconnect devtools to it later in
  // DidCreateMainFrameForBackgroundPage().
  bool has_orphaned_dev_tools =
      base::Contains(orphaned_dev_tools_, extension->id());

  // Reloading component extension does not trigger install, so RuntimeAPI won't
  // be able to detect its loading. Therefore, we need to spin up its lazy
  // background page.
  bool is_component_extension =
      Manifest::IsComponentLocation(extension->location());

  // TODO(crbug.com/40107353): This is either a workaround or something
  // that will be part of the permanent solution for service worker-
  // based extensions.
  // We spin up extensions with the webRequest permission so their
  // listeners are reconstructed on load.
  // Event page-based extension cannot have the webRequest permission, but
  // a bug allowed them to specify it in optional permissions, so filter
  // out those extensions. See crbug.com/40912377.
  bool needs_spinup_for_web_request =
      extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kWebRequest) &&
      BackgroundInfo::IsServiceWorkerBased(extension);

  // If there aren't any special cases, we're done.
  if (!has_orphaned_dev_tools && !is_component_extension &&
      !needs_spinup_for_web_request) {
    return;
  }

  // If the extension's not being reloaded (|is_newly_added| = true),
  // only wake it up if it has the webRequest permission.
  if (is_newly_added && !needs_spinup_for_web_request)
    return;

  // Wake up the extension by posting a dummy task. In the case of a service
  // worker-based extension with the webRequest permission that's being newly
  // installed, this will result in a no-op task that's not necessary, since
  // this is really only needed for a previously-installed extension. However,
  // that cost is minimal, since the worker is already active.
  const auto context_id =
      LazyContextId::ForExtension(browser_context_, extension);
  context_id.GetTaskQueue()->AddPendingTask(context_id, base::DoNothing());
}

void ExtensionRegistrar::OnStartedTrackingServiceWorkerInstance(
    const WorkerId& worker_id) {
  // Just release the host. We only get here when the new worker has been
  // attached and resumed by the DevTools, and all we need in case of service
  // worker-based extensions is to keep the host around for the target
  // auto-attacher to do its job.
  orphaned_dev_tools_.erase(worker_id.extension_id);
}

bool ExtensionRegistrar::CanAddExtension(const Extension* extension) const {
  // TODO(jstritar): We may be able to get rid of this branch by overriding the
  // default extension state to DISABLED when the --disable-extensions flag
  // is set (http://crbug.com/29067).
  if (!extensions_enabled() &&
      !Manifest::ShouldAlwaysLoadExtension(extension->location(),
                                           extension->is_theme()) &&
      disable_flag_exempted_extensions_.count(extension->id()) == 0) {
    return false;
  }
  return true;
}

bool ExtensionRegistrar::ShouldBlockExtension(
    const Extension* extension) const {
  if (!block_extensions_) {
    return false;
  }

  // Blocked extensions aren't marked as such in prefs, thus if
  // `block_extensions_` is true then CanBlockExtension() must be called with an
  // Extension object. If `extension` is not loaded, assume it should be
  // blocked.
  return !extension || CanBlockExtension(extension);
}

}  // namespace extensions

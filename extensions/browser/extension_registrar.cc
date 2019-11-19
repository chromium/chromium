// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registrar.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/browser/task_queue_util.h"
#include "extensions/common/manifest_handlers/background_info.h"

using content::DevToolsAgentHost;

namespace extensions {

ExtensionRegistrar::ExtensionRegistrar(content::BrowserContext* browser_context,
                                       Delegate* delegate)
    : browser_context_(browser_context),
      delegate_(delegate),
      extension_system_(ExtensionSystem::Get(browser_context)),
      extension_prefs_(ExtensionPrefs::Get(browser_context)),
      registry_(ExtensionRegistry::Get(browser_context)),
      renderer_helper_(
          RendererStartupHelperFactory::GetForBrowserContext(browser_context)) {
}

ExtensionRegistrar::~ExtensionRegistrar() = default;

void ExtensionRegistrar::AddExtension(
    scoped_refptr<const Extension> extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool is_extension_upgrade = false;
  bool is_extension_loaded = false;
  const Extension* old = registry_->GetInstalledExtension(extension->id());
  if (old) {
    is_extension_loaded = true;
    int version_compare_result = extension->version().CompareTo(old->version());
    is_extension_upgrade = version_compare_result > 0;
    // Other than for unpacked extensions, we should not be downgrading.
    if (!Manifest::IsUnpackedLocation(extension->location()) &&
        version_compare_result < 0) {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.AttemptedToDowngradeVersionLocation",
          extension->location(), Manifest::NUM_LOCATIONS);
      UMA_HISTOGRAM_ENUMERATION("Extensions.AttemptedToDowngradeVersionType",
                                extension->GetType(), Manifest::NUM_LOAD_TYPES);

      // TODO(https://crbug.com/810799): It would be awfully nice to CHECK this,
      // but that's caused problems. There are apparently times when this
      // happens that we aren't accounting for. We should track those down and
      // fix them, but it can be tricky.
      NOTREACHED() << "Attempted to downgrade extension."
                   << "\nID: " << extension->id()
                   << "\nOld Version: " << old->version()
                   << "\nNew Version: " << extension->version()
                   << "\nLocation: " << extension->location();
      return;
    }
  }

  // If the extension was disabled for a reload, we will enable it.
  bool was_reloading = reloading_extensions_.erase(extension->id()) > 0;

  // Set the upgraded bit; we consider reloads upgrades.
  extension_system_->runtime_data()->SetBeingUpgraded(
      extension->id(), is_extension_upgrade || was_reloading);

  // The extension is now loaded; remove its data from unloaded extension map.
  unloaded_extension_paths_.erase(extension->id());

  // If a terminated extension is loaded, remove it from the terminated list.
  UntrackTerminatedExtension(extension->id());

  // Notify the delegate we will add the extension.
  delegate_->PreAddExtension(extension.get(), old);

  if (was_reloading) {
    ReplaceReloadedExtension(extension);
  } else {
    if (is_extension_loaded) {
      // To upgrade an extension in place, remove the old one and then activate
      // the new one. ReloadExtension disables the extension, which is
      // sufficient.
      RemoveExtension(extension->id(), UnloadedExtensionReason::UPDATE);
    }
    AddNewExtension(extension);
  }

  extension_system_->runtime_data()->SetBeingUpgraded(extension->id(), false);
}

void ExtensionRegistrar::AddNewExtension(
    scoped_refptr<const Extension> extension) {
  if (extension_prefs_->IsExtensionBlacklisted(extension->id())) {
    DCHECK(!Manifest::IsComponentLocation(extension->location()));
    // Only prefs is checked for the blacklist. We rely on callers to check the
    // blacklist before calling into here, e.g. CrxInstaller checks before
    // installation then threads through the install and pending install flow
    // of this class, and ExtensionService checks when loading installed
    // extensions.
    registry_->AddBlacklisted(extension);
  } else if (delegate_->ShouldBlockExtension(extension.get())) {
    DCHECK(!Manifest::IsComponentLocation(extension->location()));
    registry_->AddBlocked(extension);
  } else if (extension_prefs_->IsExtensionDisabled(extension->id())) {
    registry_->AddDisabled(extension);
    // Notify that a disabled extension was added or updated.
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
        content::Source<content::BrowserContext>(browser_context_),
        content::Details<const Extension>(extension.get()));
  } else {  // Extension should be enabled.
    // All apps that are displayed in the launcher are ordered by their ordinals
    // so we must ensure they have valid ordinals.
    if (extension->RequiresSortOrdinal()) {
      AppSorting* app_sorting = extension_system_->app_sorting();
      app_sorting->SetExtensionVisible(extension->id(),
                                       extension->ShouldDisplayInNewTabPage());
      app_sorting->EnsureValidOrdinals(extension->id(),
                                       syncer::StringOrdinal());
    }
    registry_->AddEnabled(extension);
    ActivateExtension(extension.get(), true);
  }
}

void ExtensionRegistrar::RemoveExtension(const ExtensionId& extension_id,
                                         UnloadedExtensionReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int include_mask =
      ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::TERMINATED;
  scoped_refptr<const Extension> extension(
      registry_->GetExtensionById(extension_id, include_mask));

  // If the extension was already removed, just notify of the new unload reason.
  // TODO: It's unclear when this needs to be called given that it may be a
  // duplicate notification. See crbug.com/708230.
  if (!extension) {
    extension_system_->UnregisterExtensionWithRequestContexts(extension_id,
                                                              reason);
    return;
  }

  // Keep information about the extension so that we can reload it later
  // even if it's not permanently installed.
  unloaded_extension_paths_[extension->id()] = extension->path();

  // Stop tracking whether the extension was meant to be enabled after a reload.
  reloading_extensions_.erase(extension->id());

  if (registry_->disabled_extensions().Contains(extension_id)) {
    // The extension is already deactivated.
    registry_->RemoveDisabled(extension->id());
    extension_system_->UnregisterExtensionWithRequestContexts(extension_id,
                                                              reason);
  } else {
    // TODO(michaelpg): The extension may be blocked or blacklisted, in which
    // case it shouldn't need to be "deactivated". Determine whether the removal
    // notifications are necessary (crbug.com/708230).
    registry_->RemoveEnabled(extension_id);
    DeactivateExtension(extension.get(), reason);
  }

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_REMOVED,
      content::Source<content::BrowserContext>(browser_context_),
      content::Details<const Extension>(extension.get()));
}

void ExtensionRegistrar::EnableExtension(const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the extension is currently reloading, it will be enabled once the reload
  // is complete.
  if (reloading_extensions_.count(extension_id) > 0)
    return;

  // First, check that the extension can be enabled.
  if (IsExtensionEnabled(extension_id) ||
      extension_prefs_->IsExtensionBlacklisted(extension_id) ||
      registry_->blocked_extensions().Contains(extension_id)) {
    return;
  }

  const Extension* extension =
      registry_->disabled_extensions().GetByID(extension_id);
  if (extension && !delegate_->CanEnableExtension(extension))
    return;

  // Now that we know the extension can be enabled, update the prefs.
  extension_prefs_->SetExtensionEnabled(extension_id);

  // This can happen if sync enables an extension that is not installed yet.
  if (!extension)
    return;

  // Actually enable the extension.
  registry_->AddEnabled(extension);
  registry_->RemoveDisabled(extension->id());
  ActivateExtension(extension, false);
}

void ExtensionRegistrar::DisableExtension(const ExtensionId& extension_id,
                                          int disable_reasons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(disable_reason::DISABLE_NONE, disable_reasons);

  if (extension_prefs_->IsExtensionBlacklisted(extension_id))
    return;

  // The extension may have been disabled already. Just add the disable reasons.
  // TODO(michaelpg): Move this after the policy check, below, to ensure that
  // disable reasons disallowed by policy are not added here.
  if (!IsExtensionEnabled(extension_id)) {
    extension_prefs_->AddDisableReasons(extension_id, disable_reasons);
    return;
  }

  scoped_refptr<const Extension> extension =
      registry_->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

  bool is_controlled_extension =
      !delegate_->CanDisableExtension(extension.get());

  if (is_controlled_extension) {
    // Remove disallowed disable reasons.
    // Certain disable reasons are always allowed, since they are more internal
    // to the browser (rather than the user choosing to disable the extension).
    int internal_disable_reason_mask =
        extensions::disable_reason::DISABLE_RELOAD |
        extensions::disable_reason::DISABLE_CORRUPTED |
        extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY |
        extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY |
        extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED;
    disable_reasons &= internal_disable_reason_mask;

    if (disable_reasons == disable_reason::DISABLE_NONE)
      return;
  }

  extension_prefs_->SetExtensionDisabled(extension_id, disable_reasons);

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

void ExtensionRegistrar::ReloadExtension(
    const ExtensionId extension_id,  // Passed by value because reloading can
                                     // invalidate a reference to the ID.
    LoadErrorBehavior load_error_behavior) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the extension is already reloading, don't reload again.
  if (extension_prefs_->HasDisableReason(extension_id,
                                         disable_reason::DISABLE_RELOAD)) {
    return;
  }

  // Ignore attempts to reload a blacklisted or blocked extension. Sometimes
  // this can happen in a convoluted reload sequence triggered by the
  // termination of a blacklisted or blocked extension and a naive attempt to
  // reload it. For an example see http://crbug.com/373842.
  if (registry_->blacklisted_extensions().Contains(extension_id) ||
      registry_->blocked_extensions().Contains(extension_id)) {
    return;
  }

  base::FilePath path;

  const Extension* enabled_extension =
      registry_->enabled_extensions().GetByID(extension_id);

  // Disable the extension if it's loaded. It might not be loaded if it crashed.
  if (enabled_extension) {
    // If the extension has an inspector open for its background page, detach
    // the inspector and hang onto a cookie for it, so that we can reattach
    // later.
    // TODO(yoz): this is not incognito-safe!
    ProcessManager* manager = ProcessManager::Get(browser_context_);
    ExtensionHost* host = manager->GetBackgroundHostForExtension(extension_id);
    if (host && content::DevToolsAgentHost::HasFor(host->host_contents())) {
      // Look for an open inspector for the background page.
      scoped_refptr<content::DevToolsAgentHost> agent_host =
          content::DevToolsAgentHost::GetOrCreateFor(host->host_contents());
      agent_host->DisconnectWebContents();
      orphaned_dev_tools_[extension_id] = agent_host;
    }

    path = enabled_extension->path();
    // BeingUpgraded is set back to false when the extension is added.
    extension_system_->runtime_data()->SetBeingUpgraded(enabled_extension->id(),
                                                        true);
    DisableExtension(extension_id, disable_reason::DISABLE_RELOAD);
    DCHECK(registry_->disabled_extensions().Contains(extension_id));
    reloading_extensions_.insert(extension_id);
  } else {
    std::map<ExtensionId, base::FilePath>::const_iterator iter =
        unloaded_extension_paths_.find(extension_id);
    if (iter == unloaded_extension_paths_.end()) {
      return;
    }
    path = unloaded_extension_paths_[extension_id];
  }

  delegate_->LoadExtensionForReload(extension_id, path, load_error_behavior);
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

  // TODO(michaelpg): This notification was already sent when the extension was
  // unloaded as part of being terminated. But we send it again as observers
  // may be tracking the terminated extension. See crbug.com/708230.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_REMOVED,
      content::Source<content::BrowserContext>(browser_context_),
      content::Details<const Extension>(extension.get()));
}

bool ExtensionRegistrar::IsExtensionEnabled(
    const ExtensionId& extension_id) const {
  if (registry_->enabled_extensions().Contains(extension_id) ||
      registry_->terminated_extensions().Contains(extension_id)) {
    return true;
  }

  if (registry_->disabled_extensions().Contains(extension_id) ||
      registry_->blacklisted_extensions().Contains(extension_id) ||
      registry_->blocked_extensions().Contains(extension_id)) {
    return false;
  }

  if (delegate_->ShouldBlockExtension(nullptr))
    return false;

  // If the extension hasn't been loaded yet, check the prefs for it. Assume
  // enabled unless otherwise noted.
  return !extension_prefs_->IsExtensionDisabled(extension_id) &&
         !extension_prefs_->IsExtensionBlacklisted(extension_id) &&
         !extension_prefs_->IsExternalExtensionUninstalled(extension_id);
}

void ExtensionRegistrar::DidCreateRenderViewForBackgroundPage(
    ExtensionHost* host) {
  auto iter = orphaned_dev_tools_.find(host->extension_id());
  if (iter == orphaned_dev_tools_.end())
    return;
  // Keepalive count is reset on extension reload. This re-establishes the
  // keepalive that was added when the DevTools agent was initially attached.
  ProcessManager::Get(browser_context_)
      ->IncrementLazyKeepaliveCount(host->extension(), Activity::DEV_TOOLS,
                                    std::string());
  iter->second->ConnectWebContents(host->host_contents());
  orphaned_dev_tools_.erase(iter);
}

void ExtensionRegistrar::ActivateExtension(const Extension* extension,
                                           bool is_newly_added) {
  // The URLRequestContexts need to be first to know that the extension
  // was loaded. Otherwise a race can arise where a renderer that is created
  // for the extension may try to load an extension URL with an extension id
  // that the request context doesn't yet know about. The BrowserContext should
  // ensure its URLRequestContexts appropriately discover the loaded extension.
  extension_system_->RegisterExtensionWithRequestContexts(
      extension,
      base::Bind(&ExtensionRegistrar::OnExtensionRegisteredWithRequestContexts,
                 weak_factory_.GetWeakPtr(), WrapRefCounted(extension)));

  renderer_helper_->OnExtensionLoaded(*extension);

  // TODO(lazyboy): We should move all logic that is required to start up an
  // extension to a separate class, instead of calling adhoc methods like
  // service worker ones below.
  ActivateTaskQueueForExtension(browser_context_, extension);

  // Tell subsystems that use the ExtensionRegistryObserver::OnExtensionLoaded
  // about the new extension.
  //
  // NOTE: It is important that this happen after notifying the renderers about
  // the new extensions so that if we navigate to an extension URL in
  // ExtensionRegistryObserver::OnExtensionLoaded the renderer is guaranteed to
  // know about it.
  registry_->TriggerOnLoaded(extension);

  delegate_->PostActivateExtension(extension);

  // When an existing extension is re-enabled, it may be necessary to spin up
  // its lazy background page.
  if (!is_newly_added)
    MaybeSpinUpLazyBackgroundPage(extension);
}

void ExtensionRegistrar::DeactivateExtension(const Extension* extension,
                                             UnloadedExtensionReason reason) {
  registry_->TriggerOnUnloaded(extension, reason);
  renderer_helper_->OnExtensionUnloaded(*extension);
  extension_system_->UnregisterExtensionWithRequestContexts(extension->id(),
                                                            reason);
  DeactivateTaskQueueForExtension(browser_context_, extension);

  delegate_->PostDeactivateExtension(extension);
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
  extension_prefs_->SetExtensionEnabled(extension->id());

  // Move it over to the enabled list.
  CHECK(registry_->RemoveDisabled(extension->id()));
  CHECK(registry_->AddEnabled(extension));

  ActivateExtension(extension.get(), false);

  return true;
}

void ExtensionRegistrar::OnExtensionRegisteredWithRequestContexts(
    scoped_refptr<const Extension> extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  registry_->AddReady(extension);
  if (registry_->enabled_extensions().Contains(extension->id()))
    registry_->TriggerOnReady(extension.get());
}

void ExtensionRegistrar::MaybeSpinUpLazyBackgroundPage(
    const Extension* extension) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return;

  // For orphaned devtools, we will reconnect devtools to it later in
  // DidCreateRenderViewForBackgroundPage().
  bool has_orphaned_dev_tools =
      base::Contains(orphaned_dev_tools_, extension->id());

  // Reloading component extension does not trigger install, so RuntimeAPI won't
  // be able to detect its loading. Therefore, we need to spin up its lazy
  // background page.
  bool is_component_extension =
      Manifest::IsComponentLocation(extension->location());

  if (!has_orphaned_dev_tools && !is_component_extension)
    return;

  // Wake up the event page by posting a dummy task.
  const LazyContextId context_id(browser_context_, extension->id());
  context_id.GetTaskQueue()->AddPendingTask(context_id, base::DoNothing());
}

}  // namespace extensions

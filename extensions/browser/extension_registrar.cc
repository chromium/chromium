// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registrar.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/task_queue_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using content::DevToolsAgentHost;

namespace extensions {

namespace {

BASE_FEATURE(kExtensionUpdatesImmediatelyUnregisterWorker,
             "ExtensionUpdatesImmediatelyUnregisterWorker",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

ExtensionRegistrar::ExtensionRegistrar(content::BrowserContext* browser_context,
                                       Delegate* delegate)
    : browser_context_(browser_context),
      delegate_(delegate),
      extension_system_(ExtensionSystem::Get(browser_context)),
      extension_prefs_(ExtensionPrefs::Get(browser_context)),
      registry_(ExtensionRegistry::Get(browser_context)),
      renderer_helper_(
          RendererStartupHelperFactory::GetForBrowserContext(browser_context)) {
  // ExtensionRegistrar is created by ExtensionSystem via ExtensionService, and
  // ExtensionSystemFactory depends on ProcessManager, so this should be safe.
  auto* process_manager = ProcessManager::Get(browser_context_);
  DCHECK(process_manager);
  process_manager_observation_.Observe(process_manager);
}

ExtensionRegistrar::~ExtensionRegistrar() = default;

void ExtensionRegistrar::Shutdown() {
  // Setting to `nullptr`, because this raw pointer may become dangling once
  // the `ExtensionSystem` keyed service is destroyed.
  extension_system_ = nullptr;
}

void ExtensionRegistrar::AddExtension(
    scoped_refptr<const Extension> extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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
  } else if (delegate_->ShouldBlockExtension(extension.get())) {
    DCHECK(!Manifest::IsComponentLocation(extension->location()));
    registry_->AddBlocked(extension);
  } else if (extension_prefs_->IsExtensionDisabled(extension->id())) {
    registry_->AddDisabled(extension);
  } else {  // Extension should be enabled.
    registry_->AddEnabled(extension);
    ActivateExtension(extension.get(), true);
  }
}

void ExtensionRegistrar::RemoveExtension(const ExtensionId& extension_id,
                                         UnloadedExtensionReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int include_mask = ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
                     ExtensionRegistry::TERMINATED;
  scoped_refptr<const Extension> extension(
      registry_->GetExtensionById(extension_id, include_mask));

  // If the extension is blocked/blocklisted, no need to notify again.
  if (!extension)
    return;

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
    registry_->RemoveEnabled(extension_id);
    DeactivateExtension(extension.get(), reason);
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
        extensions::disable_reason::
            DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY |
        extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY |
        extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED |
        extensions::disable_reason::DISABLE_REINSTALL |
        extensions::disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION |
        extensions::disable_reason::DISABLE_NOT_VERIFIED |
        extensions::disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION;

#if BUILDFLAG(IS_CHROMEOS)
    // For controlled extensions, only allow disabling not ash-keeplisted
    // extensions if Lacros is the only browser.
    if (!crosapi::browser_util::IsAshWebBrowserEnabled()) {
      internal_disable_reason_mask |=
          extensions::disable_reason::DISABLE_NOT_ASH_KEEPLISTED;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)

    disable_reasons &= internal_disable_reason_mask;

    if (disable_reasons == disable_reason::DISABLE_NONE)
      return;
  }

  // The extension may have been disabled already. Just add the disable reasons.
  if (!IsExtensionEnabled(extension_id)) {
    extension_prefs_->AddDisableReasons(extension_id, disable_reasons);
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

void ExtensionRegistrar::ReloadExtension(
    const ExtensionId extension_id,  // Passed by value because reloading can
                                     // invalidate a reference to the ID.
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
    if (failed_to_reload_unpacked_extensions_.count(path) == 0)
      return;
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
    DisableExtension(extension_id, disable_reason::DISABLE_RELOAD);
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

  delegate_->LoadExtensionForReload(extension_id, path, load_error_behavior);
}

void ExtensionRegistrar::OnUnpackedExtensionReloadFailed(
    const base::FilePath& path) {
  failed_to_reload_unpacked_extensions_.insert(path);
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

  if (delegate_->ShouldBlockExtension(nullptr))
    return false;

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
  registry_->TriggerOnUnloaded(extension, reason);
  renderer_helper_->OnExtensionUnloaded(*extension);
  DeactivateTaskQueueForExtension(browser_context_, extension);

  delegate_->PostDeactivateExtension(extension);
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
  extension_prefs_->SetExtensionEnabled(extension->id());

  // Move it over to the enabled list.
  CHECK(registry_->RemoveDisabled(extension->id()));
  CHECK(registry_->AddEnabled(extension));

  ActivateExtension(extension.get(), false);

  return true;
}

void ExtensionRegistrar::MaybeSpinUpLazyContext(const Extension* extension,
                                                bool is_newly_added) {
  DCHECK(BackgroundInfo::HasLazyContext(extension));

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

}  // namespace extensions

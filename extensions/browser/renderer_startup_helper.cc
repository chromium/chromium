// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/renderer_startup_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/network_permissions_updater.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/origin.h"

namespace extensions {

namespace {

using ::content::BrowserContext;

// Returns the current activation sequence of |extension| if the extension is
// Service Worker-based, otherwise returns absl::nullopt.
absl::optional<base::UnguessableToken> GetWorkerActivationToken(
    BrowserContext* browser_context,
    const Extension& extension) {
  if (BackgroundInfo::IsServiceWorkerBased(&extension)) {
    return ServiceWorkerTaskQueue::Get(browser_context)
        ->GetCurrentActivationToken(extension.id());
  }
  return absl::nullopt;
}

PermissionSet CreatePermissionSet(const PermissionSet& set) {
  return PermissionSet(set.apis().Clone(), set.manifest_permissions().Clone(),
                       set.explicit_hosts().Clone(),
                       set.scriptable_hosts().Clone());
}

mojom::ExtensionLoadedParamsPtr CreateExtensionLoadedParams(
    const Extension& extension,
    bool include_tab_permissions,
    BrowserContext* browser_context) {
  const PermissionsData* permissions_data = extension.permissions_data();

  base::flat_map<int, PermissionSet> tab_specific_permissions;
  if (include_tab_permissions) {
    for (const auto& pair : permissions_data->tab_specific_permissions()) {
      tab_specific_permissions[pair.first] = CreatePermissionSet(*pair.second);
    }
  }

  return mojom::ExtensionLoadedParams::New(
      extension.manifest()->value()->Clone(), extension.location(),
      extension.path(),
      CreatePermissionSet(permissions_data->active_permissions()),
      CreatePermissionSet(permissions_data->withheld_permissions()),
      std::move(tab_specific_permissions),
      permissions_data->policy_blocked_hosts(),
      permissions_data->policy_allowed_hosts(),
      permissions_data->UsesDefaultPolicyHostRestrictions(), extension.id(),
      GetWorkerActivationToken(browser_context, extension),
      extension.creation_flags(), extension.guid());
}

}  // namespace

RendererStartupHelper::RendererStartupHelper(BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context);
}

RendererStartupHelper::~RendererStartupHelper() {
  for (auto& process_entry : process_mojo_map_)
    process_entry.first->RemoveObserver(this);
}

void RendererStartupHelper::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  InitializeProcess(host);
}

void RendererStartupHelper::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  UntrackProcess(host);
}

void RendererStartupHelper::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  UntrackProcess(host);
}

void RendererStartupHelper::InitializeProcess(
    content::RenderProcessHost* process) {
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  if (!client->IsSameContext(browser_context_, process->GetBrowserContext()))
    return;

  mojom::Renderer* renderer =
      process_mojo_map_.emplace(process, BindNewRendererRemote(process))
          .first->second.get();
  process->AddObserver(this);

  bool activity_logging_enabled =
      client->IsActivityLoggingEnabled(process->GetBrowserContext());
  // We only send the ActivityLoggingEnabled message if it is enabled; otherwise
  // the default (not enabled) is correct.
  if (activity_logging_enabled)
    renderer->SetActivityLoggingEnabled(activity_logging_enabled);

  // extensions need to know the developer mode value for api restrictions.
  renderer->SetDeveloperMode(
      GetCurrentDeveloperMode(util::GetBrowserContextId(browser_context_)));

  // Extensions need to know the channel and the session type for API
  // restrictions. The values are sent to all renderers, as the non-extension
  // renderers may have content scripts.
  bool is_lock_screen_context =
      client->IsLockScreenContext(process->GetBrowserContext());
  renderer->SetSessionInfo(GetCurrentChannel(), GetCurrentFeatureSessionType(),
                           is_lock_screen_context);

  // Platform apps need to know the system font.
  // TODO(dbeam): this is not the system font in all cases.
  renderer->SetSystemFont(webui::GetFontFamily(), webui::GetFontSize());

  // Scripting allowlist. This is modified by tests and must be communicated
  // to renderers.
  renderer->SetScriptingAllowlist(
      ExtensionsClient::Get()->GetScriptingAllowlist());

  // If the new render process is a WebView guest process, propagate the WebView
  // partition ID to it.
  std::string webview_partition_id = WebViewGuest::GetPartitionID(process);
  if (!webview_partition_id.empty()) {
    renderer->SetWebViewPartitionID(webview_partition_id);
  }

  BrowserContext* renderer_context = process->GetBrowserContext();

  // Load default policy_blocked_hosts and policy_allowed_hosts settings, part
  // of the ExtensionSettings policy.
  int context_id = util::GetBrowserContextId(renderer_context);
  renderer->UpdateDefaultPolicyHostRestrictions(
      PermissionsData::GetDefaultPolicyBlockedHosts(context_id),
      PermissionsData::GetDefaultPolicyAllowedHosts(context_id));

  renderer->UpdateUserHostRestrictions(
      PermissionsData::GetUserBlockedHosts(context_id),
      PermissionsData::GetUserAllowedHosts(context_id));

  // Loaded extensions.
  std::vector<mojom::ExtensionLoadedParamsPtr> loaded_extensions;
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  for (const auto& ext : extensions) {
    // OnExtensionLoaded should have already been called for the extension.
    DCHECK(base::Contains(extension_process_map_, ext->id()));
    DCHECK(!base::Contains(extension_process_map_[ext->id()], process));

    if (!util::IsExtensionVisibleToContext(*ext, renderer_context))
      continue;

    // TODO(kalman): Only include tab specific permissions for extension
    // processes, no other process needs it, so it's mildly wasteful.
    // I am not sure this is possible to know this here, at such a low
    // level of the stack. Perhaps site isolation can help.
    loaded_extensions.push_back(CreateExtensionLoadedParams(
        *ext, true /* include tab permissions*/, renderer_context));
    extension_process_map_[ext->id()].insert(process);
  }

  // Activate pending extensions.
  renderer->LoadExtensions(std::move(loaded_extensions));
  auto iter = pending_active_extensions_.find(process);
  if (iter != pending_active_extensions_.end()) {
    for (const ExtensionId& id : iter->second) {
      // The extension should be loaded in the process.
      DCHECK(extensions.Contains(id));
      DCHECK(base::Contains(extension_process_map_, id));
      DCHECK(base::Contains(extension_process_map_[id], process));
      renderer->ActivateExtension(id);
    }
  }

  pending_active_extensions_.erase(process);
}

void RendererStartupHelper::UntrackProcess(
    content::RenderProcessHost* process) {
  if (!ExtensionsBrowserClient::Get()->IsSameContext(
          browser_context_, process->GetBrowserContext())) {
    return;
  }

  process->RemoveObserver(this);
  process_mojo_map_.erase(process);
  pending_active_extensions_.erase(process);
  for (auto& extension_process_pair : extension_process_map_)
    extension_process_pair.second.erase(process);
}

void RendererStartupHelper::ActivateExtensionInProcess(
    const Extension& extension,
    content::RenderProcessHost* process) {
  // The extension should have been loaded already. Dump without crashing to
  // debug crbug.com/528026.
  if (!base::Contains(extension_process_map_, extension.id())) {
#if DCHECK_IS_ON()
    NOTREACHED() << "Extension " << extension.id()
                 << " activated before loading";
#else
    base::debug::DumpWithoutCrashing();
    return;
#endif
  }

  if (!util::IsExtensionVisibleToContext(extension,
                                         process->GetBrowserContext()))
    return;

  // Populate NetworkContext's OriginAccessList for this extension.
  //
  // Doing it in ActivateExtensionInProcess rather than in OnExtensionLoaded
  // ensures that we cover both the regular profile and incognito profiles.  See
  // also https://crbug.com/1197798.
  //
  // This is guaranteed to happen before the extension can make any network
  // requests (so there is no race) because ActivateExtensionInProcess will
  // always be called before creating URLLoaderFactory for any extension frames
  // that might be eventually hosted inside the renderer `process` (this
  // Browser-side ordering will be replicated within the NetworkService because
  // `SetCorsOriginAccessListsForOrigin()`, which is used in
  // NetworkPermissionsUpdater, and `CreateURLLoaderFactory()` are 2 methods
  // of the same mojom::NetworkContext interface).
  NetworkPermissionsUpdater::UpdateExtension(
      *process->GetBrowserContext(), extension,
      NetworkPermissionsUpdater::ContextSet::kCurrentContextOnly,
      base::DoNothing());

  auto remote = process_mojo_map_.find(process);
  if (remote != process_mojo_map_.end()) {
    DCHECK(base::Contains(extension_process_map_[extension.id()], process));
    remote->second->ActivateExtension(extension.id());
  } else {
    pending_active_extensions_[process].insert(extension.id());
  }
}

void RendererStartupHelper::OnExtensionLoaded(const Extension& extension) {
  DCHECK(!base::Contains(extension_process_map_, extension.id()));

  // Mark the extension as loaded.
  std::set<content::RenderProcessHost*>& loaded_process_set =
      extension_process_map_[extension.id()];

  // util::IsExtensionVisibleToContext() would filter out themes, but we choose
  // to return early for performance reasons.
  if (extension.is_theme())
    return;

  for (auto& process_entry : process_mojo_map_) {
    content::RenderProcessHost* process = process_entry.first;
    if (!util::IsExtensionVisibleToContext(extension,
                                           process->GetBrowserContext()))
      continue;

    // We don't need to include tab permissions here, since the extension
    // was just loaded.
    // Uninitialized renderers will be informed of the extension load during the
    // first batch of messages.
    std::vector<mojom::ExtensionLoadedParamsPtr> params;
    params.emplace_back(CreateExtensionLoadedParams(
        extension, false /* no tab permissions */, browser_context_));

    mojom::Renderer* renderer = GetRenderer(process);
    if (renderer)
      renderer->LoadExtensions(std::move(params));

    loaded_process_set.insert(process);
  }
}

void RendererStartupHelper::OnExtensionUnloaded(const Extension& extension) {
  DCHECK(base::Contains(extension_process_map_, extension.id()));

  const std::set<content::RenderProcessHost*>& loaded_process_set =
      extension_process_map_[extension.id()];
  for (content::RenderProcessHost* process : loaded_process_set) {
    mojom::Renderer* renderer = GetRenderer(process);
    if (renderer)
      renderer->UnloadExtension(extension.id());
  }

  // Resets registered origin access lists in the BrowserContext asynchronously.
  NetworkPermissionsUpdater::ResetOriginAccessForExtension(*browser_context_,
                                                           extension);

  for (auto& process_extensions_pair : pending_active_extensions_)
    process_extensions_pair.second.erase(extension.id());

  // Mark the extension as unloaded.
  extension_process_map_.erase(extension.id());
}

void RendererStartupHelper::OnDeveloperModeChanged(bool in_developer_mode) {
  for (auto& process_entry : process_mojo_map_) {
    content::RenderProcessHost* process = process_entry.first;
    mojom::Renderer* renderer = GetRenderer(process);
    if (renderer)
      renderer->SetDeveloperMode(in_developer_mode);
  }
}

void RendererStartupHelper::UnloadAllExtensionsForTest() {
  extension_process_map_.clear();
}

mojo::PendingAssociatedRemote<mojom::Renderer>
RendererStartupHelper::BindNewRendererRemote(
    content::RenderProcessHost* process) {
  mojo::AssociatedRemote<mojom::Renderer> renderer_interface;
  process->GetChannel()->GetRemoteAssociatedInterface(&renderer_interface);
  return renderer_interface.Unbind();
}

mojom::Renderer* RendererStartupHelper::GetRenderer(
    content::RenderProcessHost* process) {
  auto it = process_mojo_map_.find(process);
  if (it == process_mojo_map_.end())
    return nullptr;
  return it->second.get();
}

BrowserContext* RendererStartupHelper::GetRendererBrowserContext() {
  // `browser_context_` is redirected to remove incognito mode. This method
  // returns the original browser context associated with the renderer.
  auto* host = content::RenderProcessHost::FromID(receivers_.current_context());
  if (!host) {
    return nullptr;
  }

  return host->GetBrowserContext();
}

void RendererStartupHelper::AddAPIActionToActivityLog(
    const ExtensionId& extension_id,
    const std::string& call_name,
    base::Value::List args,
    const std::string& extra) {
  auto* browser_context = GetRendererBrowserContext();
  if (!browser_context) {
    return;
  }

  ExtensionsBrowserClient::Get()->AddAPIActionToActivityLog(
      browser_context, extension_id, call_name, std::move(args), extra);
}

void RendererStartupHelper::AddEventToActivityLog(
    const ExtensionId& extension_id,
    const std::string& call_name,
    base::Value::List args,
    const std::string& extra) {
  auto* browser_context = GetRendererBrowserContext();
  if (!browser_context) {
    return;
  }

  ExtensionsBrowserClient::Get()->AddEventToActivityLog(
      browser_context, extension_id, call_name, std::move(args), extra);
}

void RendererStartupHelper::AddDOMActionToActivityLog(
    const ExtensionId& extension_id,
    const std::string& call_name,
    base::Value::List args,
    const GURL& url,
    const std::u16string& url_title,
    int32_t call_type) {
  auto* browser_context = GetRendererBrowserContext();
  if (!browser_context) {
    return;
  }

  ExtensionsBrowserClient::Get()->AddDOMActionToActivityLog(
      browser_context, extension_id, call_name, std::move(args), url, url_title,
      call_type);
}

// static
void RendererStartupHelper::BindForRenderer(
    int process_id,
    mojo::PendingAssociatedReceiver<mojom::RendererHost> receiver) {
  auto* host = content::RenderProcessHost::FromID(process_id);
  if (!host) {
    return;
  }

  auto* renderer_startup_helper =
      RendererStartupHelperFactory::GetForBrowserContext(
          host->GetBrowserContext());
  renderer_startup_helper->receivers_.Add(renderer_startup_helper,
                                          std::move(receiver), process_id);
}

//////////////////////////////////////////////////////////////////////////////

// static
RendererStartupHelper* RendererStartupHelperFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<RendererStartupHelper*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
RendererStartupHelperFactory* RendererStartupHelperFactory::GetInstance() {
  return base::Singleton<RendererStartupHelperFactory>::get();
}

RendererStartupHelperFactory::RendererStartupHelperFactory()
    : BrowserContextKeyedServiceFactory(
          "RendererStartupHelper",
          BrowserContextDependencyManager::GetInstance()) {
  // No dependencies on other services.
}

RendererStartupHelperFactory::~RendererStartupHelperFactory() = default;

KeyedService* RendererStartupHelperFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new RendererStartupHelper(context);
}

BrowserContext* RendererStartupHelperFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetRedirectedContextInIncognito(
      context, /*force_guest_profile=*/true, /*force_system_profile=*/false);
}

bool RendererStartupHelperFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions

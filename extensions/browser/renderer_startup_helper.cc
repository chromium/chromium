// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/renderer_startup_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/l10n_file_util.h"
#include "extensions/browser/network_permissions_updater.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/user_script_world_configuration_manager.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

namespace extensions {

namespace {

// Gets the current activation token for `extension`.
std::optional<base::UnguessableToken> GetActivationTokenForWorkerBasedExtension(
    content::BrowserContext* browser_context,
    const Extension& extension) {
  CHECK(BackgroundInfo::IsServiceWorkerBased(&extension));
  std::optional<base::UnguessableToken> activation_token =
      ServiceWorkerTaskQueue::Get(browser_context)
          ->GetCurrentActivationToken(extension.id());

  // For the on the record profile...
  if (!browser_context->IsOffTheRecord()) {
    // Service worker-based extensions always have an activation token
    CHECK(activation_token.has_value());
  }

  // For the off the record profile...
  if (browser_context->IsOffTheRecord()) {
    if (IncognitoInfo::IsSplitMode(&extension)) {
      // Split mode extensions will have a separate activation token.
      CHECK(activation_token.has_value());
      // TODO(crbug.com/357889496): Add a test that confirms that split mode
      // tokens are different across the OnTR and OffTR extension processes.
    } else if (IncognitoInfo::IsSpanningMode(&extension)) {
      // Spanning mode extensions will not have a separate activation token.
      CHECK(!activation_token.has_value());
    }
  }

  return activation_token;
}

using ::content::BrowserContext;

// Returns the current activation sequence of |extension| if the extension is
// Service Worker-based, otherwise returns std::nullopt.
std::optional<base::UnguessableToken> GetWorkerActivationToken(
    BrowserContext* browser_context,
    const Extension& extension) {
  if (BackgroundInfo::IsServiceWorkerBased(&extension)) {
    return GetActivationTokenForWorkerBasedExtension(browser_context,
                                                     extension);
  }
  return std::nullopt;
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

base::flat_map<std::string, std::string> ToFlatMap(
    const std::map<std::string, std::string>& map) {
  return {map.begin(), map.end()};
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
  if (!client->IsSameContext(browser_context_, process->GetBrowserContext())) {
    return;
  }

  mojom::Renderer* renderer =
      process_mojo_map_.emplace(process, BindNewRendererRemote(process))
          .first->second.get();
  process->AddObserver(this);

  bool activity_logging_enabled =
      client->IsActivityLoggingEnabled(process->GetBrowserContext());
  // We only send the ActivityLoggingEnabled message if it is enabled; otherwise
  // the default (not enabled) is correct.
  if (activity_logging_enabled) {
    renderer->SetActivityLoggingEnabled(activity_logging_enabled);
  }

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

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // If the new render process is a WebView guest process, propagate the WebView
  // partition ID to it.
  if (WebViewRendererState::GetInstance()->IsGuest(process->GetID())) {
    std::string webview_partition_id = WebViewGuest::GetPartitionID(process);
    renderer->SetWebViewPartitionID(webview_partition_id);
  }
#endif

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

    if (!util::IsExtensionVisibleToContext(*ext, renderer_context)) {
      continue;
    }

    // TODO(kalman): Only include tab specific permissions for extension
    // processes, no other process needs it, so it's mildly wasteful.
    // I am not sure this is possible to know this here, at such a low
    // level of the stack. Perhaps site isolation can help.
    loaded_extensions.push_back(CreateExtensionLoadedParams(
        *ext, true /* include tab permissions*/, renderer_context));
    extension_process_map_[ext->id()].insert(process);

    // Each extension needs to know its user script world configurations.
    std::vector<mojom::UserScriptWorldInfoPtr> worlds_info =
        UserScriptWorldConfigurationManager::Get(browser_context_)
            ->GetAllUserScriptWorlds(ext->id());
    renderer->UpdateUserScriptWorlds(std::move(worlds_info));
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
    NOTREACHED_IN_MIGRATION()
        << "Extension " << extension.id() << " activated before loading";
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
  std::set<raw_ptr<content::RenderProcessHost, SetExperimental>>&
      loaded_process_set = extension_process_map_[extension.id()];

  // util::IsExtensionVisibleToContext() would filter out themes, but we choose
  // to return early for performance reasons.
  if (extension.is_theme()) {
    return;
  }

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
        extension, /*include_tab_permissions=*/false,
        process->GetBrowserContext()));
    mojom::Renderer* renderer = GetRenderer(process);
    if (renderer) {
      renderer->LoadExtensions(std::move(params));
    }

    loaded_process_set.insert(process);
  }
}

void RendererStartupHelper::OnExtensionUnloaded(const Extension& extension) {
  DCHECK(base::Contains(extension_process_map_, extension.id()));

  const std::set<raw_ptr<content::RenderProcessHost, SetExperimental>>&
      loaded_process_set = extension_process_map_[extension.id()];
  for (content::RenderProcessHost* process : loaded_process_set) {
    mojom::Renderer* renderer = GetRenderer(process);
    if (renderer) {
      renderer->UnloadExtension(extension.id());
    }
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
    if (renderer) {
      renderer->SetDeveloperMode(in_developer_mode);
    }
  }
}

void RendererStartupHelper::SetUserScriptWorldProperties(
    const Extension& extension,
    std::optional<std::string> world_id,
    std::optional<std::string> csp,
    bool enable_messaging) {
  mojom::UserScriptWorldInfoPtr info = mojom::UserScriptWorldInfo::New(
      extension.id(), std::move(world_id), std::move(csp), enable_messaging);
  for (auto& process_entry : process_mojo_map_) {
    content::RenderProcessHost* process = process_entry.first;
    mojom::Renderer* renderer = GetRenderer(process);
    if (!renderer) {
      continue;
    }

    if (!util::IsExtensionVisibleToContext(extension,
                                           process->GetBrowserContext())) {
      continue;
    }

    std::vector<mojom::UserScriptWorldInfoPtr> worlds_info;
    worlds_info.push_back(info.Clone());
    renderer->UpdateUserScriptWorlds(std::move(worlds_info));
  }
}

void RendererStartupHelper::ClearUserScriptWorldProperties(
    const Extension& extension,
    const std::optional<std::string>& world_id) {
  for (auto& process_entry : process_mojo_map_) {
    content::RenderProcessHost* process = process_entry.first;
    mojom::Renderer* renderer = GetRenderer(process);
    if (!renderer) {
      continue;
    }

    if (!util::IsExtensionVisibleToContext(extension,
                                           process->GetBrowserContext())) {
      continue;
    }

    renderer->ClearUserScriptWorldConfig(extension.id(), world_id);
  }
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
  if (it == process_mojo_map_.end()) {
    return nullptr;
  }
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

void RendererStartupHelper::GetMessageBundle(
    const ExtensionId& extension_id,
    GetMessageBundleCallback callback) {
  auto* browser_context = GetRendererBrowserContext();
  if (!browser_context) {
    std::move(callback).Run({});
    return;
  }

  const ExtensionSet& extension_set =
      ExtensionRegistry::Get(browser_context)->enabled_extensions();
  const Extension* extension = extension_set.GetByID(extension_id);

  if (!extension) {  // The extension has gone.
    std::move(callback).Run({});
    return;
  }

  const std::string& default_locale = LocaleInfo::GetDefaultLocale(extension);
  if (default_locale.empty()) {
    // A little optimization: send the answer here to avoid an extra thread hop.
    std::unique_ptr<MessageBundle::SubstitutionMap> dictionary_map =
        l10n_file_util::LoadNonLocalizedMessageBundleSubstitutionMap(
            extension_id);
    std::move(callback).Run(ToFlatMap(*dictionary_map));
    return;
  }

  std::vector<base::FilePath> paths_to_load;
  paths_to_load.push_back(extension->path());

  auto imports = SharedModuleInfo::GetImports(extension);
  // Iterate through the imports in reverse.  This will allow later imported
  // modules to override earlier imported modules, as the list order is
  // maintained from the definition in manifest.json of the imports.
  for (const SharedModuleInfo::ImportInfo& import : base::Reversed(imports)) {
    const Extension* imported_extension =
        extension_set.GetByID(import.extension_id);
    if (!imported_extension) {
      NOTREACHED_IN_MIGRATION()
          << "Missing shared module " << import.extension_id;
      continue;
    }
    paths_to_load.push_back(imported_extension->path());
  }

  // This blocks tab loading. Priority is inherited from the calling context.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const std::vector<base::FilePath>& extension_paths,
             const ExtensionId& main_extension_id,
             const std::string& default_locale,
             extension_l10n_util::GzippedMessagesPermission gzip_permission) {
            return l10n_file_util::LoadMessageBundleSubstitutionMapFromPaths(
                extension_paths, main_extension_id, default_locale,
                gzip_permission);
          },
          paths_to_load, extension_id, default_locale,
          extension_l10n_util::GetGzippedMessagesPermissionForExtension(
              extension)),
      base::BindOnce(
          [](GetMessageBundleCallback callback,
             std::unique_ptr<MessageBundle::SubstitutionMap> dictionary_map) {
            std::move(callback).Run(ToFlatMap(*dictionary_map));
          },
          std::move(callback)));
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
          BrowserContextDependencyManager::GetInstance()) {}

RendererStartupHelperFactory::~RendererStartupHelperFactory() = default;

std::unique_ptr<KeyedService>
RendererStartupHelperFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<RendererStartupHelper>(context);
}

BrowserContext* RendererStartupHelperFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

bool RendererStartupHelperFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions

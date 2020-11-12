// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/renderer_startup_helper.h"

#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
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
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/common/activation_sequence.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/origin.h"

using content::BrowserContext;

namespace extensions {

namespace {

// Returns whether the |extension| should be loaded in the given
// |browser_context|.
bool IsExtensionVisibleToContext(const Extension& extension,
                                 content::BrowserContext* browser_context) {
  // Renderers don't need to know about themes.
  if (extension.is_theme())
    return false;

  // Only extensions enabled in incognito mode should be loaded in an incognito
  // renderer. However extensions which can't be enabled in the incognito mode
  // (e.g. platform apps) should also be loaded in an incognito renderer to
  // ensure connections from incognito tabs to such extensions work.
  return !browser_context->IsOffTheRecord() ||
         !util::CanBeIncognitoEnabled(&extension) ||
         util::IsIncognitoEnabled(extension.id(), browser_context);
}

// Returns the current ActivationSequence of |extension| if the extension is
// Service Worker-based, otherwise returns base::nullopt.
base::Optional<ActivationSequence> GetWorkerActivationSequence(
    BrowserContext* browser_context,
    const Extension& extension) {
  if (BackgroundInfo::IsServiceWorkerBased(&extension)) {
    return ServiceWorkerTaskQueue::Get(browser_context)
        ->GetCurrentSequence(extension.id());
  }
  return base::nullopt;
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

  // Extensions need to know the channel and the session type for API
  // restrictions. The values are sent to all renderers, as the non-extension
  // renderers may have content scripts.
  bool is_lock_screen_context =
      client->IsLockScreenContext(process->GetBrowserContext());
  process->Send(new ExtensionMsg_SetSessionInfo(GetCurrentChannel(),
                                                GetCurrentFeatureSessionType(),
                                                is_lock_screen_context));

  // Platform apps need to know the system font.
  // TODO(dbeam): this is not the system font in all cases.
  process->Send(new ExtensionMsg_SetSystemFont(webui::GetFontFamily(),
                                               webui::GetFontSize()));

  // Scripting allowlist. This is modified by tests and must be communicated
  // to renderers.
  process->Send(new ExtensionMsg_SetScriptingAllowlist(
      extensions::ExtensionsClient::Get()->GetScriptingAllowlist()));

  // If the new render process is a WebView guest process, propagate the WebView
  // partition ID to it.
  std::string webview_partition_id = WebViewGuest::GetPartitionID(process);
  if (!webview_partition_id.empty()) {
    process->Send(new ExtensionMsg_SetWebViewPartitionID(
        WebViewGuest::GetPartitionID(process)));
  }

  BrowserContext* renderer_context = process->GetBrowserContext();

  // Load default policy_blocked_hosts and policy_allowed_hosts settings, part
  // of the ExtensionSettings policy.
  ExtensionMsg_UpdateDefaultPolicyHostRestrictions_Params params;
  int context_id = util::GetBrowserContextId(renderer_context);
  params.default_policy_blocked_hosts =
      PermissionsData::GetDefaultPolicyBlockedHosts(context_id);
  params.default_policy_allowed_hosts =
      PermissionsData::GetDefaultPolicyAllowedHosts(context_id);
  process->Send(new ExtensionMsg_UpdateDefaultPolicyHostRestrictions(params));

  // Loaded extensions.
  std::vector<ExtensionMsg_Loaded_Params> loaded_extensions;
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  for (const auto& ext : extensions) {
    // OnExtensionLoaded should have already been called for the extension.
    DCHECK(base::Contains(extension_process_map_, ext->id()));
    DCHECK(!base::Contains(extension_process_map_[ext->id()], process));

    if (!IsExtensionVisibleToContext(*ext, renderer_context))
      continue;

    // TODO(kalman): Only include tab specific permissions for extension
    // processes, no other process needs it, so it's mildly wasteful.
    // I am not sure this is possible to know this here, at such a low
    // level of the stack. Perhaps site isolation can help.
    bool include_tab_permissions = true;
    loaded_extensions.push_back(ExtensionMsg_Loaded_Params(
        ext.get(), include_tab_permissions,
        GetWorkerActivationSequence(renderer_context, *ext)));
    extension_process_map_[ext->id()].insert(process);
  }

  // Activate pending extensions.
  process->Send(new ExtensionMsg_Loaded(loaded_extensions));
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
                 << "activated before loading";
#else
    base::debug::DumpWithoutCrashing();
    return;
#endif
  }

  if (!IsExtensionVisibleToContext(extension, process->GetBrowserContext()))
    return;

  auto remote = process_mojo_map_.find(process);
  if (remote != process_mojo_map_.end()) {
    DCHECK(base::Contains(extension_process_map_[extension.id()], process));
    remote->second->ActivateExtension(extension.id());
  } else {
    pending_active_extensions_[process].insert(extension.id());
  }
}

void RendererStartupHelper::OnExtensionLoaded(const Extension& extension) {
  // Extension was already loaded.
  // TODO(crbug.com/708230): Ensure that clients don't call this for an
  // already loaded extension and change this to a DCHECK.
  if (base::Contains(extension_process_map_, extension.id()))
    return;

  // Mark the extension as loaded.
  std::set<content::RenderProcessHost*>& loaded_process_set =
      extension_process_map_[extension.id()];

  // IsExtensionVisibleToContext() would filter out themes, but we choose to
  // return early for performance reasons.
  if (extension.is_theme())
    return;

  // Registers the initial origin access lists to the BrowserContext
  // asynchronously.
  url::Origin extension_origin = url::Origin::Create(extension.url());
  std::vector<network::mojom::CorsOriginPatternPtr> allow_list =
      CreateCorsOriginAccessAllowList(
          extension,
          PermissionsData::EffectiveHostPermissionsMode::kOmitTabSpecific);
  browser_context_->SetCorsOriginAccessListForOrigin(
      extension_origin, std::move(allow_list),
      CreateCorsOriginAccessBlockList(extension), base::DoNothing::Once());

  // We don't need to include tab permisisons here, since the extension
  // was just loaded.
  // Uninitialized renderers will be informed of the extension load during the
  // first batch of messages.
  std::vector<ExtensionMsg_Loaded_Params> params;
  params.emplace_back(&extension, false /* no tab permissions */,
                      GetWorkerActivationSequence(browser_context_, extension));

  for (auto& process_entry : process_mojo_map_) {
    content::RenderProcessHost* process = process_entry.first;
    if (!IsExtensionVisibleToContext(extension, process->GetBrowserContext()))
      continue;
    process->Send(new ExtensionMsg_Loaded(params));
    loaded_process_set.insert(process);
  }
}

void RendererStartupHelper::OnExtensionUnloaded(const Extension& extension) {
  // Extension is not loaded.
  // TODO(crbug.com/708230): Ensure that clients call this for a loaded
  // extension only and change this to a DCHECK.
  if (!base::Contains(extension_process_map_, extension.id()))
    return;

  const std::set<content::RenderProcessHost*>& loaded_process_set =
      extension_process_map_[extension.id()];
  for (content::RenderProcessHost* process : loaded_process_set) {
    DCHECK(base::Contains(process_mojo_map_, process));
    process->Send(new ExtensionMsg_Unloaded(extension.id()));
  }

  // Resets registered origin access lists in the BrowserContext asynchronously.
  url::Origin extension_origin = url::Origin::Create(extension.url());
  browser_context_->SetCorsOriginAccessListForOrigin(
      extension_origin, std::vector<network::mojom::CorsOriginPatternPtr>(),
      std::vector<network::mojom::CorsOriginPatternPtr>(),
      base::DoNothing::Once());

  for (auto& process_extensions_pair : pending_active_extensions_)
    process_extensions_pair.second.erase(extension.id());

  // Mark the extension as unloaded.
  extension_process_map_.erase(extension.id());
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
  DCHECK(base::Contains(process_mojo_map_, process));
  return process_mojo_map_.find(process)->second.get();
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

RendererStartupHelperFactory::~RendererStartupHelperFactory() {}

KeyedService* RendererStartupHelperFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new RendererStartupHelper(context);
}

BrowserContext* RendererStartupHelperFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool RendererStartupHelperFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions

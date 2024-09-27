// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/app_view/app_view_guest.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/app_view/app_view_constants.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message_macros.h"

namespace app_runtime = extensions::api::app_runtime;

using content::RenderFrameHost;
using content::WebContents;
using extensions::ExtensionHost;
using guest_view::GuestViewBase;

namespace extensions {

namespace {

struct ResponseInfo {
  scoped_refptr<const Extension> guest_extension;
  std::unique_ptr<GuestViewBase> app_view_guest;
  GuestViewBase::WebContentsCreatedCallback callback;

  ResponseInfo(const Extension* guest_extension,
               std::unique_ptr<GuestViewBase> app_view_guest,
               GuestViewBase::WebContentsCreatedCallback callback)
      : guest_extension(guest_extension),
        app_view_guest(std::move(app_view_guest)),
        callback(std::move(callback)) {}

  ~ResponseInfo() = default;
};

using PendingResponseMap = std::map<int, std::unique_ptr<ResponseInfo>>;
base::LazyInstance<PendingResponseMap>::DestructorAtExit
    g_pending_response_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static.
const char AppViewGuest::Type[] = "appview";
const guest_view::GuestViewHistogramValue AppViewGuest::HistogramValue =
    guest_view::GuestViewHistogramValue::kAppView;

// static.
bool AppViewGuest::CompletePendingRequest(
    content::BrowserContext* browser_context,
    const GURL& url,
    int guest_instance_id,
    const std::string& guest_extension_id,
    content::RenderProcessHost* guest_render_process_host) {
  PendingResponseMap* response_map = g_pending_response_map.Pointer();
  auto it = response_map->find(guest_instance_id);
  // Kill the requesting process if it is not the real guest.
  if (it == response_map->end()) {
    // The requester used an invalid |guest_instance_id|.
    bad_message::ReceivedBadMessage(guest_render_process_host,
                                    bad_message::AVG_BAD_INST_ID);
    return false;
  }

  ResponseInfo* response_info = it->second.get();
  scoped_refptr<const Extension> guest_extension =
      response_info->guest_extension;

  if (guest_extension->id() != guest_extension_id) {
    // The app is trying to communicate with an <appview> not assigned to it.
    bad_message::ReceivedBadMessage(guest_render_process_host,
                                    bad_message::AVG_BAD_EXT_ID);
    return false;
  }

  std::unique_ptr<GuestViewBase> app_view_guest =
      std::move(response_info->app_view_guest);
  GuestViewBase::WebContentsCreatedCallback callback =
      std::move(response_info->callback);
  response_map->erase(it);

  auto* raw_app_view_guest = static_cast<AppViewGuest*>(app_view_guest.get());
  raw_app_view_guest->CompleteCreateWebContents(url, guest_extension.get(),
                                                std::move(app_view_guest),
                                                std::move(callback));

  return true;
}

// static
std::unique_ptr<GuestViewBase> AppViewGuest::Create(
    content::RenderFrameHost* owner_rfh) {
  return base::WrapUnique(new AppViewGuest(owner_rfh));
}

AppViewGuest::AppViewGuest(content::RenderFrameHost* owner_rfh)
    : GuestView<AppViewGuest>(owner_rfh),
      app_view_guest_delegate_(base::WrapUnique(
          ExtensionsAPIClient::Get()->CreateAppViewGuestDelegate())) {
  if (app_view_guest_delegate_) {
    app_delegate_ =
        base::WrapUnique(app_view_guest_delegate_->CreateAppDelegate(
            owner_rfh->GetBrowserContext()));
  }
}

AppViewGuest::~AppViewGuest() = default;

bool AppViewGuest::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  DCHECK_EQ(web_contents(),
            content::WebContents::FromRenderFrameHost(&render_frame_host));

  if (app_view_guest_delegate_) {
    return app_view_guest_delegate_->HandleContextMenu(render_frame_host,
                                                       params);
  }
  return false;
}

bool AppViewGuest::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  return true;
}

content::WebContents* AppViewGuest::CreateCustomWebContents(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    bool is_new_browsing_instance,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace) {
  // Suppress window creation.
  return nullptr;
}

void AppViewGuest::RequestMediaAccessPermission(
    WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  if (!app_delegate_) {
    WebContentsDelegate::RequestMediaAccessPermission(web_contents, request,
                                                      std::move(callback));
    return;
  }
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context())->enabled_extensions();
  const Extension* guest_extension =
      enabled_extensions.GetByID(guest_extension_id_);

  app_delegate_->RequestMediaAccessPermission(
      web_contents, request, std::move(callback), guest_extension);
}

bool AppViewGuest::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  if (!app_delegate_) {
    return WebContentsDelegate::CheckMediaAccessPermission(
        render_frame_host, security_origin, type);
  }
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context())->enabled_extensions();
  const Extension* guest_extension =
      enabled_extensions.GetByID(guest_extension_id_);

  return app_delegate_->CheckMediaAccessPermission(
      render_frame_host, security_origin, type, guest_extension);
}

void AppViewGuest::CreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                                     const base::Value::Dict& create_params,
                                     WebContentsCreatedCallback callback) {
  const std::string* app_id = create_params.FindString(appview::kAppID);
  if (!app_id) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }
  // Verifying that the appId is not the same as the host application.
  if (owner_host() == *app_id) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }

  const base::Value::Dict* data = create_params.FindDict(appview::kData);
  if (!data) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }

  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context())->enabled_extensions();
  const Extension* guest_extension = enabled_extensions.GetByID(*app_id);
  const Extension* embedder_extension =
      enabled_extensions.GetByID(GetOwnerSiteURL().host());

  if (!guest_extension || !guest_extension->is_platform_app() ||
      !embedder_extension || !embedder_extension->is_platform_app()) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }

  const auto context_id =
      LazyContextId::ForExtension(browser_context(), guest_extension);
  LazyContextTaskQueue* queue = context_id.GetTaskQueue();
  if (queue->ShouldEnqueueTask(browser_context(), guest_extension)) {
    queue->AddPendingTask(
        context_id,
        base::BindOnce(&AppViewGuest::LaunchAppAndFireEvent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(owned_this),
                       data->Clone(), std::move(callback)));
    return;
  }

  ProcessManager* process_manager = ProcessManager::Get(browser_context());
  ExtensionHost* host =
      process_manager->GetBackgroundHostForExtension(guest_extension->id());
  DCHECK(host);
  LaunchAppAndFireEvent(
      std::move(owned_this), data->Clone(), std::move(callback),
      std::make_unique<LazyContextTaskQueue::ContextInfo>(host));
}

void AppViewGuest::DidInitialize(const base::Value::Dict& create_params) {
  ExtensionsAPIClient::Get()->AttachWebContentsHelpers(web_contents());

  if (!url_.is_valid()) {
    return;
  }

  GetController().LoadURL(url_, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                          std::string());
}

void AppViewGuest::MaybeRecreateGuestContents(
    content::RenderFrameHost* outer_contents_frame) {
  // This situation is not possible for AppView.
  NOTREACHED_IN_MIGRATION();
}

const char* AppViewGuest::GetAPINamespace() const {
  return appview::kEmbedderAPINamespace;
}

int AppViewGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_APPVIEW_TAG_PREFIX;
}

void AppViewGuest::CompleteCreateWebContents(
    const GURL& url,
    const Extension* guest_extension,
    std::unique_ptr<GuestViewBase> owned_this,
    WebContentsCreatedCallback callback) {
  if (!owner_rfh()) {
    // The owner was destroyed before getting a response to the embedding
    // request, so we can't proceed with creating a guest.
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }
  if (!url.is_valid()) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }
  url_ = url;
  guest_extension_id_ = guest_extension->id();

  WebContents::CreateParams params(
      browser_context(),
      content::SiteInstance::CreateForURL(browser_context(),
                                          guest_extension->url()));
  params.guest_delegate = this;
  auto web_contents = WebContents::Create(params);
  app_delegate_->InitWebContents(web_contents.get());
  std::move(callback).Run(std::move(owned_this), std::move(web_contents));
}

void AppViewGuest::LaunchAppAndFireEvent(
    std::unique_ptr<GuestViewBase> owned_this,
    base::Value::Dict data,
    WebContentsCreatedCallback callback,
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
  bool has_event_listener = EventRouter::Get(browser_context())
                                ->ExtensionHasEventListener(
                                    context_info->extension_id,
                                    app_runtime::OnEmbedRequested::kEventName);
  if (!has_event_listener) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }

  const Extension* const extension =
      extensions::ExtensionRegistry::Get(context_info->browser_context)
          ->enabled_extensions()
          .GetByID(context_info->extension_id);

  g_pending_response_map.Get().insert(std::make_pair(
      guest_instance_id(),
      std::make_unique<ResponseInfo>(extension, std::move(owned_this),
                                     std::move(callback))));

  base::Value::Dict embed_request;
  embed_request.Set(appview::kGuestInstanceID, guest_instance_id());
  embed_request.Set(appview::kEmbedderID, owner_host());
  embed_request.Set(appview::kData, std::move(data));
  AppRuntimeEventRouter::DispatchOnEmbedRequestedEvent(
      browser_context(), std::move(embed_request), extension);
}

void AppViewGuest::SetAppDelegateForTest(AppDelegate* delegate) {
  app_delegate_.reset(delegate);
}

std::vector<int> AppViewGuest::GetAllRegisteredInstanceIdsForTesting() {
  std::vector<int> instances;
  for (const auto& key_value : g_pending_response_map.Get()) {
    instances.push_back(key_value.first);
  }
  return instances;
}

}  // namespace extensions

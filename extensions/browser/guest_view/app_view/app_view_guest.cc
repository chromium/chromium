// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/app_view/app_view_guest.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
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
#include "extensions/common/extension_messages.h"
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
  base::WeakPtr<AppViewGuest> app_view_guest;
  GuestViewBase::WebContentsCreatedCallback callback;

  ResponseInfo(const Extension* guest_extension,
               const base::WeakPtr<AppViewGuest>& app_view_guest,
               GuestViewBase::WebContentsCreatedCallback callback)
      : guest_extension(guest_extension),
        app_view_guest(app_view_guest),
        callback(std::move(callback)) {}

  ~ResponseInfo() {}
};

using PendingResponseMap = std::map<int, std::unique_ptr<ResponseInfo>>;
static base::LazyInstance<PendingResponseMap>::DestructorAtExit
    pending_response_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static.
const char AppViewGuest::Type[] = "appview";

// static.
bool AppViewGuest::CompletePendingRequest(
    content::BrowserContext* browser_context,
    const GURL& url,
    int guest_instance_id,
    const std::string& guest_extension_id,
    content::RenderProcessHost* guest_render_process_host) {
  PendingResponseMap* response_map = pending_response_map.Pointer();
  auto it = response_map->find(guest_instance_id);
  // Kill the requesting process if it is not the real guest.
  if (it == response_map->end()) {
    // The requester used an invalid |guest_instance_id|.
    bad_message::ReceivedBadMessage(guest_render_process_host,
                                    bad_message::AVG_BAD_INST_ID);
    return false;
  }

  ResponseInfo* response_info = it->second.get();
  if (!response_info->app_view_guest ||
      (response_info->guest_extension->id() != guest_extension_id)) {
    // The app is trying to communicate with an <appview> not assigned to it, or
    // the <appview> is already dead "nullptr".
    bad_message::BadMessageReason reason = !response_info->app_view_guest
                                               ? bad_message::AVG_NULL_AVG
                                               : bad_message::AVG_BAD_EXT_ID;
    bad_message::ReceivedBadMessage(guest_render_process_host, reason);
    return false;
  }

  response_info->app_view_guest->CompleteCreateWebContents(
      url, response_info->guest_extension.get(),
      std::move(response_info->callback));

  response_map->erase(guest_instance_id);
  return true;
}

// static
GuestViewBase* AppViewGuest::Create(WebContents* owner_web_contents) {
  return new AppViewGuest(owner_web_contents);
}

AppViewGuest::AppViewGuest(WebContents* owner_web_contents)
    : GuestView<AppViewGuest>(owner_web_contents),
      app_view_guest_delegate_(
          ExtensionsAPIClient::Get()->CreateAppViewGuestDelegate()) {
  if (app_view_guest_delegate_)
    app_delegate_.reset(app_view_guest_delegate_->CreateAppDelegate());
}

AppViewGuest::~AppViewGuest() {
}

bool AppViewGuest::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  if (app_view_guest_delegate_) {
    return app_view_guest_delegate_->HandleContextMenu(web_contents(), params);
  }
  return false;
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
    const GURL& security_origin,
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

void AppViewGuest::CreateWebContents(const base::DictionaryValue& create_params,
                                     WebContentsCreatedCallback callback) {
  std::string app_id;
  if (!create_params.GetString(appview::kAppID, &app_id)) {
    std::move(callback).Run(nullptr);
    return;
  }
  // Verifying that the appId is not the same as the host application.
  if (owner_host() == app_id) {
    std::move(callback).Run(nullptr);
    return;
  }
  const base::DictionaryValue* data = nullptr;
  if (!create_params.GetDictionary(appview::kData, &data)) {
    std::move(callback).Run(nullptr);
    return;
  }

  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context())->enabled_extensions();
  const Extension* guest_extension = enabled_extensions.GetByID(app_id);
  const Extension* embedder_extension =
      enabled_extensions.GetByID(GetOwnerSiteURL().host());

  if (!guest_extension || !guest_extension->is_platform_app() ||
      !embedder_extension || !embedder_extension->is_platform_app()) {
    std::move(callback).Run(nullptr);
    return;
  }

  pending_response_map.Get().insert(std::make_pair(
      guest_instance_id(), std::make_unique<ResponseInfo>(
                               guest_extension, weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback))));

  const LazyContextId context_id(browser_context(), guest_extension->id());
  LazyContextTaskQueue* queue = context_id.GetTaskQueue();
  if (queue->ShouldEnqueueTask(browser_context(), guest_extension)) {
    queue->AddPendingTask(
        context_id,
        base::BindOnce(&AppViewGuest::LaunchAppAndFireEvent,
                       weak_ptr_factory_.GetWeakPtr(), data->CreateDeepCopy(),
                       std::move(callback)));
    return;
  }

  ProcessManager* process_manager = ProcessManager::Get(browser_context());
  ExtensionHost* host =
      process_manager->GetBackgroundHostForExtension(guest_extension->id());
  DCHECK(host);
  LaunchAppAndFireEvent(
      data->CreateDeepCopy(), std::move(callback),
      std::make_unique<LazyContextTaskQueue::ContextInfo>(host));
}

void AppViewGuest::DidInitialize(const base::DictionaryValue& create_params) {
  ExtensionsAPIClient::Get()->AttachWebContentsHelpers(web_contents());

  if (!url_.is_valid())
    return;

  web_contents()->GetController().LoadURL(
      url_, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
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
    WebContentsCreatedCallback callback) {
  if (!url.is_valid()) {
    std::move(callback).Run(nullptr);
    return;
  }
  url_ = url;
  guest_extension_id_ = guest_extension->id();

  WebContents::CreateParams params(
      browser_context(),
      content::SiteInstance::CreateForURL(browser_context(),
                                          guest_extension->url()));
  params.guest_delegate = this;
  // TODO(erikchen): Fix ownership semantics for guest views.
  // https://crbug.com/832879.
  std::move(callback).Run(WebContents::Create(params).release());
}

void AppViewGuest::LaunchAppAndFireEvent(
    std::unique_ptr<base::DictionaryValue> data,
    WebContentsCreatedCallback callback,
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
  bool has_event_listener = EventRouter::Get(browser_context())
                                ->ExtensionHasEventListener(
                                    context_info->extension_id,
                                    app_runtime::OnEmbedRequested::kEventName);
  if (!has_event_listener) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<base::DictionaryValue> embed_request(
      new base::DictionaryValue());
  embed_request->SetInteger(appview::kGuestInstanceID, guest_instance_id());
  embed_request->SetString(appview::kEmbedderID, owner_host());
  embed_request->Set(appview::kData, std::move(data));
  const Extension* const extension =
      extensions::ExtensionRegistry::Get(context_info->browser_context)
          ->enabled_extensions()
          .GetByID(context_info->extension_id);
  AppRuntimeEventRouter::DispatchOnEmbedRequestedEvent(
      browser_context(), std::move(embed_request), extension);
}

void AppViewGuest::SetAppDelegateForTest(AppDelegate* delegate) {
  app_delegate_.reset(delegate);
}

std::vector<int> AppViewGuest::GetAllRegisteredInstanceIdsForTesting() {
  std::vector<int> instances;
  for (const auto& key_value : pending_response_map.Get()) {
    instances.push_back(key_value.first);
  }
  return instances;
}

}  // namespace extensions

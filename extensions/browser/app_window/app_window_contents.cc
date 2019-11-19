// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_contents.h"

#include <memory>
#include <string>
#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension_messages.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

namespace extensions {

AppWindowContentsImpl::AppWindowContentsImpl(AppWindow* host) : host_(host) {}

AppWindowContentsImpl::~AppWindowContentsImpl() {}

void AppWindowContentsImpl::Initialize(content::BrowserContext* context,
                                       content::RenderFrameHost* creator_frame,
                                       const GURL& url) {
  url_ = url;

  content::WebContents::CreateParams create_params(
      context, creator_frame->GetSiteInstance());
  create_params.opener_render_process_id = creator_frame->GetProcess()->GetID();
  create_params.opener_render_frame_id = creator_frame->GetRoutingID();
  web_contents_ = content::WebContents::Create(create_params);

  Observe(web_contents_.get());
  web_contents_->GetMutableRendererPrefs()->
      browser_handles_all_top_level_requests = true;
  web_contents_->SyncRendererPrefs();
}

void AppWindowContentsImpl::LoadContents(int32_t creator_process_id) {
  // Sandboxed page that are not in the Chrome App package are loaded in a
  // different process.
  if (web_contents_->GetMainFrame()->GetProcess()->GetID() !=
      creator_process_id) {
    VLOG(1) << "AppWindow created in new process ("
            << web_contents_->GetMainFrame()->GetProcess()->GetID()
            << ") != creator (" << creator_process_id << "). Routing disabled.";
  }
  web_contents_->GetController().LoadURL(
      url_, content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());
}

void AppWindowContentsImpl::NativeWindowChanged(
    NativeAppWindow* native_app_window) {
  base::ListValue args;
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());
  host_->GetSerializedState(dictionary.get());
  args.Append(std::move(dictionary));

  content::RenderFrameHost* rfh = web_contents_->GetMainFrame();
  rfh->Send(new ExtensionMsg_MessageInvoke(rfh->GetRoutingID(),
                                           host_->extension_id(), "app.window",
                                           "updateAppWindowProperties", args));
}

void AppWindowContentsImpl::NativeWindowClosed(bool send_onclosed) {
  content::RenderFrameHost* rfh = web_contents_->GetMainFrame();
  rfh->Send(
      new ExtensionMsg_AppWindowClosed(rfh->GetRoutingID(), send_onclosed));
}

content::WebContents* AppWindowContentsImpl::GetWebContents() const {
  return web_contents_.get();
}

WindowController* AppWindowContentsImpl::GetWindowController() const {
  return nullptr;
}

bool AppWindowContentsImpl::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* sender) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(AppWindowContentsImpl, message, sender)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_UpdateDraggableRegions,
                        UpdateDraggableRegions)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AppWindowContentsImpl::DidFinishNavigation(
    content::NavigationHandle* handle) {
  // The callback inside app_window will be moved after the first call.
  host_->OnDidFinishFirstNavigation();
}

void AppWindowContentsImpl::UpdateDraggableRegions(
    content::RenderFrameHost* sender,
    const std::vector<DraggableRegion>& regions) {
  if (!sender->GetParent())  // Only process events from the main frame.
    host_->UpdateDraggableRegions(regions);
}

}  // namespace extensions

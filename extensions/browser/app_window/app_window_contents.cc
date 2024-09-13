// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_contents.h"

#include <memory>
#include <string>
#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace extensions {

AppWindowContentsImpl::AppWindowContentsImpl(AppWindow* host) : host_(host) {}

AppWindowContentsImpl::~AppWindowContentsImpl() = default;

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
  if (web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID() !=
      creator_process_id) {
    VLOG(1) << "AppWindow created in new process ("
            << web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID()
            << ") != creator (" << creator_process_id << "). Routing disabled.";
  }
  web_contents_->GetController().LoadURL(
      url_, content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());
}

void AppWindowContentsImpl::NativeWindowChanged(
    NativeAppWindow* native_app_window) {
  base::Value::Dict dictionary;
  host_->GetSerializedState(&dictionary);
  base::Value::List args;
  args.Append(std::move(dictionary));

  content::RenderFrameHost* render_frame_host =
      web_contents_->GetPrimaryMainFrame();
  // Return early if this method is called before RenderFrameCreated(). (e.g.
  // if AppWindow is created and shown before navigation, this method is called
  // for the visibility change.)
  if (!render_frame_host->IsRenderFrameLive()) {
    return;
  }
  ExtensionWebContentsObserver::GetForWebContents(web_contents())
      ->GetLocalFrameChecked(render_frame_host)
      .MessageInvoke(host_->extension_id(), "app.window",
                     "updateAppWindowProperties", std::move(args));
}

void AppWindowContentsImpl::NativeWindowClosed(bool send_onclosed) {
  // Return early if this method is called when the render frame is not live.
  if (!web_contents_->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    return;
  }
  ExtensionWebContentsObserver::GetForWebContents(web_contents())
      ->GetLocalFrameChecked(web_contents_->GetPrimaryMainFrame())
      .AppWindowClosed(send_onclosed);
}

content::WebContents* AppWindowContentsImpl::GetWebContents() const {
  return web_contents_.get();
}

WindowController* AppWindowContentsImpl::GetWindowController() const {
  return nullptr;
}

void AppWindowContentsImpl::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame()) {
    return;
  }

  // The callback inside app_window will be moved after the first call.
  host_->OnDidFinishFirstNavigation();
}

}  // namespace extensions

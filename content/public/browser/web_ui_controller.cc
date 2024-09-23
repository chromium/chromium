// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_controller.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webui/web_ui_managed_interface.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "url/gurl.h"

namespace content {

namespace {
// This registry maintains a mapping from WebUI to its MojoJS interface broker
// initializer, i.e. callbacks that populate an interface broker's binder map
// with interfaces exposed to MojoJS. If such a mapping exists, we instantiate
// the broker in ReadyToCommitNavigation, enable MojoJS bindings for this
// frame, and ask renderer to use it to handle Mojo.bindInterface calls.
base::LazyInstance<WebUIBrowserInterfaceBrokerRegistry>::Leaky
    g_web_ui_browser_interface_broker_registry = LAZY_INSTANCE_INITIALIZER;
}  // namespace

WebUIController::WebUIController(WebUI* web_ui) : web_ui_(web_ui) {}

WebUIController::~WebUIController() {
  RemoveWebUIManagedInterfaces(this);
}

bool WebUIController::OverrideHandleWebUIMessage(
    const GURL& source_url,
    const std::string& message,
    const base::Value::List& args) {
  return false;
}

WebUIController::Type WebUIController::GetType() {
  return nullptr;
}

bool WebUIController::IsJavascriptErrorReportingEnabled() {
  return true;
}

void WebUIController::WebUIReadyToCommitNavigation(
    RenderFrameHost* render_frame_host) {
  GURL site_url = render_frame_host->GetSiteInstance()->GetSiteURL();
  GetContentClient()->browser()->LogWebUIUrl(site_url);

  broker_ =
      g_web_ui_browser_interface_broker_registry.Get().CreateInterfaceBroker(
          *this);

  if (broker_) {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    // If this WebUIController has a per-WebUI interface broker, create the
    // broker's remote and ask renderer to use it.
    rfh->EnableMojoJsBindingsWithBroker(broker_->BindNewPipeAndPassRemote());
  }
}

}  // namespace content

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/xr/webxr_internals/webxr_internals_ui.h"

#include "base/feature_list.h"
#include "content/browser/xr/webxr_internals/webxr_internals_handler_impl.h"
#include "content/grit/webxr_internals_resources.h"
#include "content/grit/webxr_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "device/vr/buildflags/buildflags.h"

#include "device/vr/public/cpp/features.h"

namespace content {

namespace {

void CreateAndAddWebXrInternalsHTMLSource(BrowserContext* browser_context) {
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      browser_context, kChromeUIWebXrInternalsHost);

  // Add TrustedTypes policies necessary for using Polymer.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types polymer-html-literal "
      "polymer-template-event-attribute-policy;");

  // Add required resources.
  source->UseStringsJs();
  source->AddResourcePaths(
      base::make_span(kWebxrInternalsResources, kWebxrInternalsResourcesSize));
  source->AddResourcePath("", IDR_WEBXR_INTERNALS_WEBXR_INTERNALS_HTML);
}

}  // namespace

bool WebXrInternalsUIConfig::IsWebUIEnabled(BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(device::features::kWebXrInternals);
}

WEB_UI_CONTROLLER_TYPE_IMPL(WebXrInternalsUI)

WebXrInternalsUI::~WebXrInternalsUI() = default;

WebXrInternalsUI::WebXrInternalsUI(WebUI* web_ui) : WebUIController(web_ui) {
  CreateAndAddWebXrInternalsHTMLSource(
      web_ui->GetWebContents()->GetBrowserContext());
}

void WebXrInternalsUI::WebUIRenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Enable the JavaScript Mojo bindings in the renderer process, so the JS
  // code can call the Mojo APIs exposed by this WebUI.
  render_frame_host->EnableMojoJsBindings(nullptr);
}

void WebXrInternalsUI::BindInterface(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<webxr::mojom::WebXrInternalsHandler> receiver) {
  ui_handler_ = std::make_unique<WebXrInternalsHandlerImpl>(
      std::move(receiver), web_ui()->GetWebContents());
}

}  // namespace content

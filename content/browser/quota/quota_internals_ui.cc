// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota/quota_internals_ui.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/grit/dev_ui_content_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "storage/browser/quota/quota_internals.mojom.h"

namespace content {

QuotaInternals2UI::QuotaInternals2UI(WebUI* web_ui) : WebUIController(web_ui) {
  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIQuotaInternals2Host);

  source->AddResourcePath("quota_internals.mojom-webui.js",
                          IDR_QUOTA_INTERNALS_MOJOM_JS);
  source->AddResourcePath("quota_internals.js", IDR_QUOTA_INTERNALS_JS);
  source->AddResourcePath("quota-internals-2", IDR_QUOTA_INTERNALS_HTML);
  source->SetDefaultResource(IDR_QUOTA_INTERNALS_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");

  WebContents* web_contents = web_ui->GetWebContents();
  WebUIDataSource::Add(web_contents->GetBrowserContext(), source);
}

WEB_UI_CONTROLLER_TYPE_IMPL(QuotaInternals2UI)

QuotaInternals2UI::~QuotaInternals2UI() = default;

void QuotaInternals2UI::WebUIRenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Enable the JavaScript Mojo bindings in the renderer process, so the JS
  // code can call the Mojo APIs exposed by this WebUI.
  static_cast<RenderFrameHostImpl*>(render_frame_host)->EnableMojoJsBindings();
}

}  // namespace content
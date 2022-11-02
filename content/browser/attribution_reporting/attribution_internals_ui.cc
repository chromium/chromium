// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_ui.h"

#include "content/browser/attribution_reporting/attribution_internals_handler_impl.h"
#include "content/grit/dev_ui_content_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

AttributionInternalsUI::AttributionInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  // Initialize the UI with no bindings. Mojo bindings will be separately
  // granted to frames within this WebContents.
  web_ui->SetBindings(BINDINGS_POLICY_NONE);
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIAttributionInternalsHost);

  source->AddResourcePath("attribution_internals.mojom-webui.js",
                          IDR_ATTRIBUTION_INTERNALS_MOJOM_JS);
  source->AddResourcePath("attribution_internals.js",
                          IDR_ATTRIBUTION_INTERNALS_JS);
  source->AddResourcePath("attribution_internals_table.js",
                          IDR_ATTRIBUTION_INTERNALS_TABLE_JS);
  source->AddResourcePath("attribution_internals_table.html.js",
                          IDR_ATTRIBUTION_INTERNALS_TABLE_HTML_JS);
  source->AddResourcePath("attribution_reporting.mojom-webui.js",
                          IDR_ATTRIBUTION_REPORTING_MOJOM_JS);
  source->AddResourcePath("source_registration_error.mojom-webui.js",
                          IDR_SOURCE_REGISTRATION_ERROR_MOJOM_JS);
  source->AddResourcePath("table_model.js",
                          IDR_ATTRIBUTION_INTERNALS_TABLE_MODEL_JS);
  source->AddResourcePath("attribution_internals.css",
                          IDR_ATTRIBUTION_INTERNALS_CSS);
  source->SetDefaultResource(IDR_ATTRIBUTION_INTERNALS_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

WEB_UI_CONTROLLER_TYPE_IMPL(AttributionInternalsUI)

AttributionInternalsUI::~AttributionInternalsUI() = default;

void AttributionInternalsUI::WebUIRenderFrameCreated(RenderFrameHost* rfh) {
  // Enable the JavaScript Mojo bindings in the renderer process, so the JS
  // code can call the Mojo APIs exposed by this WebUI.
  rfh->EnableMojoJsBindings(nullptr);
}

void AttributionInternalsUI::BindInterface(
    mojo::PendingReceiver<attribution_internals::mojom::Handler> receiver) {
  ui_handler_ = std::make_unique<AttributionInternalsHandlerImpl>(
      web_ui(), std::move(receiver));
}

}  // namespace content

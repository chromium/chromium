// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/traces_internals/traces_internals_ui.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/tracing/traces_internals/traces_internals.mojom.h"
#include "content/browser/tracing/traces_internals/traces_internals_handler.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/grit/traces_internals_resources.h"
#include "content/grit/traces_internals_resources_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"

namespace content {

TracesInternalsUI::TracesInternalsUI(WebUI* web_ui, const GURL& url)
    : WebUIController(web_ui) {
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), url.GetHost());

  source->AddResourcePaths(kTracesInternalsResources);
  source->AddResourcePath("", IDR_TRACES_INTERNALS_TRACES_INTERNALS_HTML);

  // Add TrustedTypes policies necessary for using Polymer.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types polymer-html-literal "
      "polymer-template-event-attribute-policy "
      // Add TrustedTypes policies necessary for using Lit.
      "lit-html-desktop;");
}

TracesInternalsUI::~TracesInternalsUI() = default;

void TracesInternalsUI::WebUIRenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Enable the JavaScript Mojo bindings in the renderer process, so the JS
  // code can call the Mojo APIs exposed by this WebUI.
  render_frame_host->EnableMojoJsBindings(nullptr);
}

void TracesInternalsUI::BindInterface(
    mojo::PendingReceiver<
        traces_internals::mojom::TracesInternalsHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void TracesInternalsUI::CreatePageHandler(
    mojo::PendingRemote<traces_internals::mojom::Page> page,
    mojo::PendingReceiver<traces_internals::mojom::PageHandler> receiver) {
  DCHECK(page);
  ui_handler_ = std::make_unique<TracesInternalsHandler>(std::move(receiver),
                                                         std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(TracesInternalsUI)

}  // namespace content

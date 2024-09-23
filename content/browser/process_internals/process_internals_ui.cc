// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/process_internals/process_internals_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/browser/process_internals/process_internals_handler_impl.h"
#include "content/grit/process_resources.h"
#include "content/grit/process_resources_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

ProcessInternalsUI::ProcessInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  // This WebUI does not require any process bindings, so disable it early in
  // initialization time.
  web_ui->SetBindings(BindingsPolicySet());

  // Create a WebUIDataSource to serve the HTML/JS files to the WebUI.
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIProcessInternalsHost);

  source->AddResourcePaths(
      base::make_span(kProcessResources, kProcessResourcesSize));
  source->SetDefaultResource(IDR_PROCESS_PROCESS_INTERNALS_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

WEB_UI_CONTROLLER_TYPE_IMPL(ProcessInternalsUI)

ProcessInternalsUI::~ProcessInternalsUI() = default;

void ProcessInternalsUI::WebUIRenderFrameCreated(RenderFrameHost* rfh) {
  // Enable the JavaScript Mojo bindings in the renderer process, so the JS
  // code can call the Mojo APIs exposed by this WebUI.
  rfh->EnableMojoJsBindings(nullptr);
}

void ProcessInternalsUI::BindInterface(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<::mojom::ProcessInternalsHandler> receiver) {
  ui_handler_ = std::make_unique<ProcessInternalsHandlerImpl>(
      render_frame_host->GetSiteInstance()->GetBrowserContext(),
      std::move(receiver));
}

}  // namespace content

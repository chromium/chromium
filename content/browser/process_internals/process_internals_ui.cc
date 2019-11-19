// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/process_internals/process_internals_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/browser/process_internals/process_internals_handler_impl.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"

namespace content {

ProcessInternalsUI::ProcessInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui), WebContentsObserver(web_ui->GetWebContents()) {
  // This WebUI does not require any process bindings, so disable it early in
  // initialization time.
  web_ui->SetBindings(0);

  // Create a WebUIDataSource to serve the HTML/JS files to the WebUI.
  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIProcessInternalsHost);

  source->AddResourcePath("process_internals.js", IDR_PROCESS_INTERNALS_JS);
  source->AddResourcePath("process_internals.css", IDR_PROCESS_INTERNALS_CSS);
  source->AddResourcePath("process_internals.mojom-lite.js",
                          IDR_PROCESS_INTERNALS_MOJO_JS);
  source->SetDefaultResource(IDR_PROCESS_INTERNALS_HTML);

  WebUIDataSource::Add(web_contents()->GetBrowserContext(), source);
}

ProcessInternalsUI::~ProcessInternalsUI() = default;

void ProcessInternalsUI::RenderFrameCreated(RenderFrameHost* rfh) {
  // Enable the JavaScript Mojo bindings in the renderer process, so the JS
  // code can call the Mojo APIs exposed by this WebUI.
  static_cast<RenderFrameHostImpl*>(rfh)->EnableMojoJsBindings();
}

void ProcessInternalsUI::BindProcessInternalsHandler(
    mojo::PendingReceiver<::mojom::ProcessInternalsHandler> receiver,
    RenderFrameHost* render_frame_host) {
  ui_handler_ = std::make_unique<ProcessInternalsHandlerImpl>(
      render_frame_host->GetSiteInstance()->GetBrowserContext(),
      std::move(receiver));
}

}  // namespace content

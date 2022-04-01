// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_internals_ui.h"

#include "content/browser/prerender/prerender_internals.mojom.h"
#include "content/browser/prerender/prerender_internals_handler_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/grit/dev_ui_content_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace content {

PrerenderInternalsUI::PrerenderInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://prerender-internals source.
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIPrerenderInternalsHost);

  // Add required resources.
  source->AddResourcePath("prerender_internals.js", IDR_PRERENDER_INTERNALS_JS);
  source->AddResourcePath("prerender_internals.mojom-webui.js",
                          IDR_PRERENDER_INTERNALS_MOJO_JS);
  source->SetDefaultResource(IDR_PRERENDER_INTERNALS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(PrerenderInternalsUI)

PrerenderInternalsUI::~PrerenderInternalsUI() = default;

void PrerenderInternalsUI::WebUIRenderFrameCreated(RenderFrameHost* rfh) {
  rfh->EnableMojoJsBindings(nullptr);
}

void PrerenderInternalsUI::BindPrerenderInternalsHandler(
    mojo::PendingReceiver<mojom::PrerenderInternalsHandler> receiver) {
  ui_handler_ =
      std::make_unique<PrerenderInternalsHandlerImpl>(std::move(receiver));
}

}  // namespace content

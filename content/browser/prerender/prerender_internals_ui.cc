// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_internals_ui.h"

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
  WebUIDataSource* html_source =
      WebUIDataSource::Create(kChromeUIPrerenderInternalsHost);

  // Add required resources.
  html_source->SetDefaultResource(IDR_PRERENDER_INTERNALS_HTML);

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, html_source);
}

PrerenderInternalsUI::~PrerenderInternalsUI() = default;

}  // namespace content

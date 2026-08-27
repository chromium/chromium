// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_ui.h"

#include "content/browser/tracing/grit/tracing_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace content {

TracingUI::TracingUI(WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://tracing/ source.
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUITracingHost);
  source->DisableTrustedTypesCSP();
  source->UseStringsJs();
  source->SetDefaultResource(IDR_TRACING_ABOUT_TRACING_HTML);
  source->AddResourcePath("tracing.js", IDR_TRACING_ABOUT_TRACING_JS);
}

TracingUI::~TracingUI() = default;

}  // namespace content

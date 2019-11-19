// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_ui.h"

#include "base/command_line.h"
#include "content/browser/media/media_internals_handler.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"

namespace content {
namespace {

WebUIDataSource* CreateMediaInternalsHTMLSource() {
  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIMediaInternalsHost);

  source->UseStringsJs();

  if (base::FeatureList::IsEnabled(media::kMediaInspectorLogging)) {
    source->AddResourcePath("media_internals.js",
                            IDR_MEDIA_INTERNALS_JS_DISABLED);
  } else {
    source->AddResourcePath("media_internals.js", IDR_MEDIA_INTERNALS_JS);
  }

  source->SetDefaultResource(IDR_MEDIA_INTERNALS_HTML);
  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// MediaInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

MediaInternalsUI::MediaInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<MediaInternalsMessageHandler>());

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, CreateMediaInternalsHTMLSource());
}

}  // namespace content

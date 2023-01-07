// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_internals_ui.h"

#include "content/browser/webrtc/resources/grit/webrtc_internals_resources.h"
#include "content/browser/webrtc/resources/grit/webrtc_internals_resources_map.h"
#include "content/browser/webrtc/webrtc_internals_message_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace content {
namespace {

WebUIDataSource* CreateWebRTCInternalsHTMLSource() {
  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIWebRTCInternalsHost);

  source->UseStringsJs();
  source->AddResourcePaths(base::make_span(kWebrtcInternalsResources,
                                           kWebrtcInternalsResourcesSize));
  source->SetDefaultResource(IDR_WEBRTC_INTERNALS_WEBRTC_INTERNALS_HTML);
  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// WebRTCInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

WebRTCInternalsUI::WebRTCInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<WebRTCInternalsMessageHandler>());

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, CreateWebRTCInternalsHTMLSource());
}

}  // namespace content

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/connectors_internals/connectors_internals_ui.h"

#import "base/containers/span.h"
#import "components/grit/connectors_internals_resources.h"
#import "components/grit/connectors_internals_resources_map.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/connectors_internals/connectors_internals_page_handler.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

ConnectorsInternalsUI::ConnectorsInternalsUI(web::WebUIIOS* web_ui,
                                             const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIConnectorsInternalsHost);

  source->AddResourcePaths(base::span(kConnectorsInternalsResources));
  source->SetDefaultResource(IDR_CONNECTORS_INTERNALS_INDEX_HTML);

  source->AddBoolean(
      "isOtr", web_ui->GetWebState()->GetBrowserState()->IsOffTheRecord());
  source->UseStringsJs();

  web::WebUIIOSDataSource::Add(web_ui->GetWebState()->GetBrowserState(),
                               source);

  web_ui->GetWebState()->GetInterfaceBinderForMainFrame()->AddInterface(
      base::BindRepeating(&ConnectorsInternalsUI::BindInterface,
                          base::Unretained(this)));
}

ConnectorsInternalsUI::~ConnectorsInternalsUI() {
  web_ui()->GetWebState()->GetInterfaceBinderForMainFrame()->RemoveInterface(
      "connectors_internals.mojom.PageHandler");
}

void ConnectorsInternalsUI::BindInterface(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<ConnectorsInternalsPageHandler>(
      std::move(receiver), ProfileIOS::FromWebUIIOS(web_ui()));
}

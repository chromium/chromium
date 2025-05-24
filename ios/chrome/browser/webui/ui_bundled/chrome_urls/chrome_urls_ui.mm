// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/chrome_urls/chrome_urls_ui.h"

#import "components/grit/chrome_urls_resources.h"
#import "components/grit/chrome_urls_resources_map.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/chrome_urls/chrome_urls_handler.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/webui/resource_path.h"

namespace {
// Creates a WebUI data source for chrome://chrome-urls page.
// Changes to this should be in sync with its non-iOS equivalent
// //chrome/browser/ui/webui/chrome_urls/chrome_urls_ui.cc
web::WebUIIOSDataSource* CreateChromeUrlsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIChromeURLsHost);
  source->AddResourcePaths(kChromeUrlsResources);
  // Necessary on iOS because otherwise URI fragments cause a crash.
  source->SetDefaultResource(IDR_CHROME_URLS_CHROME_URLS_HTML);
  return source;
}
}  // namespace

namespace chrome_urls {

ChromeUrlsUI::ChromeUrlsUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  profile_ = ProfileIOS::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(profile_, CreateChromeUrlsHTMLSource());
  web_ui->GetWebState()->GetInterfaceBinderForMainFrame()->AddInterface(
      base::BindRepeating(&ChromeUrlsUI::BindInterface,
                          base::Unretained(this)));
}

ChromeUrlsUI::~ChromeUrlsUI() {
  web_ui()->GetWebState()->GetInterfaceBinderForMainFrame()->RemoveInterface(
      "chrome_urls.mojom.PageHandlerFactory");
}

void ChromeUrlsUI::BindInterface(
    mojo::PendingReceiver<chrome_urls::mojom::PageHandlerFactory> receiver) {
  // TODO(crbug.com/40215132): Remove the reset which is needed now since `this`
  // is reused on internals page reloads.
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ChromeUrlsUI::CreatePageHandler(
    mojo::PendingRemote<chrome_urls::mojom::Page> page,
    mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<ChromeUrlsHandler>(
      std::move(receiver), std::move(page), profile_);
}

}  // namespace chrome_urls

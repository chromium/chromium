// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/data_sharing_internals/data_sharing_internals_ui.h"

#import "components/data_sharing/data_sharing_internals/webui/data_sharing_internals_page_handler_impl.h"
#import "components/grit/data_sharing_internals_resources.h"
#import "components/grit/data_sharing_internals_resources_map.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_provider.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/webui/resource_path.h"

namespace {

web::WebUIIOSDataSource* CreateDataSharingInternalsUIDataSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIDataSharingInternalsHost);

  source->UseStringsJs();
  for (const auto& resource : kDataSharingInternalsResources) {
    source->AddResourcePath(resource.path, resource.id);
  }
  source->SetDefaultResource(
      IDR_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_HTML);
  return source;
}

}  // namespace

DataSharingInternalsUI::DataSharingInternalsUI(web::WebUIIOS* web_ui,
                                               const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(profile,
                               CreateDataSharingInternalsUIDataSource());
  web_ui->GetWebState()->GetInterfaceBinderForMainFrame()->AddInterface(
      base::BindRepeating(&DataSharingInternalsUI::BindInterface,
                          base::Unretained(this)));
}

DataSharingInternalsUI::~DataSharingInternalsUI() {
  web_ui()->GetWebState()->GetInterfaceBinderForMainFrame()->RemoveInterface(
      "data_sharing_internals.mojom.PageHandlerFactory");
}

void DataSharingInternalsUI::BindInterface(
    mojo::PendingReceiver<data_sharing_internals::mojom::PageHandlerFactory>
        receiver) {
  data_sharing_internals_page_factory_receiver_.reset();
  data_sharing_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void DataSharingInternalsUI::CreatePageHandler(
    mojo::PendingRemote<data_sharing_internals::mojom::Page> page,
    mojo::PendingReceiver<data_sharing_internals::mojom::PageHandler>
        receiver) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());

  data_sharing_internals_page_handler_ =
      std::make_unique<DataSharingInternalsPageHandlerImpl>(
          std::move(receiver), std::move(page),
          data_sharing::DataSharingServiceFactory::GetForProfile(profile));
}

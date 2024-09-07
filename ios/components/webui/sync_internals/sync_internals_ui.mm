// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/webui/sync_internals/sync_internals_ui.h"

#import <memory>

#import "components/grit/sync_service_sync_internals_resources.h"
#import "components/grit/sync_service_sync_internals_resources_map.h"
#import "components/sync/service/sync_internals_util.h"
#import "ios/components/webui/sync_internals/ios_sync_internals_message_handler.h"
#import "ios/components/webui/web_ui_provider.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/webui/resource_path.h"

namespace {

web::WebUIIOSDataSource* CreateSyncInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUISyncInternalsHost);

  source->UseStringsJs();
  for (size_t i = 0; i < kSyncServiceSyncInternalsResourcesSize; i++) {
    const webui::ResourcePath path = kSyncServiceSyncInternalsResources[i];
    source->AddResourcePath(path.path, path.id);
  }
  source->SetDefaultResource(IDR_SYNC_SERVICE_SYNC_INTERNALS_INDEX_HTML);
  return source;
}

}  // namespace

SyncInternalsUI::SyncInternalsUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web::WebUIIOSDataSource::Add(web_ui->GetWebState()->GetBrowserState(),
                               CreateSyncInternalsHTMLSource());
  web_ui->AddMessageHandler(std::make_unique<IOSSyncInternalsMessageHandler>(
      web_ui::GetIdentityManagerForWebUI(web_ui),
      web_ui::GetSyncServiceForWebUI(web_ui),
      web_ui::GetSyncInvalidationsServiceForWebUI(web_ui),
      web_ui::GetUserEventServiceForWebUI(web_ui), web_ui::GetChannelString()));
}

SyncInternalsUI::~SyncInternalsUI() {}

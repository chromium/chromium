// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/webui/sync_internals/sync_internals_ui.h"

#import <memory>

#import "components/grit/sync_driver_sync_internals_resources.h"
#import "components/grit/sync_driver_sync_internals_resources_map.h"
#import "components/sync/driver/sync_internals_util.h"
#import "ios/components/webui/sync_internals/sync_internals_message_handler.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/webui/resource_path.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

web::WebUIIOSDataSource* CreateSyncInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUISyncInternalsHost);

  source->UseStringsJs();
  for (size_t i = 0; i < kSyncDriverSyncInternalsResourcesSize; i++) {
    const webui::ResourcePath path = kSyncDriverSyncInternalsResources[i];
    source->AddResourcePath(path.path, path.id);
  }
  source->SetDefaultResource(IDR_SYNC_DRIVER_SYNC_INTERNALS_INDEX_HTML);
  return source;
}

}  // namespace

SyncInternalsUI::SyncInternalsUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web::WebUIIOSDataSource::Add(web_ui->GetWebState()->GetBrowserState(),
                               CreateSyncInternalsHTMLSource());
  web_ui->AddMessageHandler(std::make_unique<SyncInternalsMessageHandler>());
}

SyncInternalsUI::~SyncInternalsUI() {}

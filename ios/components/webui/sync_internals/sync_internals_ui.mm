// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/webui/sync_internals/sync_internals_ui.h"

#include <memory>

#include "components/grit/sync_driver_resources.h"
#include "components/sync/driver/sync_internals_util.h"
#include "ios/components/webui/sync_internals/sync_internals_message_handler.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_state.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

web::WebUIIOSDataSource* CreateSyncInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUISyncInternalsHost);

  source->UseStringsJs();
  source->AddResourcePath(syncer::sync_ui_util::kSyncIndexJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_INDEX_JS);
  source->AddResourcePath(syncer::sync_ui_util::kChromeSyncJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_CHROME_SYNC_JS);
  source->AddResourcePath(syncer::sync_ui_util::kSyncLogJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_SYNC_LOG_JS);
  source->AddResourcePath(syncer::sync_ui_util::kSyncNodeBrowserJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_SYNC_NODE_BROWSER_JS);
  source->AddResourcePath(syncer::sync_ui_util::kSyncSearchJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_SYNC_SEARCH_JS);
  source->AddResourcePath(syncer::sync_ui_util::kAboutJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_ABOUT_JS);
  source->AddResourcePath(syncer::sync_ui_util::kDataJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_DATA_JS);
  source->AddResourcePath(syncer::sync_ui_util::kEventsJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_EVENTS_JS);
  source->AddResourcePath(syncer::sync_ui_util::kSearchJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_SEARCH_JS);
  source->AddResourcePath(syncer::sync_ui_util::kUserEventsJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_USER_EVENTS_JS);
  source->AddResourcePath(syncer::sync_ui_util::kTrafficLogJS,
                          IDR_SYNC_DRIVER_SYNC_INTERNALS_TRAFFIC_LOG_JS);
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

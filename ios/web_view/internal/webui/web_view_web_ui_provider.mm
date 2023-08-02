// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/webui/web_ui_provider.h"

#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/version_info/channel.h"
#include "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"
#include "ios/web_view/internal/sync/web_view_sync_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#include "ios/components/webui/web_ui_provider.h"

namespace web_ui {

syncer::SyncService* GetSyncServiceForWebUI(web::WebUIIOS* web_ui) {
  ios_web_view::WebViewBrowserState* browser_state =
      ios_web_view::WebViewBrowserState::FromWebUIIOS(web_ui);
  return ios_web_view::WebViewSyncServiceFactory::GetForBrowserState(
      browser_state->GetRecordingBrowserState());
}

syncer::SyncInvalidationsService* GetSyncInvalidationsServiceForWebUI(
    web::WebUIIOS* web_ui) {
  ios_web_view::WebViewBrowserState* browser_state =
      ios_web_view::WebViewBrowserState::FromWebUIIOS(web_ui);
  return ios_web_view::WebViewSyncInvalidationsServiceFactory::
      GetForBrowserState(browser_state->GetRecordingBrowserState());
}

std::string GetChannelString() {
  return std::string();
}

version_info::Channel GetChannel() {
  return version_info::Channel::STABLE;
}

}  // namespace web_ui

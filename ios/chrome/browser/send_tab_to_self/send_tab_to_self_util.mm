// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/web_state.h"

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace send_tab_to_self {

bool IsUserSyncTypeActive(ChromeBrowserState* browser_state) {
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state);
  // The service will be null if the user is in incognito mode so better to
  // check for that.
  return service && service->GetSendTabToSelfModel() &&
         service->GetSendTabToSelfModel()->IsReady();
}

bool HasValidTargetDevice(ChromeBrowserState* browser_state) {
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state);
  return service && service->GetSendTabToSelfModel() &&
         service->GetSendTabToSelfModel()->HasValidTargetDevice();
}

bool AreContentRequirementsMet(const GURL& url,
                               ChromeBrowserState* browser_state) {
  bool is_http_or_https = url.SchemeIsHTTPOrHTTPS();
  bool is_native_page = url.SchemeIs(kChromeUIScheme);
  bool is_incognito_mode = browser_state->IsOffTheRecord();
  return is_http_or_https && !is_native_page && !is_incognito_mode;
}

bool ShouldOfferFeature(ChromeBrowserState* browser_state, const GURL& url) {
  return IsUserSyncTypeActive(browser_state) &&
         HasValidTargetDevice(browser_state) &&
         AreContentRequirementsMet(url, browser_state);
}

}  // namespace send_tab_to_self

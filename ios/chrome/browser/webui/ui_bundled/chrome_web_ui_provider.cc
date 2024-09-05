// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/sync_invalidations_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"
#include "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#include "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"
#include "ios/chrome/browser/sync/model/sync_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/components/webui/web_ui_provider.h"

namespace web_ui {

signin::IdentityManager* GetIdentityManagerForWebUI(web::WebUIIOS* web_ui) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  return IdentityManagerFactory::GetForProfile(
      browser_state->GetOriginalChromeBrowserState());
}

syncer::SyncService* GetSyncServiceForWebUI(web::WebUIIOS* web_ui) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  return SyncServiceFactory::GetForBrowserState(
      browser_state->GetOriginalChromeBrowserState());
}

syncer::SyncInvalidationsService* GetSyncInvalidationsServiceForWebUI(
    web::WebUIIOS* web_ui) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  return SyncInvalidationsServiceFactory::GetForBrowserState(
      browser_state->GetOriginalChromeBrowserState());
}

syncer::UserEventService* GetUserEventServiceForWebUI(web::WebUIIOS* web_ui) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  return IOSUserEventServiceFactory::GetForBrowserState(
      browser_state->GetOriginalChromeBrowserState());
}

std::string GetChannelString() {
  return ::GetChannelString();
}

version_info::Channel GetChannel() {
  return ::GetChannel();
}

}  // namespace web_ui

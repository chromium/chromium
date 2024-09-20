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
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  return IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile());
}

syncer::SyncService* GetSyncServiceForWebUI(web::WebUIIOS* web_ui) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  return SyncServiceFactory::GetForBrowserState(profile->GetOriginalProfile());
}

syncer::SyncInvalidationsService* GetSyncInvalidationsServiceForWebUI(
    web::WebUIIOS* web_ui) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  return SyncInvalidationsServiceFactory::GetForProfile(
      profile->GetOriginalProfile());
}

syncer::UserEventService* GetUserEventServiceForWebUI(web::WebUIIOS* web_ui) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  return IOSUserEventServiceFactory::GetForProfile(
      profile->GetOriginalProfile());
}

std::string GetChannelString() {
  return ::GetChannelString();
}

version_info::Channel GetChannel() {
  return ::GetChannel();
}

}  // namespace web_ui

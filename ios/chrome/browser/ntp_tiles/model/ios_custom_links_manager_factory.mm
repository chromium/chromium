// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/model/ios_custom_links_manager_factory.h"

#import "components/keyed_service/core/service_access_type.h"
#import "components/ntp_tiles/custom_links_manager_impl.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

std::unique_ptr<ntp_tiles::CustomLinksManager>
IOSCustomLinksManagerFactory::NewForProfile(ProfileIOS* profile) {
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  return std::make_unique<ntp_tiles::CustomLinksManagerImpl>(
      profile->GetPrefs(), history_service);
}

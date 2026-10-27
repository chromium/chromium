// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/model/ios_custom_links_manager_factory.h"

#import "components/ntp_tiles/custom_links_manager_impl.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

std::unique_ptr<ntp_tiles::CustomLinksManager>
IOSCustomLinksManagerFactory::NewForProfile(ProfileIOS* profile) {
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(profile);
  // iOS shows the mobile-sized pinned tile grid, which keeps its links in the
  // mobile storage domain, isolated from desktop shortcuts.
  return std::make_unique<ntp_tiles::CustomLinksManagerImpl>(
      ntp_tiles::CustomLinksManagerImpl::Options{
          .prefs = profile->GetPrefs(),
          .history_service = history_service,
          .scope = ntp_tiles::CustomLinksScope::kMobile,
      });
}

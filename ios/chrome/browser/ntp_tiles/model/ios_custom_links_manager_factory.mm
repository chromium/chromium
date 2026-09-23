// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/model/ios_custom_links_manager_factory.h"

#import "components/keyed_service/core/service_access_type.h"
#import "components/ntp_tiles/custom_links_manager_impl.h"
#import "components/ntp_tiles/features.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

std::unique_ptr<ntp_tiles::CustomLinksManager>
IOSCustomLinksManagerFactory::NewForProfile(ProfileIOS* profile) {
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  bool enable_ai_mode_tile =
      IsAimEnabledInNtp() && ntp_tiles::GetAimButtonRefactorArm() ==
                                 ntp_tiles::AimButtonRefactorArm::kAimAsMvt;
  size_t max_links = enable_ai_mode_tile ? ntp_tiles::kMaxNumCustomLinks + 1
                                         : ntp_tiles::kMaxNumCustomLinks;
  // iOS shows the mobile-sized pinned tile grid, which keeps its links in the
  // mobile storage domain, isolated from desktop shortcuts.
  return std::make_unique<ntp_tiles::CustomLinksManagerImpl>(
      ntp_tiles::CustomLinksManagerImpl::Options{
          .prefs = profile->GetPrefs(),
          .history_service = history_service,
          .max_links = max_links,
          .enable_ai_mode_tile = enable_ai_mode_tile,
          .scope = ntp_tiles::CustomLinksScope::kMobile,
      });
}

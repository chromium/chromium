// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_family_link_user_metrics_provider.h"

#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

IOSFamilyLinkUserMetricsProvider::IOSFamilyLinkUserMetricsProvider() = default;
IOSFamilyLinkUserMetricsProvider::~IOSFamilyLinkUserMetricsProvider() = default;

bool IOSFamilyLinkUserMetricsProvider::ProvideHistograms() {
  std::vector<ChromeBrowserState*> browser_state_list =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  std::vector<supervised_user::LogSegment> log_segments;
  for (ChromeBrowserState* browser_state : browser_state_list) {
    absl::optional<supervised_user::LogSegment> log_segment =
        supervised_user::SupervisionStatusForUser(
            IdentityManagerFactory::GetForBrowserState(browser_state));
    if (log_segment.has_value()) {
      log_segments.push_back(log_segment.value());
    }
  }
  return supervised_user::EmitLogSegmentHistogram(log_segments);
}

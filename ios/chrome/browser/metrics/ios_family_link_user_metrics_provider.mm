// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_family_link_user_metrics_provider.h"

#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"

IOSFamilyLinkUserMetricsProvider::IOSFamilyLinkUserMetricsProvider() = default;
IOSFamilyLinkUserMetricsProvider::~IOSFamilyLinkUserMetricsProvider() = default;

bool IOSFamilyLinkUserMetricsProvider::ProvideHistograms() {
  std::vector<ChromeBrowserState*> browser_state_list =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  std::vector<AccountInfo> primary_accounts;
  for (ChromeBrowserState* browser_state : browser_state_list) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForBrowserState(browser_state);
    AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
    primary_accounts.push_back(std::move(account_info));
  }
  return supervised_user::EmitLogSegmentHistogram(primary_accounts);
}

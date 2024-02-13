// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_family_link_user_metrics_provider.h"

#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/supervised_user/core/browser/family_link_user_log_record.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

IOSFamilyLinkUserMetricsProvider::IOSFamilyLinkUserMetricsProvider() = default;
IOSFamilyLinkUserMetricsProvider::~IOSFamilyLinkUserMetricsProvider() = default;

bool IOSFamilyLinkUserMetricsProvider::ProvideHistograms() {
  std::vector<ChromeBrowserState*> browser_state_list =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  std::vector<supervised_user::FamilyLinkUserLogRecord> records;
  for (ChromeBrowserState* browser_state : browser_state_list) {
    supervised_user::SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForBrowserState(browser_state);
    records.push_back(supervised_user::FamilyLinkUserLogRecord::Create(
        IdentityManagerFactory::GetForBrowserState(browser_state),
        service ? service->GetURLFilter() : nullptr));
  }
  return supervised_user::EmitLogRecordHistograms(records);
}

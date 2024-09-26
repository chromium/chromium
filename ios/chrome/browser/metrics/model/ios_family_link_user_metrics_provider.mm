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
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

IOSFamilyLinkUserMetricsProvider::IOSFamilyLinkUserMetricsProvider() = default;
IOSFamilyLinkUserMetricsProvider::~IOSFamilyLinkUserMetricsProvider() = default;

bool IOSFamilyLinkUserMetricsProvider::ProvideHistograms() {
  std::vector<supervised_user::FamilyLinkUserLogRecord> records;
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    supervised_user::SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    records.push_back(supervised_user::FamilyLinkUserLogRecord::Create(
        IdentityManagerFactory::GetForProfile(profile), *profile->GetPrefs(),
        *ios::HostContentSettingsMapFactory::GetForProfile(profile),
        service ? service->GetURLFilter() : nullptr));
  }
  return supervised_user::EmitLogRecordHistograms(records);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_reporting_controller_factory_ios.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/enterprise/browser/reporting/pref_url_list_matcher.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_reporting {

// static
SaasUsageReportingController*
SaasUsageReportingControllerFactoryIOS::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<SaasUsageReportingController>(
      profile, /*create=*/true);
}

// static
SaasUsageReportingControllerFactoryIOS*
SaasUsageReportingControllerFactoryIOS::GetInstance() {
  static base::NoDestructor<SaasUsageReportingControllerFactoryIOS> instance;
  return instance.get();
}

SaasUsageReportingControllerFactoryIOS::SaasUsageReportingControllerFactoryIOS()
    : ProfileKeyedServiceFactoryIOS("SaasUsageReportingController",
                                    ProfileSelection::kNoInstanceInIncognito) {}

SaasUsageReportingControllerFactoryIOS::
    ~SaasUsageReportingControllerFactoryIOS() = default;

std::unique_ptr<KeyedService>
SaasUsageReportingControllerFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  PrefService* profile_prefs = profile->GetPrefs();
  return std::make_unique<SaasUsageReportingController>(
      local_state, profile_prefs,
      std::make_unique<PrefURLListMatcher>(local_state,
                                           kSaasUsageDomainUrlsForBrowser),
      std::make_unique<PrefURLListMatcher>(profile_prefs,
                                           kSaasUsageDomainUrlsForProfile));
}

}  // namespace enterprise_reporting

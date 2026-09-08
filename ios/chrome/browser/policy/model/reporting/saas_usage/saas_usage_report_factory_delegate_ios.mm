// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_report_factory_delegate_ios.h"

#import "base/check.h"
#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_reporting {

SaasUsageReportFactoryDelegateIOS::SaasUsageReportFactoryDelegateIOS(
    ProfileIOS* profile)
    : profile_(profile) {}

std::optional<std::string> SaasUsageReportFactoryDelegateIOS::GetProfileId() {
  if (!profile_) {
    return std::nullopt;
  }
  auto* profile_id_service =
      enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile_);
  CHECK(profile_id_service)
      << "Saas usage report is only produced for regular profiles"
         ", so profile id service should always be present.";

  return profile_id_service->GetProfileId();
}

bool SaasUsageReportFactoryDelegateIOS::IsProfileAffiliated() {
  if (!profile_) {
    return false;
  }
  return enterprise_reporting::IsProfileAffiliated(profile_);
}

}  // namespace enterprise_reporting

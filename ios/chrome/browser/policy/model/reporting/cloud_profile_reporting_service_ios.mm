// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_ios.h"

#import <string>
#import <utility>

#import "base/feature_list.h"
#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/enterprise/browser/reporting/report_scheduler.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_reporting {

CloudProfileReportingServiceIOS::CloudProfileReportingServiceIOS(
    ProfileIOS* profile)
    : profile_(profile) {
  Init();
}

CloudProfileReportingServiceIOS::~CloudProfileReportingServiceIOS() = default;

void CloudProfileReportingServiceIOS::CreateReportScheduler() {
  std::string profile_id = "";
  if (enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile_)) {
    profile_id = enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile_)
                     ->GetProfileId()
                     .value_or("");
  }
  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      profile_id,
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->device_management_service(),
      profile_->GetSharedURLLoaderFactory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());

  ReportingDelegateFactoryIOS delegate_factory;
  ReportScheduler::CreateParams params;
  params.client = cloud_policy_client_.get();
  params.delegate = delegate_factory.GetReportSchedulerDelegate(profile_);

  // Only start scheduling reports if kPoliciesEverFetchedWithProfileId is true
  // or when it flips to true.
  params.require_policy_fetch_with_profile_id = true;

  params.profile_request_generator =
      std::make_unique<ChromeProfileRequestGenerator>(
          base::FilePath(SanitizeProfilePath(profile_->GetProfileName())),
          &delegate_factory,
          /*signals_aggregator=*/nullptr);
  report_scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
}

void CloudProfileReportingServiceIOS::Init() {
  if (!base::FeatureList::IsEnabled(kCloudProfileReporting)) {
    return;
  }
  CreateReportScheduler();
}

}  // namespace enterprise_reporting

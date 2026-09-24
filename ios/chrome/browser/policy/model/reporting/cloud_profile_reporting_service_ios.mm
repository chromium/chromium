// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_ios.h"

#import <string>
#import <utility>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/enterprise/browser/reporting/report_scheduler.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_util.h"
#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_reporting {

CloudProfileReportingServiceIOS::CloudProfileReportingServiceIOS(
    enterprise::ProfileIdService* profile_id_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string_view profile_name,
    std::unique_ptr<ReportScheduler::Delegate> report_scheduler_delegate,
    device_signals::SignalsAggregator* signals_aggregator,
    std::unique_ptr<SaasUsageReportScheduler> saas_usage_report_scheduler)
    : saas_usage_report_scheduler_(std::move(saas_usage_report_scheduler)) {
  if (!base::FeatureList::IsEnabled(kCloudProfileReporting)) {
    return;
  }
  std::string profile_id = "";
  if (profile_id_service) {
    profile_id = profile_id_service->GetProfileId().value_or("");
  }
  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      profile_id,
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->device_management_service(),
      std::move(url_loader_factory),
      policy::CloudPolicyClient::DeviceDMTokenCallback());

  ReportingDelegateFactoryIOS delegate_factory;
  ReportScheduler::CreateParams params;
  params.client = cloud_policy_client_.get();
  params.delegate = std::move(report_scheduler_delegate);

  // Only start scheduling reports if kPoliciesEverFetchedWithProfileId is
  // true or when it flips to true.
  params.require_policy_fetch_with_profile_id = true;
  params.profile_request_generator =
      std::make_unique<ChromeProfileRequestGenerator>(
          base::FilePath(SanitizeProfilePath(profile_name)), &delegate_factory,
          signals_aggregator);
  report_scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
}

CloudProfileReportingServiceIOS::~CloudProfileReportingServiceIOS() = default;

}  // namespace enterprise_reporting

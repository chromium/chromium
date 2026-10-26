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
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
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
    std::unique_ptr<SaasUsageReportScheduler> saas_usage_report_scheduler,
    policy::UserCloudPolicyManager* user_cloud_policy_manager)
    : saas_usage_report_scheduler_(std::move(saas_usage_report_scheduler)),
      profile_id_service_(profile_id_service),
      signals_aggregator_(signals_aggregator),
      url_loader_factory_(std::move(url_loader_factory)),
      profile_name_(profile_name),
      report_scheduler_delegate_(std::move(report_scheduler_delegate)) {
  Init(user_cloud_policy_manager);
}

CloudProfileReportingServiceIOS::~CloudProfileReportingServiceIOS() = default;

void CloudProfileReportingServiceIOS::Shutdown() {
  store_observation_.Reset();

  saas_usage_report_scheduler_.reset();
  report_scheduler_.reset();
  cloud_policy_client_.reset();

  // Drop every reference to objects owned by the profile or by other
  // KeyedServices: none of them may be used past this point, and some of them
  // will be destroyed before this object is.
  report_scheduler_delegate_.reset();
  url_loader_factory_.reset();
  profile_id_service_ = nullptr;
  signals_aggregator_ = nullptr;
}

void CloudProfileReportingServiceIOS::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  if (!report_scheduler_) {
    CreateReportScheduler();
    return;
  }
  report_scheduler_->OnDMTokenUpdated();
}

void CloudProfileReportingServiceIOS::OnStoreError(
    policy::CloudPolicyStore* store) {
  if (!report_scheduler_) {
    CreateReportScheduler();
  }
}

void CloudProfileReportingServiceIOS::OnStoreDestruction(
    policy::CloudPolicyStore* store) {
  store_observation_.Reset();
}

void CloudProfileReportingServiceIOS::Init(
    policy::UserCloudPolicyManager* user_cloud_policy_manager) {
  if (!base::FeatureList::IsEnabled(kCloudProfileReporting)) {
    return;
  }

  policy::CloudPolicyStore* store =
      user_cloud_policy_manager && user_cloud_policy_manager->core()
          ? user_cloud_policy_manager->core()->store()
          : nullptr;
  if (!store) {
    CreateReportScheduler();
    return;
  }

  store_observation_.Observe(store);
  if (store->is_initialized()) {
    CreateReportScheduler();
  }
}

void CloudProfileReportingServiceIOS::CreateReportScheduler() {
  CHECK(!report_scheduler_);

  std::string profile_id = "";
  if (profile_id_service_) {
    profile_id = profile_id_service_->GetProfileId().value_or("");
  }
  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      profile_id,
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->device_management_service(),
      url_loader_factory_, policy::CloudPolicyClient::DeviceDMTokenCallback());

  ReportingDelegateFactoryIOS delegate_factory;
  ReportScheduler::CreateParams params;
  params.client = cloud_policy_client_.get();
  params.delegate = std::move(report_scheduler_delegate_);

  // Only start scheduling reports if kPoliciesEverFetchedWithProfileId is
  // true or when it flips to true.
  params.require_policy_fetch_with_profile_id = true;
  params.profile_request_generator =
      std::make_unique<ChromeProfileRequestGenerator>(
          base::FilePath(SanitizeProfilePath(profile_name_)), &delegate_factory,
          signals_aggregator_);
  report_scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
}

}  // namespace enterprise_reporting

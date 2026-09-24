// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_factory_ios.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#import "components/enterprise/device_attestation/device_attestation_service_factory.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/signals/model/ios_signals_aggregator_factory.h"
#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_ios.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/public/provider/chrome/browser/device_attestation/device_attestation_api.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_reporting {

namespace {

std::unique_ptr<SaasUsageReportScheduler> CreateSaasUsageReportScheduler(
    ProfileIOS* profile) {
  if (!base::FeatureList::IsEnabled(kSaasUsageReporting)) {
    return nullptr;
  }
  auto saas_usage_reporting_delegate_factory =
      SaasUsageReportingDelegateFactoryIOS::CreateForProfile(profile);
  return SaasUsageReportScheduler::Create(
      "profile", saas_usage_reporting_delegate_factory.get());
}

}  // namespace

// static
CloudProfileReportingServiceFactoryIOS*
CloudProfileReportingServiceFactoryIOS::GetInstance() {
  static base::NoDestructor<CloudProfileReportingServiceFactoryIOS> instance;
  return instance.get();
}

// static
CloudProfileReportingServiceIOS*
CloudProfileReportingServiceFactoryIOS::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<CloudProfileReportingServiceIOS>(
      profile, /*create=*/true);
}

std::unique_ptr<KeyedService>
CloudProfileReportingServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(kCloudProfileReporting)) {
    return nullptr;
  }
  ReportingDelegateFactoryIOS delegate_factory;
  return std::make_unique<CloudProfileReportingServiceIOS>(
      enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(), profile->GetProfileName(),
      delegate_factory.GetReportSchedulerDelegate(profile),
      IOSSignalsAggregatorFactory::GetForProfile(profile),
      CreateSaasUsageReportScheduler(profile));
}

CloudProfileReportingServiceFactoryIOS::CloudProfileReportingServiceFactoryIOS()
    : ProfileKeyedServiceFactoryIOS("CloudProfileReportingService",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(enterprise::ProfileIdServiceFactoryIOS::GetInstance());
  DependsOn(IOSSignalsAggregatorFactory::GetInstance());
  enterprise::DeviceAttestationServiceFactory::SetAttestationServiceIOSProvider(
      base::BindRepeating(&ios::provider::CreateAttestationServiceIOS));
}

CloudProfileReportingServiceFactoryIOS::
    ~CloudProfileReportingServiceFactoryIOS() = default;

}  // namespace enterprise_reporting

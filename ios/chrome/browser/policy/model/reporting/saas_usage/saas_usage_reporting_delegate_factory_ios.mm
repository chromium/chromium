// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_reporting_delegate_factory_ios.h"

#import <memory>

#import "base/memory/ptr_util.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_report_factory_delegate_ios.h"
#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_report_scheduler_delegate_ios.h"
#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_report_uploader_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_reporting {

// static
std::unique_ptr<SaasUsageReportingDelegateFactoryIOS>
SaasUsageReportingDelegateFactoryIOS::CreateForBrowser() {
  return base::WrapUnique(new SaasUsageReportingDelegateFactoryIOS(nullptr));
}

// static
std::unique_ptr<SaasUsageReportingDelegateFactoryIOS>
SaasUsageReportingDelegateFactoryIOS::CreateForProfile(ProfileIOS* profile) {
  return base::WrapUnique(new SaasUsageReportingDelegateFactoryIOS(profile));
}

SaasUsageReportingDelegateFactoryIOS::SaasUsageReportingDelegateFactoryIOS(
    ProfileIOS* profile)
    : profile_(profile) {}

SaasUsageReportingDelegateFactoryIOS::~SaasUsageReportingDelegateFactoryIOS() =
    default;

PrefService* SaasUsageReportingDelegateFactoryIOS::GetPrefService() const {
  return profile_ ? profile_->GetPrefs()
                  : GetApplicationContext()->GetLocalState();
}

std::unique_ptr<SaasUsageReportFactory::Delegate>
SaasUsageReportingDelegateFactoryIOS::GetSaasUsageReportFactoryDelegate()
    const {
  return std::make_unique<SaasUsageReportFactoryDelegateIOS>(profile_);
}

std::unique_ptr<SaasUsageReportUploader>
SaasUsageReportingDelegateFactoryIOS::GetSaasUsageReportUploader() const {
  return profile_ ? std::make_unique<SaasUsageReportUploaderIOS>(profile_)
                  : std::make_unique<SaasUsageReportUploaderIOS>();
}

std::unique_ptr<SaasUsageReportScheduler::Delegate>
SaasUsageReportingDelegateFactoryIOS::GetSaasUsageReportSchedulerDelegate()
    const {
  return profile_ ? nullptr
                  : std::make_unique<SaasUsageReportSchedulerDelegateIOS>();
}

}  // namespace enterprise_reporting

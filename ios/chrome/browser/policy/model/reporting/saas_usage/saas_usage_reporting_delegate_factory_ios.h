// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_IOS_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_delegate_factory.h"

class ProfileIOS;

namespace enterprise_reporting {

// Implementation of SaasUsageReportingDelegateFactory for iOS.
class SaasUsageReportingDelegateFactoryIOS
    : public SaasUsageReportingDelegateFactory {
 public:
  static std::unique_ptr<SaasUsageReportingDelegateFactoryIOS>
  CreateForBrowser();
  static std::unique_ptr<SaasUsageReportingDelegateFactoryIOS> CreateForProfile(
      ProfileIOS* profile);

  SaasUsageReportingDelegateFactoryIOS(
      const SaasUsageReportingDelegateFactoryIOS&) = delete;
  SaasUsageReportingDelegateFactoryIOS& operator=(
      const SaasUsageReportingDelegateFactoryIOS&) = delete;
  ~SaasUsageReportingDelegateFactoryIOS() override;

  // SaasUsageReportingDelegateFactory implementation:
  PrefService* GetPrefService() const override;

  std::unique_ptr<SaasUsageReportFactory::Delegate>
  GetSaasUsageReportFactoryDelegate() const override;

  std::unique_ptr<SaasUsageReportUploader> GetSaasUsageReportUploader()
      const override;

  std::unique_ptr<SaasUsageReportScheduler::Delegate>
  GetSaasUsageReportSchedulerDelegate() const override;

 private:
  explicit SaasUsageReportingDelegateFactoryIOS(ProfileIOS* profile);

  // `profile_` is null for browser-level reporting.
  raw_ptr<ProfileIOS> profile_ = nullptr;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_IOS_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_FACTORY_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_FACTORY_DELEGATE_IOS_H_

#import <optional>
#import <string>

#import "base/memory/raw_ptr.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"

class ProfileIOS;

namespace enterprise_reporting {

// Implementation of SaasUsageReportFactory::Delegate for iOS.
class SaasUsageReportFactoryDelegateIOS final
    : public SaasUsageReportFactory::Delegate {
 public:
  explicit SaasUsageReportFactoryDelegateIOS(ProfileIOS* profile);
  SaasUsageReportFactoryDelegateIOS(const SaasUsageReportFactoryDelegateIOS&) =
      delete;
  SaasUsageReportFactoryDelegateIOS& operator=(
      const SaasUsageReportFactoryDelegateIOS&) = delete;

  ~SaasUsageReportFactoryDelegateIOS() override = default;

  // SaasUsageReportFactory::Delegate:
  std::optional<std::string> GetProfileId() override;
  bool IsProfileAffiliated() override;

 private:
  // `profile_` is null for browser-level factory and non-null for
  // profile-level factory.
  raw_ptr<ProfileIOS> profile_;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_FACTORY_DELEGATE_IOS_H_

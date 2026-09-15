// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"

class ProfileIOS;

namespace enterprise_reporting {

// Implementation of SaasUsageReportUploader for iOS.
// Encapsulates the logic for wrapping the feature proto into a generic event.
class SaasUsageReportUploaderIOS final : public SaasUsageReportUploader {
 public:
  // Browser-level uploader constructor.
  SaasUsageReportUploaderIOS();
  // Profile-level uploader constructor.
  explicit SaasUsageReportUploaderIOS(ProfileIOS* profile);

  SaasUsageReportUploaderIOS(const SaasUsageReportUploaderIOS&) = delete;
  SaasUsageReportUploaderIOS& operator=(const SaasUsageReportUploaderIOS&) =
      delete;

  ~SaasUsageReportUploaderIOS() override;

  // SaasUsageReportUploader:
  void UploadReport(
      const ::chrome::cros::reporting::proto::SaasUsageReportEvent& report,
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>
          upload_callback) override;

 private:
  // `profile_` is null for browser-level uploader and non-null for
  // profile-level uploader.
  raw_ptr<ProfileIOS> profile_;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_IOS_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_report_uploader_ios.h"

#import <optional>
#import <string>
#import <utility>

#import "base/check.h"
#import "base/functional/callback.h"
#import "components/enterprise/common/proto/synced/saas_usage_report_event.pb.h"
#import "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
#import "components/policy/core/common/policy_logger.h"
#import "ios/chrome/browser/enterprise/common/util.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace enterprise_reporting {

namespace {

enterprise_connectors::IOSRealtimeReportingClient* GetReportingClient(
    ProfileIOS* profile) {
  if (profile) {
    return enterprise_connectors::IOSRealtimeReportingClientFactory::
        GetForProfile(profile);
  }

  ApplicationContext* context = GetApplicationContext();
  if (!context || !context->GetProfileManager()) {
    return nullptr;
  }

  // TODO(crbug.com/527894269): RealtimeReportingClient is currently a
  // KeyedService and we retrieve a client from an arbitrary profile for
  // browser-level reporting. This is not ideal. We should refactor it to have a
  // browser-wide instance.
  for (ProfileIOS* loaded_profile :
       context->GetProfileManager()->GetLoadedProfiles()) {
    auto* client =
        enterprise_connectors::IOSRealtimeReportingClientFactory::GetForProfile(
            loaded_profile);
    if (client) {
      return client;
    }
  }
  return nullptr;
}

std::optional<std::string> GetDMToken(ProfileIOS* profile) {
  return profile
             ? enterprise::GetUserDmToken(profile->GetUserCloudPolicyManager())
             : enterprise::GetBrowserDmToken();
}

}  // namespace

SaasUsageReportUploaderIOS::SaasUsageReportUploaderIOS() : profile_(nullptr) {}

SaasUsageReportUploaderIOS::SaasUsageReportUploaderIOS(ProfileIOS* profile)
    : profile_(profile) {
  CHECK(profile_);
}

SaasUsageReportUploaderIOS::~SaasUsageReportUploaderIOS() = default;

void SaasUsageReportUploaderIOS::UploadReport(
    const ::chrome::cros::reporting::proto::SaasUsageReportEvent& report,
    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
        upload_callback) {
  bool per_profile =
      profile_ && !enterprise_reporting::IsProfileAffiliated(profile_);
  enterprise_connectors::IOSRealtimeReportingClient* client =
      GetReportingClient(profile_);
  if (!client) {
    LOG_POLICY(ERROR, REPORTING)
        << "No real time reporting client found for SaaS usage report upload.";
    return;
  }

  std::optional<std::string> dm_token =
      GetDMToken(per_profile ? profile_ : nullptr);
  if (!dm_token || dm_token->empty()) {
    LOG_POLICY(ERROR, REPORTING)
        << "No DM token found for SaaS usage report upload.";
    return;
  }

  VLOG_POLICY(1, REPORTING)
      << "Sending " << (profile_ ? "profile" : "browser")
      << " SaaS usage report with " << report.domain_metrics_size()
      << " domain metrics.";

  ::chrome::cros::reporting::proto::Event event;
  *event.mutable_saas_usage_report_event() = report;

  client->ReportSaasUsageEvent(event, per_profile, *dm_token,
                               std::move(upload_callback));
}

}  // namespace enterprise_reporting

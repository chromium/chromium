// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REALTIME_REPORTING_CLIENT_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REALTIME_REPORTING_CLIENT_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/enterprise/common/proto/upload_request_response.pb.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"

class ProfileIOS;

namespace signin {
class IdentityManager;
}

// The event reporting client that sends an event to the reporting server and
// it's utilized by the reporting event router.
namespace enterprise_connectors {
class IOSRealtimeReportingClient : public RealtimeReportingClientBase {
 public:
  explicit IOSRealtimeReportingClient(ProfileIOS* profile);

  IOSRealtimeReportingClient(const IOSRealtimeReportingClient&) = delete;
  IOSRealtimeReportingClient& operator=(const IOSRealtimeReportingClient&) =
      delete;

  ~IOSRealtimeReportingClient() override;

  // RealtimeReportingClientBase overrides:
  std::string GetProfileUserName() override;
  base::WeakPtr<RealtimeReportingClientBase> AsWeakPtr() override;
  std::optional<ReportingSettings> GetReportingSettings() override;

  base::WeakPtr<IOSRealtimeReportingClient> AsWeakPtrImpl() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetBrowserCloudPolicyClientForTesting(policy::CloudPolicyClient* client);
  void SetProfileCloudPolicyClientForTesting(policy::CloudPolicyClient* client);

  void SetIdentityManagerForTesting(signin::IdentityManager* identity_manager);

  // policy::CloudPolicyClient::Observer overrides:
  void OnClientError(policy::CloudPolicyClient* client) override;

  // Report safe browsing event through real-time reporting channel, if enabled.
  // Declared as virtual for tests.
  virtual void ReportRealtimeEvent(const std::string& name,
                                   const ReportingSettings& settings,
                                   base::Value::Dict event);

 private:
  // RealtimeReportingClientBase overrides (all overrides below):
  std::string GetProfileIdentifier() override;
  std::string GetBrowserClientId() override;
  base::Value::Dict GetContext() override;
  ::chrome::cros::reporting::proto::UploadEventsRequest
  CreateUploadEventsRequest() override;
  bool ShouldIncludeDeviceInfo(bool per_profile) override;
  void UploadCallbackDeprecated(
      base::Value::Dict event_wrapper,
      bool per_profile,
      policy::CloudPolicyClient* client,
      EnterpriseReportingEventType eventType,
      policy::CloudPolicyClient::Result upload_result) override;
  void UploadCallback(
      ::chrome::cros::reporting::proto::UploadEventsRequest request,
      bool per_profile,
      policy::CloudPolicyClient* client,
      EnterpriseReportingEventType eventType,
      policy::CloudPolicyClient::Result upload_result) override;

  std::pair<std::string, policy::CloudPolicyClient*> InitProfileReportingClient(
      const std::string& dm_token) override;

  void RemoveDmTokenFromRejectedSet(const std::string& dm_token);

  raw_ptr<ProfileIOS> profile_;
  std::string username_;

  base::WeakPtrFactory<IOSRealtimeReportingClient> weak_ptr_factory_{this};
};
}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REALTIME_REPORTING_CLIENT_H_

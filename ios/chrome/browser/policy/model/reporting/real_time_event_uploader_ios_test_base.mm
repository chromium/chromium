// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/real_time_event_uploader_ios_test_base.h"

#import <memory>
#import <string>
#import <utility>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/cloud_policy_service.h"
#import "components/policy/core/common/cloud/dm_token.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "services/network/test/test_network_connection_tracker.h"

namespace enterprise_reporting {

namespace {

constexpr char kTestAffiliationId[] = "test_affiliation_id";
constexpr char kTestClientId[] = "browser_client_id";
constexpr char kTestDomain[] = "example.com";

}  // namespace

MockIOSRealtimeReportingClient::MockIOSRealtimeReportingClient(
    ProfileIOS* profile)
    : IOSRealtimeReportingClient(profile) {}

MockIOSRealtimeReportingClient::~MockIOSRealtimeReportingClient() = default;

RealTimeEventUploaderIOSTestBase::RealTimeEventUploaderIOSTestBase() = default;

RealTimeEventUploaderIOSTestBase::~RealTimeEventUploaderIOSTestBase() = default;

void RealTimeEventUploaderIOSTestBase::SetUp() {
  PlatformTest::SetUp();
  fake_browser_dm_token_storage_.EnableStorage(true);
  policy::BrowserDMTokenStorage::SetForTesting(&fake_browser_dm_token_storage_);
}

void RealTimeEventUploaderIOSTestBase::TearDown() {
  GetApplicationContext()
      ->GetBrowserPolicyConnector()
      ->SetMachineLevelUserCloudPolicyManagerForTesting(/*manager=*/nullptr);
  fake_browser_dm_token_storage_.ResetForTesting();
  policy::BrowserDMTokenStorage::SetForTesting(nullptr);
  PlatformTest::TearDown();
}

void RealTimeEventUploaderIOSTestBase::SetBrowserManaged(
    bool is_managed,
    bool set_affiliation,
    const std::string& dm_token) {
  if (is_managed) {
    fake_browser_dm_token_storage_.SetDMToken(dm_token);
    fake_browser_dm_token_storage_.SetClientId(kTestClientId);

    if (!set_affiliation) {
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->SetMachineLevelUserCloudPolicyManagerForTesting(nullptr);
      machine_policy_manager_.reset();
      return;
    }

    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_managed_by(kTestDomain);
    policy_data->add_device_affiliation_ids(kTestAffiliationId);
    policy_data->set_state(enterprise_management::PolicyData::ACTIVE);

    auto machine_store =
        std::make_unique<policy::MachineLevelUserCloudPolicyStore>(
            policy::DMToken::CreateValidToken(dm_token), std::string(),
            base::FilePath(), base::FilePath(), base::FilePath(),
            base::FilePath(),
            policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
            scoped_refptr<base::SequencedTaskRunner>());
    machine_store->set_policy_data_for_testing(std::move(policy_data));

    machine_policy_manager_ =
        std::make_unique<policy::MachineLevelUserCloudPolicyManager>(
            std::move(machine_store), /*extension_install_store=*/nullptr,
            /*external_data_manager=*/nullptr,
            /*policy_dir=*/base::FilePath(),
            scoped_refptr<base::SequencedTaskRunner>(),
            network::TestNetworkConnectionTracker::CreateGetter());

    auto client = std::make_unique<policy::CloudPolicyClient>(
        /*service=*/nullptr, /*url_loader_factory=*/nullptr,
        policy::CloudPolicyClient::DeviceDMTokenCallback());
    client->SetupRegistration(dm_token, kTestClientId, {kTestAffiliationId});
    machine_policy_manager_->core()->ConnectForTesting(
        /*service=*/nullptr, std::move(client));

    GetApplicationContext()
        ->GetBrowserPolicyConnector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(
            machine_policy_manager_.get());
  } else {
    fake_browser_dm_token_storage_.ResetForTesting();
    policy::BrowserDMTokenStorage::SetForTesting(nullptr);
    GetApplicationContext()
        ->GetBrowserPolicyConnector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(nullptr);
    machine_policy_manager_.reset();
  }
}

ProfileIOS* RealTimeEventUploaderIOSTestBase::CreateProfile(
    const std::string& name,
    bool is_managed,
    bool is_affiliated,
    bool create_reporting_client) {
  TestProfileIOS::Builder builder;
  builder.SetName(name);

  if (create_reporting_client) {
    builder.AddTestingFactory(
        enterprise_connectors::IOSRealtimeReportingClientFactory::GetInstance(),
        base::BindOnce(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<MockIOSRealtimeReportingClient>(profile);
            }));
  }
  policy::MockUserCloudPolicyStore* policy_store = nullptr;
  if (is_managed) {
    auto user_policy_data =
        std::make_unique<enterprise_management::PolicyData>();
    user_policy_data->set_request_token("user_dm_token_" + name);
    user_policy_data->set_policy_type(
        policy::dm_protocol::GetChromeUserPolicyType());
    user_policy_data->set_state(enterprise_management::PolicyData::ACTIVE);
    if (is_affiliated) {
      user_policy_data->add_user_affiliation_ids(kTestAffiliationId);
    }

    auto user_store = std::make_unique<policy::MockUserCloudPolicyStore>(
        policy::dm_protocol::GetChromeUserPolicyType());
    user_store->set_policy_data_for_testing(std::move(user_policy_data));
    policy_store = user_store.get();

    auto cloud_policy_manager =
        std::make_unique<policy::UserCloudPolicyManager>(
            std::move(user_store), /*extension_install_store=*/nullptr,
            base::FilePath(),
            /*cloud_external_data_manager=*/nullptr,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            network::TestNetworkConnectionTracker::CreateGetter());

    builder.SetUserCloudPolicyManager(std::move(cloud_policy_manager));
  }

  builder.SetPolicyConnector(std::make_unique<ProfilePolicyConnectorMock>(
      std::make_unique<policy::MockPolicyService>(), &schema_registry_,
      policy_store));

  return profile_manager_.AddProfileWithBuilder(std::move(builder));
}

MockIOSRealtimeReportingClient* RealTimeEventUploaderIOSTestBase::GetMockClient(
    ProfileIOS* profile) {
  return static_cast<MockIOSRealtimeReportingClient*>(
      enterprise_connectors::IOSRealtimeReportingClientFactory::GetForProfile(
          profile));
}

}  // namespace enterprise_reporting

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_ios.h"

#import <memory>
#import <utility>

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/enterprise/browser/reporting/report_scheduler.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_reporting {

namespace {

constexpr char kDMToken[] = "dm_token";
constexpr char kClientId[] = "client_id";

}  // namespace

class CloudProfileReportingServiceIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // The mock store starts uninitialized, mirroring a real
    // UserCloudPolicyStore whose asynchronous load from disk has not completed
    // yet. `NotifyStoreLoaded()` / `NotifyStoreError()` complete the
    // initialization.
    auto store = std::make_unique<policy::MockUserCloudPolicyStore>(
        policy::dm_protocol::GetChromeUserPolicyType());
    store_ = store.get();

    auto user_cloud_policy_manager =
        std::make_unique<policy::UserCloudPolicyManager>(
            std::move(store), /*extension_install_store=*/nullptr,
            base::FilePath(),
            /*cloud_external_data_manager=*/nullptr,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            network::TestNetworkConnectionTracker::CreateGetter());

    TestProfileIOS::Builder builder;
    builder.SetUserCloudPolicyManager(std::move(user_cloud_policy_manager));
    profile_ = std::move(builder).Build();
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
      service_.reset();
    }
    PlatformTest::TearDown();
  }

  // Creates the service under test the same way
  // CloudProfileReportingServiceFactoryIOS::BuildServiceInstanceFor() does.
  void CreateService(
      policy::UserCloudPolicyManager* user_cloud_policy_manager) {
    ReportingDelegateFactoryIOS delegate_factory;
    service_ = std::make_unique<CloudProfileReportingServiceIOS>(
        /*profile_id_service=*/nullptr, profile_->GetSharedURLLoaderFactory(),
        profile_->GetProfileName(),
        delegate_factory.GetReportSchedulerDelegate(profile_.get()),
        /*signals_aggregator=*/nullptr,
        /*saas_usage_report_scheduler=*/nullptr, user_cloud_policy_manager);
  }

  void CreateService() { CreateService(profile_->GetUserCloudPolicyManager()); }

  // Puts a policy carrying a DM token and a client id into the store, as if
  // the profile had been enrolled.
  void SetEnrolledPolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_request_token(kDMToken);
    policy_data->set_device_id(kClientId);
    store_->set_policy_data_for_testing(std::move(policy_data));
  }

  base::test::ScopedFeatureList scoped_feature_list_{kCloudProfileReporting};
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  // Owned by the UserCloudPolicyManager, itself owned by `profile_`.
  raw_ptr<policy::MockUserCloudPolicyStore> store_ = nullptr;
  // Declared last so that it is destroyed before `profile_` and `store_`.
  std::unique_ptr<CloudProfileReportingServiceIOS> service_;
};

// The ReportScheduler must not be created while the store is still loading,
// and must be created as soon as the store reports that it has loaded.
TEST_F(CloudProfileReportingServiceIOSTest, SchedulerCreatedOnStoreLoaded) {
  ASSERT_FALSE(store_->is_initialized());

  CreateService();
  EXPECT_EQ(service_->report_scheduler(), nullptr);

  store_->NotifyStoreLoaded();
  EXPECT_NE(service_->report_scheduler(), nullptr);
}

// A load error also completes the store initialization, so the ReportScheduler
// must be created in that case too.
TEST_F(CloudProfileReportingServiceIOSTest, SchedulerCreatedOnStoreError) {
  ASSERT_FALSE(store_->is_initialized());

  CreateService();
  EXPECT_EQ(service_->report_scheduler(), nullptr);

  store_->NotifyStoreError();
  EXPECT_NE(service_->report_scheduler(), nullptr);
}

// If the store is already initialized when the service is created, the
// ReportScheduler is created synchronously.
TEST_F(CloudProfileReportingServiceIOSTest,
       SchedulerCreatedImmediatelyWhenStoreAlreadyInitialized) {
  store_->NotifyStoreLoaded();
  ASSERT_TRUE(store_->is_initialized());

  CreateService();
  EXPECT_NE(service_->report_scheduler(), nullptr);
}

// Without a UserCloudPolicyManager there is no store to wait for, so the
// ReportScheduler is created synchronously.
TEST_F(CloudProfileReportingServiceIOSTest,
       SchedulerCreatedImmediatelyWithoutPolicyManager) {
  CreateService(/*user_cloud_policy_manager=*/nullptr);
  EXPECT_NE(service_->report_scheduler(), nullptr);
}

// Once the ReportScheduler exists, a later store load (e.g. the profile got
// enrolled and received a DM token) must be forwarded to the scheduler so that
// it can register its client and schedule the next report.
TEST_F(CloudProfileReportingServiceIOSTest,
       StoreLoadedAfterSchedulerCreatedUpdatesDMToken) {
  profile_->GetPrefs()->SetBoolean(kCloudProfileReportingEnabled, true);
  profile_->GetPrefs()->SetBoolean(kPoliciesEverFetchedWithProfileId, true);

  // The store finishes loading without any policy, i.e. no DM token yet.
  store_->NotifyStoreLoaded();
  CreateService();
  ASSERT_NE(service_->report_scheduler(), nullptr);
  EXPECT_FALSE(service_->report_scheduler()->IsNextReportScheduledForTesting());

  // The store now loads a policy carrying a DM token and a client id.
  SetEnrolledPolicyData();
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(service_->report_scheduler()->IsNextReportScheduledForTesting());
}

// After Shutdown(), the service must neither own a ReportScheduler nor react
// to store notifications anymore.
TEST_F(CloudProfileReportingServiceIOSTest, ShutdownStopsObservingStore) {
  CreateService();
  EXPECT_EQ(service_->report_scheduler(), nullptr);

  service_->Shutdown();
  store_->NotifyStoreLoaded();
  EXPECT_EQ(service_->report_scheduler(), nullptr);
}

}  // namespace enterprise_reporting

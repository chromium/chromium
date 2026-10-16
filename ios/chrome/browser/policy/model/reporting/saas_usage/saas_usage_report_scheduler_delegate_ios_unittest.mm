// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_report_scheduler_delegate_ios.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/test/mock_callback.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_test_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_reporting {

namespace {

std::unique_ptr<KeyedService> CreateNullService(ProfileIOS* profile) {
  return nullptr;
}

}  // namespace

class SaasUsageReportSchedulerDelegateIOSTest : public PlatformTest {
 public:
  SaasUsageReportSchedulerDelegateIOSTest() = default;
  ~SaasUsageReportSchedulerDelegateIOSTest() override = default;

  ProfileIOS* CreatePersonalProfile(bool has_client) {
    CHECK(profile_manager_.GetLoadedProfiles().empty());
    return profile_manager_.AddProfileWithBuilder(
        CreateProfileBuilder(has_client));
  }

  ProfileIOS* CreateManagedProfile(bool has_client) {
    return AddManagedProfileWithBuilder(
        profile_manager_, [FakeSystemIdentity fakeManagedIdentity],
        CreateProfileBuilder(has_client));
  }

 protected:
  TestProfileIOS::Builder CreateProfileBuilder(bool has_client) {
    TestProfileIOS::Builder builder;
    if (has_client) {
      builder.AddTestingFactory(
          enterprise_connectors::IOSRealtimeReportingClientFactory::
              GetInstance(),
          enterprise_connectors::IOSRealtimeReportingClientFactory::
              GetDefaultFactory());
    } else {
      builder.AddTestingFactory(
          enterprise_connectors::IOSRealtimeReportingClientFactory::
              GetInstance(),
          base::BindRepeating(&CreateNullService));
    }
    return builder;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
};

TEST_F(SaasUsageReportSchedulerDelegateIOSTest, IsReady_NoProfiles) {
  SaasUsageReportSchedulerDelegateIOS delegate;
  EXPECT_FALSE(delegate.IsReady());
}

TEST_F(SaasUsageReportSchedulerDelegateIOSTest, IsReady_ProfileWithoutClient) {
  SaasUsageReportSchedulerDelegateIOS delegate;
  CreatePersonalProfile(/*has_client=*/false);
  EXPECT_FALSE(delegate.IsReady());
}

TEST_F(SaasUsageReportSchedulerDelegateIOSTest, IsReady_ProfileWithClient) {
  SaasUsageReportSchedulerDelegateIOS delegate;
  CreatePersonalProfile(/*has_client=*/true);
  EXPECT_TRUE(delegate.IsReady());
}

TEST_F(SaasUsageReportSchedulerDelegateIOSTest, IsReady_MixedProfiles) {
  SaasUsageReportSchedulerDelegateIOS delegate;
  CreatePersonalProfile(/*has_client=*/false);
  EXPECT_FALSE(delegate.IsReady());
  CreateManagedProfile(/*has_client=*/true);
  EXPECT_TRUE(delegate.IsReady());
}

TEST_F(SaasUsageReportSchedulerDelegateIOSTest,
       CallbackCalledWhenProfileWithClientAdded) {
  SaasUsageReportSchedulerDelegateIOS delegate;
  base::MockCallback<base::RepeatingClosure> callback;
  delegate.SetReadyStateChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run()).Times(1);
  CreatePersonalProfile(/*has_client=*/true);
}

TEST_F(SaasUsageReportSchedulerDelegateIOSTest,
       CallbackNotCalledWhenProfileWithoutClientAdded) {
  SaasUsageReportSchedulerDelegateIOS delegate;
  base::MockCallback<base::RepeatingClosure> callback;
  delegate.SetReadyStateChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run()).Times(0);
  CreatePersonalProfile(/*has_client=*/false);
}

TEST_F(SaasUsageReportSchedulerDelegateIOSTest,
       NotifiesOnlyWhenReadyStateChanges) {
  SaasUsageReportSchedulerDelegateIOS delegate;
  base::MockCallback<base::RepeatingClosure> callback;
  delegate.SetReadyStateChangedCallback(callback.Get());

  // Callback should be called when the first profile with a client is added.
  EXPECT_CALL(callback, Run()).Times(1);
  ProfileIOS* personal_profile = CreatePersonalProfile(/*has_client=*/true);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Callback should not be called again if the delegate is already ready.
  EXPECT_CALL(callback, Run()).Times(0);
  ProfileIOS* managed_profile = CreateManagedProfile(/*has_client=*/true);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Callback should not be called when there are still profiles with client.
  EXPECT_CALL(callback, Run()).Times(0);
  delegate.OnProfileUnloaded(&profile_manager_, managed_profile);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Callback should be called when the last profile with a client is removed.
  EXPECT_CALL(callback, Run()).Times(1);
  delegate.OnProfileUnloaded(&profile_manager_, personal_profile);
  testing::Mock::VerifyAndClearExpectations(&callback);
}

TEST_F(SaasUsageReportSchedulerDelegateIOSTest, PreExistingProfileWithClient) {
  CreatePersonalProfile(/*has_client=*/true);
  SaasUsageReportSchedulerDelegateIOS delegate;
  EXPECT_TRUE(delegate.IsReady());
}

TEST_F(SaasUsageReportSchedulerDelegateIOSTest,
       PreExistingProfileWithoutClient) {
  CreatePersonalProfile(/*has_client=*/false);
  SaasUsageReportSchedulerDelegateIOS delegate;
  EXPECT_FALSE(delegate.IsReady());
}

}  // namespace enterprise_reporting

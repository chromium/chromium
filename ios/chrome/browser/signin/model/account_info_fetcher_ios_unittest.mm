// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_info_fetcher_ios.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace ios {
namespace {

FakeSystemIdentity* CreateFakeIdentity() {
  FakeSystemIdentity* identity =
      [FakeSystemIdentity identityWithEmail:@"foo@gmail.com"
                                     gaiaID:GaiaId("foo_gaia_id")];
  identity.userFullName = @"Foo Name";
  identity.userGivenName = @"Foo";
  return identity;
}

}  // namespace

class AccountInfoFetcherIosTest : public PlatformTest {
 public:
  AccountInfoFetcherIosTest() {
    scoped_feature_list_.InitAndEnableFeature(
        switches::kUseAccountInfoFetcherIos);

    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_);
    system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  raw_ptr<FakeSystemIdentityManager> system_identity_manager_;
};

TEST_F(AccountInfoFetcherIosTest, FetchAccountInfoInstant) {
  FakeSystemIdentity* identity = CreateFakeIdentity();
  system_identity_manager_->AddIdentity(identity);

  std::optional<AccountInfo> result_info;

  auto fetcher = std::make_unique<AccountInfoFetcherIos>(
      account_manager_service_, identity.gaiaId,
      base::BindOnce(
          [](std::optional<AccountInfo>* result_info,
             std::optional<AccountInfo> info) {
            *result_info = std::move(info);
          },
          &result_info));
  fetcher->Start();

  // When we have a cached hosted domain for the account - the information
  // should be available instantaneously.
  ASSERT_TRUE(result_info.has_value());
  EXPECT_EQ(result_info->GetGaiaId(), identity.gaiaId);
  EXPECT_EQ(result_info->GetEmail(), "foo@gmail.com");
  EXPECT_EQ(result_info->GetFullName(), "Foo Name");
  EXPECT_EQ(result_info->GetGivenName(), "Foo");
  EXPECT_EQ(result_info->GetHostedDomain(), "");
}

TEST_F(AccountInfoFetcherIosTest, FetchAccountInfoDelayed) {
  FakeSystemIdentity* identity = CreateFakeIdentity();
  system_identity_manager_->AddIdentity(identity);
  SystemIdentityManager::HostedDomainCallback captured_hosted_domain_callback;
  system_identity_manager_->OverrideGetHostedDomainImplementation(
      base::BindRepeating(
          [](SystemIdentityManager::HostedDomainCallback*
                 captured_hosted_domain_callback,
             id<SystemIdentity>,
             SystemIdentityManager::HostedDomainCallback callback) {
            CHECK(!*captured_hosted_domain_callback);
            *captured_hosted_domain_callback = std::move(callback);
          },
          &captured_hosted_domain_callback));

  base::RunLoop run_loop;
  std::optional<AccountInfo> result_info;

  auto fetcher = std::make_unique<AccountInfoFetcherIos>(
      account_manager_service_, identity.gaiaId,
      base::BindOnce(
          [](base::RunLoop* run_loop, std::optional<AccountInfo>* result_info,
             std::optional<AccountInfo> info) {
            *result_info = std::move(info);
            run_loop->Quit();
          },
          &run_loop, &result_info));
  fetcher->Start();

  ASSERT_FALSE(result_info.has_value());

  // Simulate GetHostedDomain request completing successfully.
  ASSERT_TRUE(captured_hosted_domain_callback);
  std::move(captured_hosted_domain_callback).Run(@"", nil);
  run_loop.Run();

  ASSERT_TRUE(result_info.has_value());
  EXPECT_EQ(result_info->GetGaiaId(), identity.gaiaId);
  EXPECT_EQ(result_info->GetEmail(), "foo@gmail.com");
  EXPECT_EQ(result_info->GetFullName(), "Foo Name");
  EXPECT_EQ(result_info->GetGivenName(), "Foo");
  EXPECT_EQ(result_info->GetHostedDomain(), "");

  system_identity_manager_->OverrideGetHostedDomainImplementation({});
}

TEST_F(AccountInfoFetcherIosTest, FetchAccountInfoIdentityNotFound) {
  FakeSystemIdentity* identity = CreateFakeIdentity();
  base::RunLoop run_loop;
  std::optional<AccountInfo> result_info;
  bool callback_called = false;

  auto fetcher = std::make_unique<AccountInfoFetcherIos>(
      account_manager_service_, identity.gaiaId,
      base::BindOnce(
          [](base::RunLoop* run_loop, std::optional<AccountInfo>* result_info,
             bool* callback_called, std::optional<AccountInfo> info) {
            *callback_called = true;
            *result_info = std::move(info);
            run_loop->Quit();
          },
          &run_loop, &result_info, &callback_called));
  fetcher->Start();
  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(result_info.has_value());
}

}  // namespace ios

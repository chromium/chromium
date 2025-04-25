// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/system_account_updater.h"

#import <UIKit/UIKit.h>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/widget_kit/model/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "testing/platform_test.h"

#if BUILDFLAG(ENABLE_WIDGETS_FOR_MIM)

class SystemAccountUpdaterTest : public PlatformTest {
 public:
  SystemAccountUpdaterTest() {
    NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
    [shared_defaults removeObjectForKey:app_group::kAccountsOnDevice];

    system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_account_updater_ =
        std::make_unique<SystemAccountUpdater>(system_identity_manager_);
  }

  ~SystemAccountUpdaterTest() override {
    system_identity_manager_ = nullptr;

    NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
    [shared_defaults removeObjectForKey:app_group::kAccountsOnDevice];
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SystemAccountUpdater> system_account_updater_;
  raw_ptr<FakeSystemIdentityManager> system_identity_manager_;
};

// Tests that OnIdentityListChangedis correctly updates key
// 'app_group::kAccountsOnDevice'.
TEST_F(SystemAccountUpdaterTest, OnIdentityListChanged) {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  EXPECT_EQ([[shared_defaults objectForKey:app_group::kAccountsOnDevice] count],
            0u);

  // Add 'fakeIdentity1' to the identity list.
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  system_identity_manager_->AddIdentity(fake_identity);

  {
    NSDictionary* accounts_on_device =
        [shared_defaults objectForKey:app_group::kAccountsOnDevice];
    EXPECT_EQ([accounts_on_device count], 1u);
    EXPECT_TRUE(
        [[accounts_on_device allKeys] containsObject:fake_identity.gaiaID]);
    EXPECT_EQ([[accounts_on_device valueForKey:fake_identity.gaiaID]
                  valueForKey:@"email"],
              fake_identity.userEmail);
  }

  // Add 'fakeIdentity2' to the identity list.
  FakeSystemIdentity* fake_identity_2 = [FakeSystemIdentity fakeIdentity2];
  system_identity_manager_->AddIdentity(fake_identity_2);

  {
    NSDictionary* accounts_on_device =
        [shared_defaults objectForKey:app_group::kAccountsOnDevice];
    EXPECT_EQ([accounts_on_device count], 2u);
    EXPECT_TRUE(
        [[accounts_on_device allKeys] containsObject:fake_identity_2.gaiaID]);
    EXPECT_EQ([[accounts_on_device valueForKey:fake_identity_2.gaiaID]
                  valueForKey:@"email"],
              fake_identity_2.userEmail);
  }

  // Remove 'fakeIdentity' from the identity list.
  system_identity_manager_->ForgetIdentity(fake_identity, base::DoNothing());
  system_identity_manager_->WaitForServiceCallbacksToComplete();

  {
    NSDictionary* accounts_on_device =
        [shared_defaults objectForKey:app_group::kAccountsOnDevice];
    EXPECT_EQ([accounts_on_device count], 1u);
  }
}

#endif

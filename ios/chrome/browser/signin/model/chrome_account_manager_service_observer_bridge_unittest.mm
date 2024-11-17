// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class ChromeAccountManagerServiceObserverBridgeTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());

    test_observer_ =
        OCMStrictProtocolMock(@protocol(ChromeAccountManagerServiceObserver));
    observer_bridge_.reset(new ChromeAccountManagerServiceObserverBridge(
        test_observer_, account_manager_service));
  }

  void TearDown() override {
    PlatformTest::TearDown();
    observer_bridge_.reset();
    EXPECT_OCMOCK_VERIFY(test_observer_);
    test_observer_ = nil;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<ChromeAccountManagerService::Observer> observer_bridge_;
  id<ChromeAccountManagerServiceObserver> test_observer_ = nil;
};

// Tests that `onIdentityListChanged` is forwarded.
TEST_F(ChromeAccountManagerServiceObserverBridgeTest, onIdentityListChanged) {
  OCMExpect([test_observer_ identityListChanged]);
  observer_bridge_->OnIdentityListChanged();
}

// Tests that `onIdentityChanged` is forwarded.
TEST_F(ChromeAccountManagerServiceObserverBridgeTest, onIdentityUpdated) {
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];

  OCMExpect([test_observer_ identityUpdated:identity]);
  observer_bridge_->OnIdentityUpdated(identity);
}

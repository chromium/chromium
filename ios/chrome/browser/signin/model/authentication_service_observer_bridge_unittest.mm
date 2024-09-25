// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AuthenticationServiceObserverBridgeTest : public PlatformTest {
 public:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForProfile(
            profile_.get()));
  }

  AuthenticationService* GetAuthenticationService() { return auth_service_; }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<AuthenticationService> auth_service_ = nullptr;
};

// Tests that `OnServiceStatusChanged` is forwarded from the service.
TEST_F(AuthenticationServiceObserverBridgeTest, TestOnServiceStatusChanged) {
  id<AuthenticationServiceObserving> observer_delegate =
      OCMStrictProtocolMock(@protocol(AuthenticationServiceObserving));
  AuthenticationServiceObserverBridge bridge(GetAuthenticationService(),
                                             observer_delegate);

  OCMExpect([observer_delegate onServiceStatusChanged]);
  bridge.OnServiceStatusChanged();
  EXPECT_OCMOCK_VERIFY(observer_delegate);
}

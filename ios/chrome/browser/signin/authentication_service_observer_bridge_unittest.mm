// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AuthenticationServiceObserverBridgeTest : public PlatformTest {
 public:
  void SetUp() override {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    browser_state_ = builder.Build();
    auth_service_ = static_cast<AuthenticationServiceFake*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            browser_state_.get()));
  }

  AuthenticationService* GetAuthenticationService() { return auth_service_; }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  AuthenticationServiceFake* auth_service_ = nullptr;
};

// Tests that |OnPrimaryAccountRestricted| is forwarded from the service.
TEST_F(AuthenticationServiceObserverBridgeTest, primaryAccountRestricted) {
  id<AuthenticationServiceObserving> observer_delegate =
      OCMStrictProtocolMock(@protocol(AuthenticationServiceObserving));
  AuthenticationServiceObserverBridge bridge(GetAuthenticationService(),
                                             observer_delegate);

  OCMExpect([observer_delegate onPrimaryAccountRestricted]);
  bridge.OnPrimaryAccountRestricted();
  EXPECT_OCMOCK_VERIFY(observer_delegate);
}

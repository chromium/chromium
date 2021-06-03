// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_coordinator.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#import "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This class provides a hook for platform-specific operations across
// SyncScreenCoordinator unit tests.
class SyncScreenCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    browser_state_ = builder.Build();
    WebStateList* web_state_list = nullptr;
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), web_state_list);

    navigationController_ = [[UINavigationController alloc] init];
    delegate_ = OCMProtocolMock(@protocol(FirstRunScreenDelegate));

    coordinator_ = [[SyncScreenCoordinator alloc]
        initWithBaseNavigationController:navigationController_
                                 browser:browser_.get()
                                delegate:delegate_];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  SyncScreenCoordinator* coordinator_;
  UINavigationController* navigationController_;
  id delegate_;
};

// Tests that calling the delegate immidiately to stop the coordinator when
// there's no user identity.
TEST_F(SyncScreenCoordinatorTest, TestStartWithoutIdentity) {
  OCMExpect([delegate_ willFinishPresenting]);
  [coordinator_ start];
}

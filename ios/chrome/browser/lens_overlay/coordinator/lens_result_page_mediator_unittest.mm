// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class LensResultPageMediatorTest : public PlatformTest {
 public:
  LensResultPageMediatorTest() {
    // AuthenticationService in required in AttachTabHelpers.
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    web::WebState::CreateParams params(browser_state_.get());
    mediator_ = [[LensResultPageMediator alloc]
         initWithWebStateParams:params
        browserWebStateDelegate:&browser_web_state_delegate_];
  }

  ~LensResultPageMediatorTest() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;

  LensResultPageMediator* mediator_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::FakeWebStateDelegate browser_web_state_delegate_;
};

// Tests that the mediator starts a navigation when loadResultsURL is called.
TEST_F(LensResultPageMediatorTest, ShouldStartNavigationWhenLoadingResultsURL) {
  GURL result_url = GURL("google.com");
  [mediator_ loadResultsURL:result_url];

  EXPECT_EQ(browser_web_state_delegate_.last_open_url_request()->params.url,
            result_url);
}

}  // namespace

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/task_observer_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "net/test/embedded_test_server/http_request.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kWrongURL = @"https://example.com";

}  // namespace

class PasswordTabHelperTest : public PlatformTest {
 public:
  PasswordTabHelperTest()
      : web_client_(std::make_unique<web::FakeWebClient>()),
        task_environment_(web::WebTaskEnvironment::Options::IO_MAINLOOP) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);

    UniqueIDDataTabHelper::CreateForWebState(web_state_.get());
    PasswordTabHelper::CreateForWebState(web_state_.get());
  }

  void SetUp() override {
    PlatformTest::SetUp();

    id dispatcher = [[CommandDispatcher alloc] init];
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));
    [dispatcher
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    helper_ = PasswordTabHelper::FromWebState(web_state_.get());
    ASSERT_TRUE(helper_);

    helper_->SetDispatcher(dispatcher);
  }

 protected:
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  PasswordTabHelper* helper_ = nullptr;
  id dispatcher_;
};

TEST_F(PasswordTabHelperTest, RedirectsToPasswordsAndCancelsRequest) {
  base::HistogramTester histogram_tester;
  NSURLRequest* request = [NSURLRequest
      requestWithURL:
          [NSURL URLWithString:base::SysUTF8ToNSString(
                                   password_manager::kManageMyPasswordsURL)]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*has_user_gesture=*/false);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision request_policy =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        request_policy = decision;
        callback_called = true;
      });

  OCMExpect([dispatcher_ showSavedPasswordsSettingsFromViewController:nil
                                                     showCancelButton:NO
                                                   startPasswordCheck:NO]);

  helper_->ShouldAllowRequest(request, request_info, std::move(callback));

  EXPECT_OCMOCK_VERIFY(dispatcher_);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(request_policy.ShouldCancelNavigation());
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kPasswordsGoogleWebsite, 1);
}

TEST_F(PasswordTabHelperTest, NoRedirectWhenWrongLink) {
  base::HistogramTester histogram_tester;
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:kWrongURL]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*has_user_gesture=*/false);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision request_policy =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        request_policy = decision;
        callback_called = true;
      });

  helper_->ShouldAllowRequest(request, request_info, std::move(callback));

  EXPECT_OCMOCK_VERIFY(dispatcher_);
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(request_policy.ShouldCancelNavigation());
  histogram_tester.ExpectTotalCount("PasswordManager.ManagePasswordsReferrer",
                                    0);
}

TEST_F(PasswordTabHelperTest, NoRedirectWhenWrongTransition) {
  base::HistogramTester histogram_tester;
  NSURLRequest* request = [NSURLRequest
      requestWithURL:
          [NSURL URLWithString:base::SysUTF8ToNSString(
                                   password_manager::kManageMyPasswordsURL)]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED, /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*has_user_gesture=*/false);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision request_policy =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        request_policy = decision;
        callback_called = true;
      });

  helper_->ShouldAllowRequest(request, request_info, std::move(callback));

  EXPECT_OCMOCK_VERIFY(dispatcher_);
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(request_policy.ShouldCancelNavigation());
  histogram_tester.ExpectTotalCount("PasswordManager.ManagePasswordsReferrer",
                                    0);
}

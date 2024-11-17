// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
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

namespace {

NSString* const kWrongURL = @"https://example.com";

}  // namespace

class PasswordTabHelperTest : public PlatformTest {
 public:
  PasswordTabHelperTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);

    PasswordTabHelper::CreateForWebState(web_state_.get());
  }

  void SetUp() override {
    PlatformTest::SetUp();

    id dispatcher = [[CommandDispatcher alloc] init];
    id mockSettingsCommandHandler =
        OCMProtocolMock(@protocol(SettingsCommands));
    dispatcher_ = mockSettingsCommandHandler;
    [dispatcher startDispatchingToTarget:mockSettingsCommandHandler
                             forProtocol:@protocol(SettingsCommands)];

    helper_ = PasswordTabHelper::FromWebState(web_state_.get());
    ASSERT_TRUE(helper_);

    helper_->SetDispatcher(dispatcher);
  }

 protected:
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  raw_ptr<PasswordTabHelper> helper_ = nullptr;
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
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision request_policy =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        request_policy = decision;
        callback_called = true;
      });

  OCMExpect([dispatcher_ showSavedPasswordsSettingsFromViewController:nil
                                                     showCancelButton:NO]);

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
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
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
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
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

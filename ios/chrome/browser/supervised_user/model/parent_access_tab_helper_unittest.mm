// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/strings/stringprintf.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/supervised_user/test_support/parent_access_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// A test object that conforms to the NetExportTabHelperDelegate protocol.
@interface TestParentAccessTabHelperDelegate
    : NSObject <ParentAccessTabHelperDelegate>
// Boolean to track whether the bottom sheet is hidden.
@property(nonatomic, readonly) BOOL isBottomSheetHidden;
// Result extracted from the PACP callback.
@property(nonatomic, readonly) supervised_user::LocalApprovalResult result;

@end

@implementation TestParentAccessTabHelperDelegate
@synthesize isBottomSheetHidden = _isBottomSheetHidden;
@synthesize result = _result;

- (void)hideParentAccessBottomSheetWithResult:
    (supervised_user::LocalApprovalResult)result {
  _isBottomSheetHidden = true;
  _result = result;
}

@end

// Test fixture for testing ParentAccessTabHelper.
class ParentAccessTabHelperTest : public PlatformTest {
 public:
  ParentAccessTabHelperTest()
      : delegate_([[TestParentAccessTabHelperDelegate alloc] init]) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
  }

  ParentAccessTabHelperTest(const ParentAccessTabHelperTest&) = delete;
  ParentAccessTabHelperTest& operator=(const ParentAccessTabHelperTest&) =
      delete;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ParentAccessTabHelper::CreateForWebState(web_state());
    tab_helper()->SetDelegate(delegate_);
  }

  ParentAccessTabHelper* tab_helper() {
    return ParentAccessTabHelper::FromWebState(web_state());
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  // A delegate that is given to the ParentAccessTabHelper for testing.
  __strong TestParentAccessTabHelperDelegate* delegate_;
};

// Verifies the initial state of the ParentAccessTabHelper and its delegate.
TEST_F(ParentAccessTabHelperTest, InitialState) {
  EXPECT_TRUE(tab_helper());
  EXPECT_FALSE(delegate_.isBottomSheetHidden);
}

// Verifies that an invalid URL does not affect the bottom sheet state.
TEST_F(ParentAccessTabHelperTest, InvalidURL) {
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com"]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);

  __block std::optional<web::WebStatePolicyDecider::PolicyDecision>
      policy_decision = std::nullopt;
  tab_helper()->ShouldAllowRequest(
      request, request_info,
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
      }));

  EXPECT_FALSE(delegate_.isBottomSheetHidden);
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());
}

// Verifies that a valid URL that has an empty result closes the bottom sheet
// and reports the error metric.
TEST_F(ParentAccessTabHelperTest, ValidUrlWithoutResult) {
  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL
                         URLWithString:@"http://families.google.com/?result="]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);

  __block std::optional<web::WebStatePolicyDecider::PolicyDecision>
      policy_decision = std::nullopt;
  tab_helper()->ShouldAllowRequest(
      request, request_info,
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
      }));

  EXPECT_TRUE(delegate_.isBottomSheetHidden);
  EXPECT_EQ(delegate_.result, supervised_user::LocalApprovalResult::kError);
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());
}

// Verifies that a valid URL with a malformed message closes the bottom sheet
// and reports the error metric.
TEST_F(ParentAccessTabHelperTest, ValidUrlWithError) {
  GURL result_url(
      base::StringPrintf("http://families.google.com/?result=malformed"));
  NSURLRequest* request =
      [NSURLRequest requestWithURL:net::NSURLWithGURL(result_url)];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);

  __block std::optional<web::WebStatePolicyDecider::PolicyDecision>
      policy_decision = std::nullopt;
  tab_helper()->ShouldAllowRequest(
      request, request_info,
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
      }));

  EXPECT_TRUE(delegate_.isBottomSheetHidden);
  EXPECT_EQ(delegate_.result, supervised_user::LocalApprovalResult::kError);
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());
}

// Verifies that a valid URL with an unexpected result closes the bottom sheet
// and reports an error.
TEST_F(ParentAccessTabHelperTest, ValidUrlWithUnexpectedResult) {
  GURL result_url(
      base::StringPrintf("http://families.google.com/?result=%s",
                         supervised_user::CreatePacpResizeResult()));
  NSURLRequest* request =
      [NSURLRequest requestWithURL:net::NSURLWithGURL(result_url)];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);

  __block std::optional<web::WebStatePolicyDecider::PolicyDecision>
      policy_decision = std::nullopt;
  tab_helper()->ShouldAllowRequest(
      request, request_info,
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
      }));

  EXPECT_TRUE(delegate_.isBottomSheetHidden);
  EXPECT_EQ(delegate_.result, supervised_user::LocalApprovalResult::kError);
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());
}

// Verifies that a valid URL with a valid result closes the bottom sheet and
// reports an approved result.
TEST_F(ParentAccessTabHelperTest, ValidUrlWithApprovalResult) {
  GURL result_url(
      base::StringPrintf("http://families.google.com/?result=%s",
                         supervised_user::CreatePacpApprovalResult()));
  NSURLRequest* request =
      [NSURLRequest requestWithURL:net::NSURLWithGURL(result_url)];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false, /*user_tapped_recently=*/false);

  __block std::optional<web::WebStatePolicyDecider::PolicyDecision>
      policy_decision = std::nullopt;
  tab_helper()->ShouldAllowRequest(
      request, request_info,
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
      }));

  EXPECT_TRUE(delegate_.isBottomSheetHidden);
  EXPECT_EQ(delegate_.result, supervised_user::LocalApprovalResult::kApproved);
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());
}

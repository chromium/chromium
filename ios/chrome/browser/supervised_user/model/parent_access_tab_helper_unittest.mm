// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/base64.h"
#import "base/strings/stringprintf.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/supervised_user/test_support/parent_access_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/supervised_user/model/ios_web_content_handler_impl.h"
#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// A test object that conforms to the ParentAccessTabHelperDelegate protocol.
@interface TestParentAccessTabHelperDelegate
    : NSObject <ParentAccessTabHelperDelegate>
// Boolean to track whether the bottom sheet is hidden.
@property(nonatomic, readonly) BOOL isBottomSheetHidden;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCompletion:(ParentAccessApprovalResultCallback)callback
    NS_DESIGNATED_INITIALIZER;

@end

@implementation TestParentAccessTabHelperDelegate {
  // Completion callback to be executed when the bottom sheet is dismissed.
  ParentAccessApprovalResultCallback _completion;
}

@synthesize isBottomSheetHidden = _isBottomSheetHidden;

- (instancetype)initWithCompletion:
    (ParentAccessApprovalResultCallback)callback {
  if ((self = [super init])) {
    _completion = std::move(callback);
  }
  return self;
}

- (void)hideParentAccessBottomSheetWithResult:
            (supervised_user::LocalApprovalResult)result
                                    errorType:
                                        (std::optional<
                                            supervised_user::
                                                LocalWebApprovalErrorType>)
                                            errorType {
  _isBottomSheetHidden = true;
  CHECK(_completion);
  std::move(_completion).Run(result, errorType);
}

@end

// Test fixture for testing ParentAccessTabHelper.
class ParentAccessTabHelperTest : public PlatformTest {
 public:
  ParentAccessTabHelperTest() {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);

    mock_parent_access_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ParentAccessCommands));
    web_content_handler_ = std::make_unique<IOSWebContentHandlerImpl>(
        web_state_.get(), mock_parent_access_commands_handler_,
        /*is_main_frame=*/true);
    web_content_handler_->is_bottomsheet_shown_ = true;

    // URL is ignored in this test; using an empty GURL.
    ParentAccessApprovalResultCallback completion_callback = base::BindOnce(
        &IOSWebContentHandlerImpl::OnLocalApprovalRequestCompleted,
        std::move(web_content_handler_), GURL(),
        /*start_time=*/base::TimeTicks());

    delegate_ = [[TestParentAccessTabHelperDelegate alloc]
        initWithCompletion:std::move(completion_callback)];
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

  base::HistogramTester histogram_tester_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  id mock_parent_access_commands_handler_;
  std::unique_ptr<IOSWebContentHandlerImpl> web_content_handler_;

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

  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 0);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 0);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalDurationMillisecondsHistogramName, 0);
}

// Verifies that a valid URL that has an empty result closes the bottom sheet
// and reports error metrics.
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
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());

  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      supervised_user::LocalApprovalResult::kError, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName,
      supervised_user::LocalWebApprovalErrorType::kPacpEmptyResponse, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalDurationMillisecondsHistogramName, 0);
}

// Verifies that a valid URL with an incorrectly encoded message closes the
// bottom sheet and reports error metrics.
TEST_F(ParentAccessTabHelperTest, ValidUrlWithDecodingError) {
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
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());

  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      supervised_user::LocalApprovalResult::kError, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName,
      supervised_user::LocalWebApprovalErrorType::kFailureToDecodePacpResponse,
      1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalDurationMillisecondsHistogramName, 0);
}

// Verifies that a valid URL with a malformed message closes the bottom sheet
// and reports the error metrics.
TEST_F(ParentAccessTabHelperTest, ValidUrlWithParsingError) {
  GURL result_url(base::StringPrintf("http://families.google.com/?result=%s",
                                     base::Base64Encode("invalid_response")));
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
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());

  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      supervised_user::LocalApprovalResult::kError, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName,
      supervised_user::LocalWebApprovalErrorType::kFailureToParsePacpResponse,
      1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalDurationMillisecondsHistogramName, 0);
}

// Verifies that a valid URL with an unexpected result closes the bottom sheet
// and reports error metrics.
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
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());

  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      supervised_user::LocalApprovalResult::kError, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName,
      supervised_user::LocalWebApprovalErrorType::kUnexpectedPacpResponse, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalDurationMillisecondsHistogramName, 0);
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
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());

  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      supervised_user::LocalApprovalResult::kApproved, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName,
      supervised_user::LocalWebApprovalErrorType::kPacpTimeoutExceeded, 0);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 0);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalDurationMillisecondsHistogramName, 1);
}

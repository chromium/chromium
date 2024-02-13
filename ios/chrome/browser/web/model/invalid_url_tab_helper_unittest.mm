// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/invalid_url_tab_helper.h"

#import <Foundation/Foundation.h>

#import "ios/net/protocol_handler_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/net_errors.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/url_constants.h"

class InvalidUrlTabHelperTest : public PlatformTest {
 protected:
  InvalidUrlTabHelperTest() {
    InvalidUrlTabHelper::CreateForWebState(&web_state_);
  }

  // Returns PolicyDecision for URL request with given `spec` and `transition`.
  web::WebStatePolicyDecider::PolicyDecision GetPolicy(
      NSString* spec,
      ui::PageTransition transition) {
    NSURL* url = [NSURL URLWithString:spec];
    NSURLRequest* request = [[NSURLRequest alloc] initWithURL:url];
    const web::WebStatePolicyDecider::RequestInfo info(
        transition,
        /*target_frame_is_main=*/true,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    web_state_.ShouldAllowRequest(request, info, std::move(callback));
    EXPECT_TRUE(callback_called);
    return policy_decision;
  }

  web::FakeWebState web_state_;
};

// Tests that navigation is allowed for https url link.
TEST_F(InvalidUrlTabHelperTest, HttpsUrl) {
  auto policy = GetPolicy(@"https://foo.test", ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(policy.ShouldAllowNavigation());
}

// Tests that navigation is allowed for https url links if url length is under
// allowed limit.
TEST_F(InvalidUrlTabHelperTest, HttpsUrlUnderLengthLimit) {
  NSString* spec = [@"https://" stringByPaddingToLength:url::kMaxURLChars
                                             withString:@"0"
                                        startingAtIndex:0];
  auto policy = GetPolicy(spec, ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(policy.ShouldAllowNavigation());
}

// Tests that navigation is cancelled for https url links if url length is above
// allowed limit.
TEST_F(InvalidUrlTabHelperTest, HttpsUrlAboveLengthLimit) {
  NSString* spec = [@"https://" stringByPaddingToLength:url::kMaxURLChars + 1
                                             withString:@"0"
                                        startingAtIndex:0];
  auto policy = GetPolicy(spec, ui::PAGE_TRANSITION_LINK);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_FALSE(policy.ShouldDisplayError());
}

// Tests that navigation is allowed for valid data url link.
TEST_F(InvalidUrlTabHelperTest, ValidDataUrlLink) {
  auto policy = GetPolicy(@"data:text/plain;charset=utf-8,test",
                          ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(policy.ShouldAllowNavigation());
}

// Tests that navigation is sillently cancelled for invalid data url link.
TEST_F(InvalidUrlTabHelperTest, InvalidDataUrlLink) {
  auto policy = GetPolicy(@"data://", ui::PAGE_TRANSITION_LINK);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_FALSE(policy.ShouldDisplayError());
}

// Tests that navigation is cancelled with error for invalid data: url bookmark.
TEST_F(InvalidUrlTabHelperTest, InvalidDataUrlBookmark) {
  auto policy = GetPolicy(@"data://", ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_NSEQ(net::kNSErrorDomain, policy.GetDisplayError().domain);
  EXPECT_EQ(net::ERR_INVALID_URL, policy.GetDisplayError().code);
}

// Tests that navigation is cancelled with error for invalid data: url typed.
TEST_F(InvalidUrlTabHelperTest, InvalidDataUrlTyped) {
  auto policy = GetPolicy(@"data://", ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_NSEQ(net::kNSErrorDomain, policy.GetDisplayError().domain);
  EXPECT_EQ(net::ERR_INVALID_URL, policy.GetDisplayError().code);
}

// Tests that navigation is cancelled with error for invalid data: url
// generated.
TEST_F(InvalidUrlTabHelperTest, InvalidDataUrlGenerated) {
  auto policy = GetPolicy(@"data://", ui::PAGE_TRANSITION_GENERATED);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_NSEQ(net::kNSErrorDomain, policy.GetDisplayError().domain);
  EXPECT_EQ(net::ERR_INVALID_URL, policy.GetDisplayError().code);
}

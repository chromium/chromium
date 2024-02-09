// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"

#import "base/functional/callback_helpers.h"
#import "ios/web/public/test/fakes/crw_fake_web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace web {

// Test fixture to test WebStatePolicyDeciderBridge class.
class WebStatePolicyDeciderBridgeTest : public PlatformTest {
 public:
  WebStatePolicyDeciderBridgeTest()
      : decider_([[CRWFakeWebStatePolicyDecider alloc] init]),
        decider_bridge_(&fake_web_state_, decider_) {}

 protected:
  web::FakeWebState fake_web_state_;
  CRWFakeWebStatePolicyDecider* decider_;
  WebStatePolicyDeciderBridge decider_bridge_;
};

// Tests `shouldAllowRequest:requestInfo:` forwarding.
TEST_F(WebStatePolicyDeciderBridgeTest, ShouldAllowRequest) {
  ASSERT_FALSE([decider_ shouldAllowRequestInfo]);
  NSURL* url = [NSURL URLWithString:@"http://test.url"];
  NSURLRequest* request = [NSURLRequest requestWithURL:url];
  const ui::PageTransition transition_type =
      ui::PageTransition::PAGE_TRANSITION_LINK;
  const bool target_frame_is_main = true;
  const bool target_frame_is_cross_origin = false;
  const bool target_window_is_cross_origin = false;
  const bool is_user_initiated = false;
  const bool user_tapped_recently = false;
  const WebStatePolicyDecider::RequestInfo request_info(
      transition_type, target_frame_is_main, target_frame_is_cross_origin,
      target_window_is_cross_origin, is_user_initiated, user_tapped_recently);
  decider_bridge_.ShouldAllowRequest(request, request_info, base::DoNothing());
  const FakeShouldAllowRequestInfo* should_allow_request_info =
      [decider_ shouldAllowRequestInfo];
  ASSERT_TRUE(should_allow_request_info);
  EXPECT_EQ(request, should_allow_request_info->request);
  EXPECT_EQ(target_frame_is_main,
            should_allow_request_info->request_info.target_frame_is_main);
  EXPECT_EQ(is_user_initiated,
            should_allow_request_info->request_info.is_user_initiated);
  EXPECT_EQ(user_tapped_recently,
            should_allow_request_info->request_info.user_tapped_recently);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      transition_type,
      should_allow_request_info->request_info.transition_type));
}

// Tests `decidePolicyForNavigationResponse:responseInfo:completionHandler:`
// forwarding.
TEST_F(WebStatePolicyDeciderBridgeTest, DecidePolicyForNavigationResponse) {
  ASSERT_FALSE([decider_ decidePolicyForNavigationResponseInfo]);
  NSURL* url = [NSURL URLWithString:@"http://test.url"];
  NSURLResponse* response = [[NSURLResponse alloc] initWithURL:url
                                                      MIMEType:@"text/html"
                                         expectedContentLength:0
                                              textEncodingName:nil];
  const bool for_main_frame = true;
  const WebStatePolicyDecider::ResponseInfo response_info(for_main_frame);
  decider_bridge_.ShouldAllowResponse(response, response_info,
                                      base::DoNothing());
  const FakeDecidePolicyForNavigationResponseInfo*
      decide_policy_for_navigation_response_info =
          [decider_ decidePolicyForNavigationResponseInfo];
  ASSERT_TRUE(decide_policy_for_navigation_response_info);
  EXPECT_EQ(response, decide_policy_for_navigation_response_info->response);
  EXPECT_EQ(
      for_main_frame,
      decide_policy_for_navigation_response_info->response_info.for_main_frame);
}

}  // namespace web

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"

#import "ios/web/public/test/fakes/crw_fake_web_state_policy_decider.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Test fixture to test WebStatePolicyDeciderBridge class.
class WebStatePolicyDeciderBridgeTest : public PlatformTest {
 public:
  WebStatePolicyDeciderBridgeTest()
      : decider_([[CRWFakeWebStatePolicyDecider alloc] init]),
        decider_bridge_(&test_web_state_, decider_) {}

 protected:
  web::TestWebState test_web_state_;
  CRWFakeWebStatePolicyDecider* decider_;
  WebStatePolicyDeciderBridge decider_bridge_;
};

// Tests |shouldAllowRequest:requestInfo:| forwarding.
TEST_F(WebStatePolicyDeciderBridgeTest, ShouldAllowRequest) {
  ASSERT_FALSE([decider_ shouldAllowRequestInfo]);
  NSURL* url = [NSURL URLWithString:@"http://test.url"];
  NSURLRequest* request = [NSURLRequest requestWithURL:url];
  ui::PageTransition transition_type = ui::PageTransition::PAGE_TRANSITION_LINK;
  bool target_frame_is_main = true;
  bool has_user_gesture = false;
  WebStatePolicyDecider::RequestInfo request_info(
      transition_type, target_frame_is_main, has_user_gesture);
  decider_bridge_.ShouldAllowRequest(request, request_info);
  FakeShouldAllowRequestInfo* should_allow_request_info =
      [decider_ shouldAllowRequestInfo];
  ASSERT_TRUE(should_allow_request_info);
  EXPECT_EQ(request, should_allow_request_info->request);
  EXPECT_EQ(target_frame_is_main,
            should_allow_request_info->request_info.target_frame_is_main);
  EXPECT_EQ(has_user_gesture,
            should_allow_request_info->request_info.has_user_gesture);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      transition_type,
      should_allow_request_info->request_info.transition_type));
}

// Tests |shouldAllowResponse:forMainFrame:| forwarding.
TEST_F(WebStatePolicyDeciderBridgeTest, ShouldAllowResponse) {
  ASSERT_FALSE([decider_ shouldAllowResponseInfo]);
  NSURL* url = [NSURL URLWithString:@"http://test.url"];
  NSURLResponse* response = [[NSURLResponse alloc] initWithURL:url
                                                      MIMEType:@"text/html"
                                         expectedContentLength:0
                                              textEncodingName:nil];
  bool for_main_frame = true;
  decider_bridge_.ShouldAllowResponse(response, for_main_frame);
  FakeShouldAllowResponseInfo* should_allow_response_info =
      [decider_ shouldAllowResponseInfo];
  ASSERT_TRUE(should_allow_response_info);
  EXPECT_EQ(response, should_allow_response_info->response);
  EXPECT_EQ(for_main_frame, should_allow_response_info->for_main_frame);
}

}  // namespace web

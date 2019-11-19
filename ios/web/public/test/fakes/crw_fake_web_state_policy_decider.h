// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_WEB_STATE_POLICY_DECIDER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_WEB_STATE_POLICY_DECIDER_H_

#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class NSURLRequest;
@class NSURLResponse;

namespace web {

// Arguments passed to |shouldAllowRequest:requestInfo:|.
struct FakeShouldAllowRequestInfo {
  FakeShouldAllowRequestInfo();
  ~FakeShouldAllowRequestInfo();
  NSURLRequest* request = nil;
  WebStatePolicyDecider::RequestInfo request_info;
};

// Arguments passed to |shouldAllowResponse:forMainFrame:|.
struct FakeShouldAllowResponseInfo {
  NSURLResponse* response = nil;
  BOOL for_main_frame = NO;
};

}  // namespace web

// Test implementation of CRWWebStatePolicyDecider protocol.
@interface CRWFakeWebStatePolicyDecider : NSObject<CRWWebStatePolicyDecider>
// Arguments passed to |shouldAllowRequest:requestInfo:|.
@property(nonatomic, readonly)
    web::FakeShouldAllowRequestInfo* shouldAllowRequestInfo;
// Arguments passed to |shouldAllowResponse:forMainFrame:|.
@property(nonatomic, readonly)
    web::FakeShouldAllowResponseInfo* shouldAllowResponseInfo;

@end

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_WEB_STATE_POLICY_DECIDER_H_

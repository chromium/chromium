// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_BRIDGE_H_
#define IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/navigation/web_state_policy_decider.h"

// Objective-C interface for web::WebStatePolicyDecider.
@protocol CRWWebStatePolicyDecider <NSObject>
@optional

// Invoked by |WebStatePolicyDeciderBridge::ShouldAllowRequest|.
- (BOOL)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:
                   (const web::WebStatePolicyDecider::RequestInfo&)requestInfo;

// Invoked by |WebStatePolicyDeciderBridge::ShouldAllowResponse|.
- (BOOL)shouldAllowResponse:(NSURLResponse*)response
               forMainFrame:(BOOL)forMainFrame;
@end

namespace web {

// Adapter to use an id<CRWWebStatePolicyDecider> as a
// web::WebStatePolicyDecider.
class WebStatePolicyDeciderBridge : public web::WebStatePolicyDecider {
 public:
  WebStatePolicyDeciderBridge(web::WebState* web_state,
                              id<CRWWebStatePolicyDecider> decider);
  ~WebStatePolicyDeciderBridge() override;

  // web::WebStatePolicyDecider methods.
  bool ShouldAllowRequest(NSURLRequest* request,
                          const RequestInfo& request_info) override;

  bool ShouldAllowResponse(NSURLResponse* response,
                           bool for_main_frame) override;

 private:
  // CRWWebStatePolicyDecider which receives forwarded calls.
  __weak id<CRWWebStatePolicyDecider> decider_ = nil;

  DISALLOW_COPY_AND_ASSIGN(WebStatePolicyDeciderBridge);
};

}  // web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_BRIDGE_H_

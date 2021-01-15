// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_web_state_policy_decider.h"

#include <memory>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FakeShouldAllowRequestInfo::FakeShouldAllowRequestInfo()
    : request_info(ui::PageTransition::PAGE_TRANSITION_FIRST,
                   /*target_frame_is_main=*/false,
                   /*target_frame_is_cross_origin=*/false,
                   /*has_user_gesture=*/false) {}
FakeShouldAllowRequestInfo::~FakeShouldAllowRequestInfo() = default;

}  // namespace web

@implementation CRWFakeWebStatePolicyDecider {
  // Arguments passed to |shouldAllowRequest:requestInfo:|.
  std::unique_ptr<web::FakeShouldAllowRequestInfo> _shouldAllowRequestInfo;
  // Arguments passed to
  // |decidePolicyForNavigationResponse:forMainFrame:completionHandler:|.
  std::unique_ptr<web::FakeDecidePolicyForNavigationResponseInfo>
      _decidePolicyForNavigationResponseInfo;
}

- (web::FakeShouldAllowRequestInfo*)shouldAllowRequestInfo {
  return _shouldAllowRequestInfo.get();
}

- (web::FakeDecidePolicyForNavigationResponseInfo*)
    decidePolicyForNavigationResponseInfo {
  return _decidePolicyForNavigationResponseInfo.get();
}

#pragma mark CRWWebStatePolicyDecider methods -

- (web::WebStatePolicyDecider::PolicyDecision)
    shouldAllowRequest:(NSURLRequest*)request
           requestInfo:
               (const web::WebStatePolicyDecider::RequestInfo&)requestInfo {
  _shouldAllowRequestInfo = std::make_unique<web::FakeShouldAllowRequestInfo>();
  _shouldAllowRequestInfo->request = request;
  _shouldAllowRequestInfo->request_info = requestInfo;
  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}

- (void)
    decidePolicyForNavigationResponse:(NSURLResponse*)response
                         forMainFrame:(BOOL)forMainFrame
                    completionHandler:
                        (void (^)(web::WebStatePolicyDecider::PolicyDecision))
                            completionHandler {
  _decidePolicyForNavigationResponseInfo =
      std::make_unique<web::FakeDecidePolicyForNavigationResponseInfo>();
  _decidePolicyForNavigationResponseInfo->response = response;
  _decidePolicyForNavigationResponseInfo->for_main_frame = forMainFrame;
  completionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

@end

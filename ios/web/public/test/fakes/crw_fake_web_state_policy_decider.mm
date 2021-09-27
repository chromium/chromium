// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_web_state_policy_decider.h"

#include <memory>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FakeShouldAllowRequestInfo::FakeShouldAllowRequestInfo(
    NSURLRequest* request,
    WebStatePolicyDecider::RequestInfo request_info)
    : request(request), request_info(request_info) {}

FakeShouldAllowRequestInfo::~FakeShouldAllowRequestInfo() = default;

FakeDecidePolicyForNavigationResponseInfo::
    FakeDecidePolicyForNavigationResponseInfo(NSURLResponse* response,
                                              BOOL for_main_frame)
    : response(response), for_main_frame(for_main_frame) {}

FakeDecidePolicyForNavigationResponseInfo::
    ~FakeDecidePolicyForNavigationResponseInfo() = default;

}  // namespace web

@implementation CRWFakeWebStatePolicyDecider {
  // Arguments passed to |shouldAllowRequest:requestInfo:|.
  std::unique_ptr<web::FakeShouldAllowRequestInfo> _shouldAllowRequestInfo;
  // Arguments passed to
  // |decidePolicyForNavigationResponse:forMainFrame:completionHandler:|.
  std::unique_ptr<web::FakeDecidePolicyForNavigationResponseInfo>
      _decidePolicyForNavigationResponseInfo;
}

- (const web::FakeShouldAllowRequestInfo*)shouldAllowRequestInfo {
  return _shouldAllowRequestInfo.get();
}

- (const web::FakeDecidePolicyForNavigationResponseInfo*)
    decidePolicyForNavigationResponseInfo {
  return _decidePolicyForNavigationResponseInfo.get();
}

#pragma mark CRWWebStatePolicyDecider methods -

- (void)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(web::WebStatePolicyDecider::RequestInfo)requestInfo
           decisionHandler:(PolicyDecisionHandler)decisionHandler {
  _shouldAllowRequestInfo =
      std::make_unique<web::FakeShouldAllowRequestInfo>(request, requestInfo);
  decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

- (void)decidePolicyForNavigationResponse:(NSURLResponse*)response
                             forMainFrame:(BOOL)forMainFrame
                          decisionHandler:
                              (PolicyDecisionHandler)decisionHandler {
  _decidePolicyForNavigationResponseInfo =
      std::make_unique<web::FakeDecidePolicyForNavigationResponseInfo>(
          response, forMainFrame);
  decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

@end

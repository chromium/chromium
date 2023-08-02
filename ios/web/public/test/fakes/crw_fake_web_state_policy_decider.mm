// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_web_state_policy_decider.h"

#import <memory>

namespace web {

FakeShouldAllowRequestInfo::FakeShouldAllowRequestInfo(
    NSURLRequest* request,
    WebStatePolicyDecider::RequestInfo request_info)
    : request(request), request_info(request_info) {}

FakeShouldAllowRequestInfo::~FakeShouldAllowRequestInfo() = default;

FakeDecidePolicyForNavigationResponseInfo::
    FakeDecidePolicyForNavigationResponseInfo(
        NSURLResponse* response,
        WebStatePolicyDecider::ResponseInfo response_info)
    : response(response), response_info(response_info) {}

FakeDecidePolicyForNavigationResponseInfo::
    ~FakeDecidePolicyForNavigationResponseInfo() = default;

}  // namespace web

@implementation CRWFakeWebStatePolicyDecider {
  // Arguments passed to `shouldAllowRequest:requestInfo:`.
  std::unique_ptr<web::FakeShouldAllowRequestInfo> _shouldAllowRequestInfo;
  // Arguments passed to
  // `decidePolicyForNavigationResponse:responseInfo:completionHandler:`.
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

- (void)
    decidePolicyForNavigationResponse:(NSURLResponse*)response
                         responseInfo:(web::WebStatePolicyDecider::ResponseInfo)
                                          responseInfo
                      decisionHandler:(PolicyDecisionHandler)decisionHandler {
  _decidePolicyForNavigationResponseInfo =
      std::make_unique<web::FakeDecidePolicyForNavigationResponseInfo>(
          response, responseInfo);
  decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

@end

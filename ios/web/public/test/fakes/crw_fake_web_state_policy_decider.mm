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
                   /*has_user_gesture=*/false) {}
FakeShouldAllowRequestInfo::~FakeShouldAllowRequestInfo() = default;

}  // namespace web

@implementation CRWFakeWebStatePolicyDecider {
  // Arguments passed to |shouldAllowRequest:requestInfo:|.
  std::unique_ptr<web::FakeShouldAllowRequestInfo> _shouldAllowRequestInfo;
  // Arguments passed to |shouldAllowResponse:forMainFrame:|.
  std::unique_ptr<web::FakeShouldAllowResponseInfo> _shouldAllowResponseInfo;
}

- (web::FakeShouldAllowRequestInfo*)shouldAllowRequestInfo {
  return _shouldAllowRequestInfo.get();
}

- (web::FakeShouldAllowResponseInfo*)shouldAllowResponseInfo {
  return _shouldAllowResponseInfo.get();
}

#pragma mark CRWWebStatePolicyDecider methods -

- (BOOL)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:
                   (const web::WebStatePolicyDecider::RequestInfo&)requestInfo {
  _shouldAllowRequestInfo = std::make_unique<web::FakeShouldAllowRequestInfo>();
  _shouldAllowRequestInfo->request = request;
  _shouldAllowRequestInfo->request_info = requestInfo;
  return YES;
}

- (BOOL)shouldAllowResponse:(NSURLResponse*)response
               forMainFrame:(BOOL)forMainFrame {
  _shouldAllowResponseInfo =
      std::make_unique<web::FakeShouldAllowResponseInfo>();
  _shouldAllowResponseInfo->response = response;
  _shouldAllowResponseInfo->for_main_frame = forMainFrame;
  return YES;
}

@end

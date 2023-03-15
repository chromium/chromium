// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/supervised_user_url_filter_tab_helper.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/net/protocol_handler_util.h"
#import "net/base/mac/url_conversions.h"
#import "net/base/net_errors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kFilteredURLExample[] = "/filtered";

NSError* CreateBlockedUrlError() {
  return [NSError errorWithDomain:net::kNSErrorDomain
                             code:net::ERR_BLOCKED_BY_ADMINISTRATOR
                         userInfo:nil];
}

}  // namespace

SupervisedUserURLFilterTabHelper::~SupervisedUserURLFilterTabHelper() = default;

SupervisedUserURLFilterTabHelper::SupervisedUserURLFilterTabHelper(
    web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

void SupervisedUserURLFilterTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  // TODO(b/265761985): integrate with SupervisedUserService::GetURLFilter().
  if ([request.URL.absoluteString containsString:@(kFilteredURLExample)]) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
            CreateBlockedUrlError()));
    return;
  }

  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

WEB_STATE_USER_DATA_KEY_IMPL(SupervisedUserURLFilterTabHelper)

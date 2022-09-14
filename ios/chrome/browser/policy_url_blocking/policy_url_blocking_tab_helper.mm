// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_tab_helper.h"

#import "components/policy/core/browser/url_blocklist_manager.h"
#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_service.h"
#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PolicyUrlBlockingTabHelper::~PolicyUrlBlockingTabHelper() = default;

PolicyUrlBlockingTabHelper::PolicyUrlBlockingTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

void PolicyUrlBlockingTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  GURL gurl = net::GURLWithNSURL(request.URL);
  PolicyBlocklistService* blocklistService =
      PolicyBlocklistServiceFactory::GetForBrowserState(
          web_state()->GetBrowserState());
  if (blocklistService->GetURLBlocklistState(gurl) ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
            policy_url_blocking_util::CreateBlockedUrlError()));
  }

  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

WEB_STATE_USER_DATA_KEY_IMPL(PolicyUrlBlockingTabHelper)

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_tab_helper.h"

#include "components/policy/core/browser/url_blocklist_manager.h"
#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_service.h"
#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PolicyUrlBlockingTabHelper::~PolicyUrlBlockingTabHelper() = default;

PolicyUrlBlockingTabHelper::PolicyUrlBlockingTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

web::WebStatePolicyDecider::PolicyDecision
PolicyUrlBlockingTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {

  GURL gurl = net::GURLWithNSURL(request.URL);
  PolicyBlocklistService* blocklistService =
      PolicyBlocklistServiceFactory::GetForBrowserState(
          web_state()->GetBrowserState());
  if (blocklistService->GetURLBlocklistState(gurl) ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
        policy_url_blocking_util::CreateBlockedUrlError());
  }

  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}

WEB_STATE_USER_DATA_KEY_IMPL(PolicyUrlBlockingTabHelper)

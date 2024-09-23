// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_tab_helper.h"

#import "components/policy/core/browser/url_blocklist_manager.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "net/base/apple/url_conversions.h"

PolicyUrlBlockingTabHelper::~PolicyUrlBlockingTabHelper() = default;

PolicyUrlBlockingTabHelper::PolicyUrlBlockingTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

void PolicyUrlBlockingTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  GURL gurl = net::GURLWithNSURL(request.URL);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state()->GetBrowserState());
  PolicyBlocklistService* blocklistService =
      PolicyBlocklistServiceFactory::GetForProfile(profile);
  if (blocklistService->GetURLBlocklistState(gurl) ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
            policy_url_blocking_util::CreateBlockedUrlError()));
  }

  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

WEB_STATE_USER_DATA_KEY_IMPL(PolicyUrlBlockingTabHelper)

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_tab_helper.h"

#import "components/policy/core/browser/url_list/policy_blocklist_service.h"
#import "components/policy/core/browser/url_list/url_blocklist_manager.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service_factory.h"
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
  PolicyBlocklistService* service =
      PolicyBlocklistServiceFactory::GetForProfile(profile);
  PolicyBlocklistService::PolicyBlocklistState blocklist_state =
      service->GetURLBlocklistStateWithPolicySource(gurl);
  if (blocklist_state.url_blocklist_state ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
            policy_url_blocking_util::CreateBlockedUrlError(
                blocklist_state.policy_source)));
  }

  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_only_mode_upgrade_tab_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_allowlist.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_blocking_page.h"
#include "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_controller_client.h"
#include "ios/components/security_interstitials/https_only_mode/https_only_mode_error.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_error.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_upgrade_tab_helper.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "net/base/mac/url_conversions.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::https_only_mode::Event;

namespace {

void RecordUMA(Event event) {
  base::UmaHistogramEnumeration(
      security_interstitials::https_only_mode::kEventHistogram, event);
}

}  // namespace

HttpsOnlyModeUpgradeTabHelper::~HttpsOnlyModeUpgradeTabHelper() = default;

void HttpsOnlyModeUpgradeTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

void HttpsOnlyModeUpgradeTabHelper::WebStateDestroyed() {}

// static
GURL HttpsOnlyModeUpgradeTabHelper::GetUpgradedHttpsUrl(
    const GURL& http_url,
    int https_port_for_testing,
    bool use_fake_https_for_testing) {
  DCHECK_EQ(url::kHttpScheme, http_url.scheme());
  GURL::Replacements replacements;

  // This needs to be in scope when ReplaceComponents() is called:
  const std::string port_str = base::NumberToString(https_port_for_testing);
  DCHECK(https_port_for_testing || !use_fake_https_for_testing);
  if (https_port_for_testing) {
    // We'll only get here in tests. Tests should always have a non-default
    // port on the input text.
    DCHECK(!http_url.port().empty());
    replacements.SetPortStr(port_str);
  }
  if (!use_fake_https_for_testing) {
    replacements.SetSchemeStr(url::kHttpsScheme);
  }
  return http_url.ReplaceComponents(replacements);
}

void HttpsOnlyModeUpgradeTabHelper::SetHttpsPortForTesting(
    int https_port_for_testing) {
  https_port_for_testing_ = https_port_for_testing;
}

int HttpsOnlyModeUpgradeTabHelper::GetHttpsPortForTesting() {
  return https_port_for_testing_;
}

void HttpsOnlyModeUpgradeTabHelper::SetHttpPortForTesting(
    int http_port_for_testing) {
  http_port_for_testing_ = http_port_for_testing;
}

int HttpsOnlyModeUpgradeTabHelper::GetHttpPortForTesting() {
  return http_port_for_testing_;
}

void HttpsOnlyModeUpgradeTabHelper::UseFakeHTTPSForTesting(
    bool use_fake_https_for_testing) {
  use_fake_https_for_testing_ = use_fake_https_for_testing;
}

bool HttpsOnlyModeUpgradeTabHelper::IsFakeHTTPSForTesting(
    const GURL& url) const {
  return url.IntPort() == https_port_for_testing_;
}

bool HttpsOnlyModeUpgradeTabHelper::IsHttpAllowedForUrl(const GURL& url) const {
  // TODO(crbug.com/1302509): Allow HTTP for IP addresses when not running
  // tests. If the URL is in the allowlist, don't show any warning.
  HttpsOnlyModeAllowlist* allow_list =
      HttpsOnlyModeAllowlist::FromWebState(web_state());
  return allow_list->IsHttpAllowedForHost(url.host());
}

HttpsOnlyModeUpgradeTabHelper::HttpsOnlyModeUpgradeTabHelper(
    web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state), was_upgraded_(false) {
  web_state->AddObserver(this);
}

void HttpsOnlyModeUpgradeTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  navigation_transition_type_ = navigation_context->GetPageTransition();
  navigation_is_renderer_initiated_ = navigation_context->IsRendererInitiated();
}

void HttpsOnlyModeUpgradeTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }

  if (stopped_loading_to_upgrade_) {
    DCHECK(!was_upgraded_);
    // Start an upgraded navigation.
    RecordUMA(Event::kUpgradeAttempted);
    stopped_loading_to_upgrade_ = false;
    was_upgraded_ = true;
    web::NavigationManager::WebLoadParams params(upgraded_https_url_);
    params.transition_type = navigation_transition_type_;
    params.is_renderer_initiated = navigation_is_renderer_initiated_;
    params.referrer = referrer_;
    params.is_using_https_as_default_scheme = true;
    web_state->GetNavigationManager()->LoadURLWithParams(params);
    return;
  }

  if (!was_upgraded_) {
    return;
  }
  was_upgraded_ = false;

  if (navigation_context->IsFailedHTTPSUpgrade()) {
    RecordUMA(Event::kUpgradeFailed);
    is_http_fallback_navigation_ = true;
    // Start a new navigation to the original HTTP page. We'll then show an
    // interstitial for this navigation in ShouldAllowResponse().
    web::NavigationManager::WebLoadParams params(http_url_);
    params.transition_type = navigation_transition_type_;
    params.is_renderer_initiated = navigation_is_renderer_initiated_;
    params.referrer = referrer_;
    params.is_using_https_as_default_scheme = true;
    web_state->GetNavigationManager()->LoadURLWithParams(params);
    return;
  }

  if (navigation_context->GetUrl().SchemeIs(url::kHttpsScheme) ||
      IsFakeHTTPSForTesting(navigation_context->GetUrl())) {
    RecordUMA(Event::kUpgradeSucceeded);
    return;
  }
}

void HttpsOnlyModeUpgradeTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    WebStatePolicyDecider::RequestInfo request_info,
    WebStatePolicyDecider::PolicyDecisionCallback callback) {
  // Ignore subframe requests.
  if (!request_info.target_frame_is_main) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }
  DCHECK(!stopped_loading_to_upgrade_);
  // Show an interstitial if this is a fallback HTTP navigation.
  if (!is_http_fallback_navigation_) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }
  // If the URL is in the allowlist, don't show any warning. This can happen
  // if another tab allowlists the host before we initiate the fallback
  // navigation.
  DCHECK(!was_upgraded_);
  is_http_fallback_navigation_ = false;
  GURL url = net::GURLWithNSURL(request.URL);
  if (IsHttpAllowedForUrl(url)) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }
  DCHECK(url.SchemeIs(url::kHttpScheme) && !IsFakeHTTPSForTesting(url));
  DCHECK(!was_upgraded_);
  HttpsOnlyModeContainer* container =
      HttpsOnlyModeContainer::FromWebState(web_state());
  container->SetHttpUrl(http_url_);
  std::move(callback).Run(CreateHttpsOnlyModeErrorDecision());
}

void HttpsOnlyModeUpgradeTabHelper::ShouldAllowResponse(
    NSURLResponse* response,
    WebStatePolicyDecider::ResponseInfo response_info,
    base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
        callback) {
  GURL url = net::GURLWithNSURL(response.URL);
  // Ignore subframe navigations and schemes that we don't care about.
  // TODO(crbug.com/1302509): Exclude prerender navigations here.
  if (!response_info.for_main_frame ||
      !(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }
  DCHECK(!is_http_fallback_navigation_);

  // If the URL is in the allowlist, don't upgrade.
  if (IsHttpAllowedForUrl(url)) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  // Upgrade to HTTPS.
  if (url.SchemeIs(url::kHttpScheme) && !IsFakeHTTPSForTesting(url) &&
      !is_http_fallback_navigation_) {
    web::NavigationItem* item_pending =
        web_state()->GetNavigationManager()->GetPendingItem();
    DCHECK(!stopped_loading_to_upgrade_);
    DCHECK(!item_pending->IsUpgradedToHttps());
    // Copy navigation parameters, then cancel the current navigation.
    http_url_ = url;
    referrer_ = item_pending->GetReferrer();
    upgraded_https_url_ = GetUpgradedHttpsUrl(url, https_port_for_testing_,
                                              use_fake_https_for_testing_);
    DCHECK(upgraded_https_url_.is_valid());
    stopped_loading_to_upgrade_ = true;
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Cancel());
    return;
  }
  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

WEB_STATE_USER_DATA_KEY_IMPL(HttpsOnlyModeUpgradeTabHelper)

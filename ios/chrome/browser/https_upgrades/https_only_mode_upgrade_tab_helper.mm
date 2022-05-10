// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/https_only_mode_upgrade_tab_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/https_upgrades/https_upgrade_service_factory.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_blocking_page.h"
#include "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_controller_client.h"
#include "ios/components/security_interstitials/https_only_mode/https_only_mode_error.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_error.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"
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

    // Change the URL to help with debugging.
    if (use_fake_https_for_testing)
      replacements.SetRefStr("fake-https");
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

void HttpsOnlyModeUpgradeTabHelper::SetHttpPortForTesting(
    int http_port_for_testing) {
  http_port_for_testing_ = http_port_for_testing;
}

void HttpsOnlyModeUpgradeTabHelper::UseFakeHTTPSForTesting(
    bool use_fake_https_for_testing) {
  use_fake_https_for_testing_ = use_fake_https_for_testing;
}

void HttpsOnlyModeUpgradeTabHelper::SetFallbackDelayForTesting(
    base::TimeDelta delay) {
  fallback_delay_ = delay;
}

bool HttpsOnlyModeUpgradeTabHelper::IsTimerRunningForTesting() const {
  return timer_.IsRunning();
}

void HttpsOnlyModeUpgradeTabHelper::ClearAllowlistForTesting() {
  HttpsUpgradeService* service = HttpsUpgradeServiceFactory::GetForBrowserState(
      web_state()->GetBrowserState());
  service->ClearAllowlist();
}

bool HttpsOnlyModeUpgradeTabHelper::IsFakeHTTPSForTesting(
    const GURL& url) const {
  return url.IntPort() == https_port_for_testing_;
}

bool HttpsOnlyModeUpgradeTabHelper::IsHttpAllowedForUrl(const GURL& url) const {
  // TODO(crbug.com/1302509): Allow HTTP for IP addresses when not running
  // tests. If the URL is in the allowlist, don't show any warning.
  HttpsUpgradeService* service = HttpsUpgradeServiceFactory::GetForBrowserState(
      web_state()->GetBrowserState());
  return service->IsHttpAllowedForHost(url.host());
}

// static
void HttpsOnlyModeUpgradeTabHelper::CreateForWebState(web::WebState* web_state,
                                                      PrefService* prefs) {
  DCHECK(web_state);
  DCHECK(prefs);
  if (!FromWebState(web_state)) {
    PrerenderService* prerender_service =
        PrerenderServiceFactory::GetForBrowserState(
            ChromeBrowserState::FromBrowserState(web_state->GetBrowserState()));
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new HttpsOnlyModeUpgradeTabHelper(
                               web_state, prefs, prerender_service)));
  }
}

HttpsOnlyModeUpgradeTabHelper::HttpsOnlyModeUpgradeTabHelper(
    web::WebState* web_state,
    PrefService* prefs,
    PrerenderService* prerender_service)
    : web::WebStatePolicyDecider(web_state),
      was_upgraded_(false),
      prefs_(prefs),
      prerender_service_(prerender_service) {
  web_state->AddObserver(this);
}

void HttpsOnlyModeUpgradeTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  if (was_upgraded_) {
    DCHECK(!timer_.IsRunning());
    // |timer_| is deleted when the tab helper is deleted, so it's safe to use
    // Unretained here.
    timer_.Start(
        FROM_HERE, fallback_delay_,
        base::BindOnce(&HttpsOnlyModeUpgradeTabHelper::OnHttpsLoadTimeout,
                       base::Unretained(this)));
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
    DCHECK(!stopped_with_timeout_);
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

  if (stopped_with_timeout_) {
    DCHECK(!timer_.IsRunning());
    stopped_with_timeout_ = false;
    RecordUMA(Event::kUpgradeTimedOut);
    FallbackToHttp();
    return;
  }

  if (!was_upgraded_) {
    return;
  }
  was_upgraded_ = false;
  // The upgrade either failed or succeeded. In both cases, stop the timer.
  timer_.Stop();

  if (navigation_context->IsFailedHTTPSUpgrade()) {
    RecordUMA(Event::kUpgradeFailed);
    FallbackToHttp();
    return;
  }

  if (navigation_context->GetUrl().SchemeIs(url::kHttpsScheme) ||
      IsFakeHTTPSForTesting(navigation_context->GetUrl())) {
    RecordUMA(Event::kUpgradeSucceeded);
    return;
  }
}

void HttpsOnlyModeUpgradeTabHelper::FallbackToHttp() {
  DCHECK(!was_upgraded_);
  DCHECK(!stopped_with_timeout_);
  DCHECK(!timer_.IsRunning());
  DCHECK(!is_http_fallback_navigation_);
  is_http_fallback_navigation_ = true;
  // Start a new navigation to the original HTTP page. We'll then show an
  // interstitial for this navigation in ShouldAllowRequest().
  web::NavigationManager::WebLoadParams params(http_url_);
  params.transition_type = navigation_transition_type_;
  params.is_renderer_initiated = navigation_is_renderer_initiated_;
  params.referrer = referrer_;
  params.is_using_https_as_default_scheme = true;
  // Post a task to navigate to the fallback URL. We don't want to navigate
  // synchronously from a DidNavigationFinish() call.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<web::WebState> web_state,
             const web::NavigationManager::WebLoadParams& params) {
            if (web_state)
              web_state->GetNavigationManager()->LoadURLWithParams(params);
          },
          web_state()->GetWeakPtr(), std::move(params)));
}

void HttpsOnlyModeUpgradeTabHelper::OnHttpsLoadTimeout() {
  DCHECK(!stopped_with_timeout_);
  DCHECK(was_upgraded_);
  stopped_with_timeout_ = true;
  was_upgraded_ = false;
  web_state()->Stop();
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
  // This is a fallback navigation, no need to keep the slow upgrade timer
  // running.
  timer_.Stop();

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
  // Fallback navigations are handled in ShouldAllowRequest().
  DCHECK(!is_http_fallback_navigation_);

  // If the URL is in the allowlist, don't upgrade.
  if (IsHttpAllowedForUrl(url)) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }
  // If already HTTPS (real or faux), simply allow the response.
  if (url.SchemeIs(url::kHttpsScheme) || IsFakeHTTPSForTesting(url)) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  web::NavigationItem* item_pending =
      web_state()->GetNavigationManager()->GetPendingItem();
  DCHECK(item_pending);
  // Upgrade to HTTPS if the navigation wasn't upgraded before.
  if (!item_pending->IsUpgradedToHttps()) {
    if (!prefs_ || !prefs_->GetBoolean(prefs::kHttpsOnlyModeEnabled)) {
      // If the feature is disabled, don't upgrade.
      std::move(callback).Run(
          web::WebStatePolicyDecider::PolicyDecision::Allow());
      return;
    }
    // If the tab is being prerendered, cancel the HTTP response.
    if (prerender_service_ &&
        prerender_service_->IsWebStatePrerendered(web_state())) {
      prerender_service_->CancelPrerender();
      std::move(callback).Run(
          web::WebStatePolicyDecider::PolicyDecision::Cancel());
      return;
    }
    DCHECK(!stopped_loading_to_upgrade_);
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

  // The navigation was already upgraded but landed on an HTTP URL, possibly
  // through redirects (e.g. upgraded HTTPS -> HTTP). In this case, show the
  // interstitial.
  // Note that this doesn't handle HTTP URLs in the middle of redirects such as
  // HTTPS -> HTTP -> HTTPS. The alternative is to do this check in
  // ShouldAllowRequest(), but we don't have enough information there to ensure
  // whether the HTTP URL is part of the redirect chain or a completely new
  // navigation.
  // This is a divergence from the desktop implementation of this feature which
  // relies on a redirect loop triggering a net error.
  RecordUMA(Event::kUpgradeFailed);
  DCHECK(was_upgraded_);
  DCHECK(timer_.IsRunning());
  was_upgraded_ = false;
  timer_.Stop();
  HttpsOnlyModeContainer* container =
      HttpsOnlyModeContainer::FromWebState(web_state());
  container->SetHttpUrl(url);
  std::move(callback).Run(CreateHttpsOnlyModeErrorDecision());
}

WEB_STATE_USER_DATA_KEY_IMPL(HttpsOnlyModeUpgradeTabHelper)

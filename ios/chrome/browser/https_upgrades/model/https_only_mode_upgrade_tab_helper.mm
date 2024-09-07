// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/model/https_only_mode_upgrade_tab_helper.h"

#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/prefs/pref_service.h"
#import "components/security_interstitials/core/https_only_mode_metrics.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_blocking_page.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_controller_client.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_error.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/url_constants.h"

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

bool HttpsOnlyModeUpgradeTabHelper::IsTimerRunningForTesting() const {
  return timer_.IsRunning();
}

void HttpsOnlyModeUpgradeTabHelper::ClearAllowlistForTesting() {
  service_->ClearAllowlist(base::Time(), base::Time::Max());
}

bool HttpsOnlyModeUpgradeTabHelper::IsHttpAllowedForUrl(const GURL& url) const {
  return service_->IsHttpAllowedForHost(url.host());
}

HttpsOnlyModeUpgradeTabHelper::HttpsOnlyModeUpgradeTabHelper(
    web::WebState* web_state,
    PrefService* prefs,
    PrerenderService* prerender_service,
    HttpsUpgradeService* service)
    : web::WebStatePolicyDecider(web_state),
      prefs_(prefs),
      prerender_service_(prerender_service),
      service_(service) {
  web_state->AddObserver(this);
}

void HttpsOnlyModeUpgradeTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  if (state_ == State::kUpgraded) {
    DCHECK(!timer_.IsRunning());
    // `timer_` is deleted when the tab helper is deleted, so it's safe to use
    // Unretained here.
    timer_.Start(
        FROM_HERE, service_->GetFallbackDelay(),
        base::BindOnce(&HttpsOnlyModeUpgradeTabHelper::OnHttpsLoadTimeout,
                       base::Unretained(this), web_state->GetWeakPtr()));
    return;
  }
  if (state_ == State::kNone) {
    // Store navigation parameters on initial navigation.
    navigation_transition_type_ = navigation_context->GetPageTransition();
    navigation_is_renderer_initiated_ =
        navigation_context->IsRendererInitiated();
    navigation_is_post_ = navigation_context->IsPost();
  }
}

void HttpsOnlyModeUpgradeTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  navigation_is_post_ = false;
  if (state_ == State::kNone) {
    return;
  }

  if (state_ == State::kStoppedToUpgrade) {
    state_ = State::kUpgraded;
    // Start an upgraded navigation.
    RecordUMA(Event::kUpgradeAttempted);
    web::NavigationManager::WebLoadParams params(upgraded_https_url_);
    params.transition_type = navigation_transition_type_;
    params.is_renderer_initiated = navigation_is_renderer_initiated_;
    params.referrer = referrer_;
    params.https_upgrade_type = web::HttpsUpgradeType::kHttpsOnlyMode;
    web_state->GetNavigationManager()->LoadURLWithParams(params);
    return;
  }

  if (state_ == State::kStoppedWithTimeout ||
      state_ == State::kStoppedToFallback) {
    DCHECK(!timer_.IsRunning());
    RecordUMA(state_ == State::kStoppedWithTimeout ? Event::kUpgradeTimedOut
                                                   : Event::kUpgradeFailed);
    FallbackToHttp();
    return;
  }

  DCHECK(state_ == State::kUpgraded || state_ == State::kDone);
  // The upgrade either failed or succeeded. In both cases, stop the timer.
  timer_.Stop();

  if (navigation_context->GetFailedHttpsUpgradeType() ==
      web::HttpsUpgradeType::kHttpsOnlyMode) {
    RecordUMA(Event::kUpgradeFailed);
    FallbackToHttp();
    return;
  }

  if (state_ == State::kDone &&
      (navigation_context->GetUrl().SchemeIs(url::kHttpsScheme) ||
       service_->IsFakeHTTPSForTesting(navigation_context->GetUrl()))) {
    RecordUMA(Event::kUpgradeSucceeded);
  }
  state_ = State::kNone;
}

void HttpsOnlyModeUpgradeTabHelper::StopToUpgrade(
    const GURL& url,
    const web::Referrer& referrer,
    base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
        callback) {
  state_ = State::kStoppedToUpgrade;
  // Copy navigation parameters, then cancel the current navigation.
  http_url_ = url;
  referrer_ = referrer;
  upgraded_https_url_ = service_->GetUpgradedHttpsUrl(url);
  DCHECK(upgraded_https_url_.is_valid());
  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
}

void HttpsOnlyModeUpgradeTabHelper::FallbackToHttp() {
  DCHECK(state_ == State::kUpgraded || state_ == State::kStoppedWithTimeout ||
         state_ == State::kStoppedToFallback);
  DCHECK(!timer_.IsRunning());
  state_ = State::kFallbackStarted;
  // Start a new navigation to the original HTTP page. We'll then show an
  // interstitial for this navigation in ShouldAllowRequest().
  web::NavigationManager::WebLoadParams params(http_url_);
  params.transition_type = navigation_transition_type_;
  params.is_renderer_initiated = navigation_is_renderer_initiated_;
  params.referrer = referrer_;
  // Even though this is an HTTP navigation, mark it as "upgraded" so that we
  // don't attempt to upgrade it again.
  params.https_upgrade_type = web::HttpsUpgradeType::kHttpsOnlyMode;
  // Post a task to navigate to the fallback URL. We don't want to navigate
  // synchronously from a DidNavigationFinish() call.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<web::WebState> web_state,
             const web::NavigationManager::WebLoadParams& params) {
            if (web_state) {
              web_state->GetNavigationManager()->LoadURLWithParams(params);
            }
          },
          web_state()->GetWeakPtr(), std::move(params)));
}

void HttpsOnlyModeUpgradeTabHelper::ResetState() {
  state_ = State::kNone;
  timer_.Stop();
}

void HttpsOnlyModeUpgradeTabHelper::OnHttpsLoadTimeout(
    base::WeakPtr<web::WebState> weak_web_state) {
  DCHECK(state_ == State::kUpgraded);
  state_ = State::kStoppedWithTimeout;
  if (weak_web_state) {
    weak_web_state->Stop();
  }
}

void HttpsOnlyModeUpgradeTabHelper::ShouldAllowResponse(
    NSURLResponse* response,
    WebStatePolicyDecider::ResponseInfo response_info,
    base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
        callback) {
  GURL url = net::GURLWithNSURL(response.URL);
  // Ignore subframe navigations and schemes that we don't care about.
  if (!response_info.for_main_frame ||
      !(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    ResetState();
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  // If the URL is in the allowlist, don't upgrade.
  if (IsHttpAllowedForUrl(url)) {
    // The URL might have been allowlisted in another tab while we are loading
    // this tab, so we can't make any assumptions about the state. Simply clear
    // it.
    ResetState();
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  // If already HTTPS (real or faux), simply allow the response.
  if (url.SchemeIs(url::kHttpsScheme) || service_->IsFakeHTTPSForTesting(url)) {
    timer_.Stop();
    if (state_ != State::kNone) {
      // Only call it done if the navigation was originally upgraded.
      state_ = State::kDone;
    }
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  if (state_ == State::kFallbackStarted) {
    DCHECK(!timer_.IsRunning());
    state_ = State::kDone;

    // If HTTPS-First Mode is enabled, show the interstitial.
    if (prefs_ && prefs_->GetBoolean(prefs::kHttpsOnlyModeEnabled)) {
      HttpsOnlyModeContainer* container =
          HttpsOnlyModeContainer::FromWebState(web_state());
      container->SetHttpUrl(http_url_);
      std::move(callback).Run(CreateHttpsOnlyModeErrorDecision());
      return;
    }
    // Otherwise, this is a failed HTTPS-Upgrade. Allow the response.
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  web::NavigationItem* item_pending =
      web_state()->GetNavigationManager()->GetPendingItem();
  if (!item_pending) {
    ResetState();
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  // Upgrade to HTTPS if the navigation wasn't upgraded before. Ignore POST
  // navigations.
  if (item_pending->GetHttpsUpgradeType() == web::HttpsUpgradeType::kNone &&
      !navigation_is_post_) {
    if ((!base::FeatureList::IsEnabled(
             security_interstitials::features::kHttpsUpgrades) &&
         !(prefs_ && prefs_->GetBoolean(prefs::kHttpsOnlyModeEnabled))) ||
        service_->IsLocalhost(url)) {
      // Don't upgrade if the feature is disabled or the URL is localhost.
      // See ShouldCreateLoader() function in
      // https_only_mode_upgrade_interceptor.cc for the desktop/Android
      // implementation.
      ResetState();
      std::move(callback).Run(
          web::WebStatePolicyDecider::PolicyDecision::Allow());
      return;
    }
    // If the tab is being prerendered, cancel the HTTP response.
    if (prerender_service_ &&
        prerender_service_->IsWebStatePrerendered(web_state())) {
      RecordUMA(Event::kPrerenderCancelled);
      ResetState();
      std::move(callback).Run(
          web::WebStatePolicyDecider::PolicyDecision::Cancel());
      prerender_service_->CancelPrerender();
      // IMPORTANT: CancelPrerender() destroys the web state. Do not access
      // it after here.
      return;
    }
    StopToUpgrade(url, item_pending->GetReferrer(), std::move(callback));
    return;
  }

  // Omnibox upgrade failures are handled in TypedNavigationUpgradeTabHelper.
  // Ignore them here.
  if (item_pending->GetHttpsUpgradeType() !=
      web::HttpsUpgradeType::kHttpsOnlyMode) {
    DCHECK(state_ == State::kNone);
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  if (state_ == State::kNone && !navigation_is_post_) {
    // If the pending item was a failed upgrade but the upgrade bit wasn't set,
    // this is likely an interstitial reload.
    timer_.Stop();
    StopToUpgrade(url, item_pending->GetReferrer(), std::move(callback));
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
  DCHECK(state_ == State::kUpgraded || state_ == State::kNone);
  timer_.Stop();
  state_ = State::kDone;
  RecordUMA(Event::kUpgradeFailed);

  // If HTTPS-First Mode is enabled, show the interstitial.
  if (prefs_ && prefs_->GetBoolean(prefs::kHttpsOnlyModeEnabled)) {
    HttpsOnlyModeContainer* container =
        HttpsOnlyModeContainer::FromWebState(web_state());
    container->SetHttpUrl(url);
    std::move(callback).Run(CreateHttpsOnlyModeErrorDecision());
    return;
  }
  // Otherwise, this is a failed HTTPS-Upgrade. Allow the response.
  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

WEB_STATE_USER_DATA_KEY_IMPL(HttpsOnlyModeUpgradeTabHelper)

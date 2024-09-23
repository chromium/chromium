// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/model/typed_navigation_upgrade_tab_helper.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/security_interstitials/core/omnibox_https_upgrade_metrics.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_impl.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"
#import "ios/web/public/navigation/https_upgrade_type.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/window_open_disposition.h"

using security_interstitials::omnibox_https_upgrades::Event;
using security_interstitials::omnibox_https_upgrades::kEventHistogram;

namespace {

void RecordUMA(Event event) {
  base::UmaHistogramEnumeration(kEventHistogram, event);
}

}  // namespace

TypedNavigationUpgradeTabHelper::~TypedNavigationUpgradeTabHelper() = default;

TypedNavigationUpgradeTabHelper::TypedNavigationUpgradeTabHelper(
    web::WebState* web_state,
    PrerenderService* prerender_service,
    HttpsUpgradeService* service)
    : prerender_service_(prerender_service), service_(service) {
  web_state->AddObserver(this);
}

bool TypedNavigationUpgradeTabHelper::IsTimerRunningForTesting() const {
  return timer_.IsRunning();
}

void TypedNavigationUpgradeTabHelper::OnHttpsLoadTimeout(
    base::WeakPtr<web::WebState> weak_web_state) {
  DCHECK(state_ == State::kUpgraded);
  state_ = State::kStoppedWithTimeout;
  web::WebState* web_state = weak_web_state.get();
  if (web_state) {
    web_state->Stop();
  }
}

void TypedNavigationUpgradeTabHelper::FallbackToHttp(web::WebState* web_state,
                                                     const GURL& https_url) {
  const GURL http_url = service_->GetHttpUrl(https_url);
  DCHECK(http_url.is_valid());
  state_ = State::kFallbackStarted;
  // Start a new navigation to the original HTTP page.
  web::NavigationManager::WebLoadParams params(http_url);
  params.transition_type = navigation_transition_type_;
  params.is_renderer_initiated = navigation_is_renderer_initiated_;
  params.referrer = referrer_;
  // The fallback navigation is no longer considered upgraded.
  params.https_upgrade_type = web::HttpsUpgradeType::kNone;
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
          web_state->GetWeakPtr(), std::move(params)));
}

void TypedNavigationUpgradeTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  if (prerender_service_ &&
      prerender_service_->IsWebStatePrerendered(web_state)) {
    return;
  }

  web::NavigationItem* item_pending =
      web_state->GetNavigationManager()->GetPendingItem();
  if (item_pending &&
      item_pending->GetHttpsUpgradeType() == web::HttpsUpgradeType::kOmnibox) {
    upgraded_https_url_ = navigation_context->GetUrl();

    // TODO(crbug.com/40230443): Remove this scheme check once fixed. Without
    // the fix, kHttpsLoadStarted bucket is mildly overcounted.
    GURL url = item_pending->GetURL();
    if (url.SchemeIs(url::kHttpsScheme) ||
        service_->IsFakeHTTPSForTesting(url)) {
      // Pending navigation may not always correspond to the initial navigation,
      // e.g. when a new navigation is started before the first one is finished,
      // but we are only using it to record metrics so this is acceptable.
      state_ = State::kUpgraded;
      RecordUMA(Event::kHttpsLoadStarted);
      // `timer_` is deleted when the tab helper is deleted, so it's safe to use
      // Unretained here.
      timer_.Start(
          FROM_HERE, service_->GetFallbackDelay(),
          base::BindOnce(&TypedNavigationUpgradeTabHelper::OnHttpsLoadTimeout,
                         base::Unretained(this), web_state->GetWeakPtr()));
    }
  }
}

void TypedNavigationUpgradeTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument() || state_ == State::kNone) {
    return;
  }
  if (prerender_service_ &&
      prerender_service_->IsWebStatePrerendered(web_state)) {
    return;
  }
  timer_.Stop();

  // Start a fallback navigation if the upgraded navigation failed.
  if (navigation_context->GetFailedHttpsUpgradeType() ==
      web::HttpsUpgradeType::kOmnibox) {
    RecordUMA(Event::kHttpsLoadFailedWithCertError);
    FallbackToHttp(web_state, navigation_context->GetUrl());
    return;
  }

  // Start a fallback navigation if the upgraded navigation timed out.
  if (state_ == State::kStoppedWithTimeout) {
    DCHECK(!timer_.IsRunning());
    RecordUMA(Event::kHttpsLoadTimedOut);
    // TODO(crbug.com/40875679): Cleanup this logic, we should only use
    // upgraded_https_url_ here.
    if (upgraded_https_url_.is_valid()) {
      FallbackToHttp(web_state, upgraded_https_url_);
    } else {
      FallbackToHttp(web_state, navigation_context->GetUrl());
    }
  }

  // Record success.
  if (state_ == State::kUpgraded &&
      (navigation_context->GetUrl().SchemeIs(url::kHttpsScheme) ||
       service_->IsFakeHTTPSForTesting(navigation_context->GetUrl()))) {
    RecordUMA(Event::kHttpsLoadSucceeded);
  }
  state_ = State::kNone;
}

void TypedNavigationUpgradeTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(TypedNavigationUpgradeTabHelper)

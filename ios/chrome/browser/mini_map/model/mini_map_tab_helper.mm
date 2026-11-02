// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/model/mini_map_tab_helper.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/strings/string_util.h"
#import "base/task/sequenced_task_runner.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/mini_map/mini_map_api.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "ui/base/mojom/window_open_disposition.mojom-shared.h"
#import "url/gurl.h"

MiniMapTabHelper::MiniMapTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {
  web_state->AddObserver(this);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  mini_map_service_ = MiniMapServiceFactory::GetForProfile(profile);
}

MiniMapTabHelper::~MiniMapTabHelper() {
  if (policy_callback_) {
    std::move(policy_callback_).Run(PolicyDecision::Cancel());
  }
}

void MiniMapTabHelper::SetMiniMapCommands(
    id<MiniMapCommands> mini_map_handler) {
  mini_map_handler_ = mini_map_handler;
}

#pragma mark - WebStateObserver

void MiniMapTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  is_on_google_srp_ =
      google_util::IsGoogleSearchUrl(web_state->GetLastCommittedURL());
}

void MiniMapTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

#pragma mark - WebStatePolicyDecider

void MiniMapTabHelper::ShouldAllowRequest(NSURLRequest* request,
                                          RequestInfo request_info,
                                          PolicyDecisionCallback callback) {
  if (!request_info.target_frame_is_main || !mini_map_service_) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  if (!ShouldInterceptRequest(request.URL, request_info.transition_type)) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  const GURL request_url = net::GURLWithNSURL(request.URL);
  GURL modified_url =
      ios::provider::URLByAppendingCampaignTokenIfNeeded(request_url);
  if (modified_url == request_url) {
    modified_url =
        net::AppendQueryParameter(request_url, "utm_campaign", "as-npt-bling");
  }

  // User Disabled in Settings (Opt-out):
  if (!mini_map_service_->IsMiniMapEnabled()) {
    CancelAndOpenURL(std::move(callback), modified_url,
                     request_info.transition_type);
    return;
  }

  // Save fallback URL, defer navigation, and present Native Preview sheet.
  PolicyDecisionCallback previous_callback = std::move(policy_callback_);
  pending_treatment_url_ = modified_url;
  pending_transition_type_ = request_info.transition_type;
  policy_callback_ = std::move(callback);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MiniMapTabHelper::PresentMiniMapNativePreview,
                                weak_ptr_factory_.GetWeakPtr(), modified_url));

  if (previous_callback) {
    std::move(previous_callback).Run(PolicyDecision::Allow());
  }
}

void MiniMapTabHelper::OnMiniMapSuccess() {
  pending_treatment_url_ = GURL();
  pending_transition_type_.reset();
  if (policy_callback_) {
    std::move(policy_callback_).Run(PolicyDecision::Cancel());
  }
}

void MiniMapTabHelper::OnMiniMapFailure() {
  GURL url = std::move(pending_treatment_url_);
  pending_treatment_url_ = GURL();
  std::optional<ui::PageTransition> transition_type =
      std::exchange(pending_transition_type_, std::nullopt);
  if (policy_callback_ && transition_type.has_value()) {
    CancelAndOpenURL(std::move(policy_callback_), url, *transition_type);
  }
}

void MiniMapTabHelper::WebStateDestroyed() {
  // Implemented in MiniMapTabHelper::WebStateDestroyed(web::WebState*).
}

#pragma mark - Private

bool MiniMapTabHelper::ShouldInterceptRequest(
    NSURL* url,
    ui::PageTransition page_transition) {
  if (!IsMiniMapUniversalLinkEnabled()) {
    return false;
  }

  GURL target_url = net::GURLWithNSURL(url);
  if (!is_on_google_srp_ || !mini_map_service_) {
    // Only consider links from Google Search results page when service exists.
    return false;
  }

  // If Google Maps is installed and the bypass experiment is disabled, do not
  // intercept; allow standard navigation / Universal Links to handle the URL.
  // When the bypass flag is enabled, we intercept the request so that the
  // downstream provider/handler can directly launch Google Maps via custom
  // schemes, avoiding any intermediate preview sheets and disengaged link
  // issues.
  if (mini_map_service_->IsGoogleMapsInstalled() &&
      !mini_map_service_->IsMapsNativePreviewBypassEnabled()) {
    return false;
  }

  // Consider all link clicks, including those that are redirects (e.g. short
  // links expanding to long links).
  if (!ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_LINK)) {
    // Only consider user initiated link clicks.
    return false;
  }

  // Only consider URLS like maps.google.com/maps/...
  if (!google_util::IsGoogleDomainUrl(
          target_url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return false;
  }

  if (target_url.host().find("maps.google.") == std::string::npos &&
      target_url.host().find("google.com") == std::string::npos) {
    return false;
  }

  if (target_url.path() != "/maps" &&
      !base::StartsWith(target_url.path(), "/maps/")) {
    return false;
  }

  if (!ios::provider::MiniMapCanHandleURL(url)) {
    return false;
  }

  std::string value;
  if (net::GetValueForKeyInQuery(target_url, "utm_campaign", &value) &&
      value == "as-npt-bling") {
    return false;
  }
  if (ios::provider::URLHasCampaignToken(target_url)) {
    return false;
  }

  return true;
}

void MiniMapTabHelper::PresentMiniMapNativePreview(const GURL& url) {
  [mini_map_handler_ presentMiniMapNativePreviewForURL:net::NSURLWithGURL(url)];
}

void MiniMapTabHelper::CancelAndOpenURL(PolicyDecisionCallback callback,
                                        const GURL& url,
                                        ui::PageTransition transition_type) {
  web::WebState::OpenURLParams params(
      url, web::Referrer(), ui::mojom::WindowOpenDisposition::kCurrentTab,
      transition_type,
      /*is_renderer_initiated=*/false);
  // Running `callback` with `PolicyDecision::Cancel()` must happen before
  // `OpenURL()` (so the cancelled navigation's pending item is discarded
  // first), and may synchronously close the WebState and destroy `this`.
  // Chain `OpenURL()` via a WeakPtr so `this` is not accessed if destroyed.
  base::BindOnce(std::move(callback), PolicyDecision::Cancel())
      .Then(base::BindOnce(&MiniMapTabHelper::OpenURL,
                           weak_ptr_factory_.GetWeakPtr(), params))
      .Run();
}

void MiniMapTabHelper::OpenURL(web::WebState::OpenURLParams params) {
  if (web_state()) {
    web_state()->OpenURL(params);
  }
}

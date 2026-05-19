// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/model/mini_map_tab_helper.h"

#import "base/strings/string_util.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/mini_map/mini_map_api.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
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
  if (!request_info.target_frame_is_main) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  if (!ShouldInterceptRequest(request.URL, request_info.transition_type)) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  if (base::FeatureList::IsEnabled(kIOSMiniMapUniversalLinkCounterfactual)) {
    GURL target_url = net::GURLWithNSURL(request.URL);
    GURL modified_url =
        net::AppendQueryParameter(target_url, "utm_campaign", "as-npc-bling");

    std::move(callback).Run(PolicyDecision::Cancel());

    web::WebState::OpenURLParams params(modified_url, web::Referrer(),
                                        WindowOpenDisposition::CURRENT_TAB,
                                        request_info.transition_type,
                                        /*is_renderer_initiated=*/false);
    web_state()->OpenURL(params);
    return;
  }

  if (policy_callback_) {
    std::move(policy_callback_).Run(PolicyDecision::Allow());
  }

  pending_treatment_url_ = net::AppendQueryParameter(
      net::GURLWithNSURL(request.URL), "utm_campaign", "as-npt-bling");
  pending_transition_type_ = request_info.transition_type;
  policy_callback_ = std::move(callback);
}

void MiniMapTabHelper::OnMiniMapSuccess() {
  if (policy_callback_) {
    std::move(policy_callback_).Run(PolicyDecision::Cancel());
  }
  pending_treatment_url_ = GURL();
  pending_transition_type_ = ui::PageTransition::PAGE_TRANSITION_LINK;
}

void MiniMapTabHelper::OnMiniMapFailure() {
  if (policy_callback_) {
    std::move(policy_callback_).Run(PolicyDecision::Cancel());
    web::WebState::OpenURLParams params(pending_treatment_url_, web::Referrer(),
                                        WindowOpenDisposition::CURRENT_TAB,
                                        pending_transition_type_,
                                        /*is_renderer_initiated=*/false);
    web_state()->OpenURL(params);
  }
  pending_treatment_url_ = GURL();
  pending_transition_type_ = ui::PageTransition::PAGE_TRANSITION_LINK;
}

void MiniMapTabHelper::WebStateDestroyed() {
  // Implemented in MiniMapTabHelper::WebStateDestroyed(web::WebState*).
}

#pragma mark - Private

bool MiniMapTabHelper::ShouldInterceptRequest(
    NSURL* url,
    ui::PageTransition page_transition) {
  GURL target_url = net::GURLWithNSURL(url);
  if (!mini_map_service_->IsMiniMapEnabled() &&
      !base::FeatureList::IsEnabled(kIOSMiniMapUniversalLinkCounterfactual)) {
    // Only intercept request when the feature or counterfactual is enabled.
    return false;
  }
  if (!is_on_google_srp_) {
    // Only consider links from Google Search results page.
    return false;
  }
  if (mini_map_service_->IsGoogleMapsInstalled()) {
    // Do not intercept if Google Maps is installed.
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
      (value == "as-npc-bling" || value == "as-npt-bling")) {
    return false;
  }

  if (base::FeatureList::IsEnabled(kIOSMiniMapUniversalLinkCounterfactual)) {
    // Intercept the request to add the UTM campaign parameter.
    // Early return to prevent mini map from opening.
    return true;
  }

  GURL modified_url =
      net::AppendQueryParameter(target_url, "utm_campaign", "as-npt-bling");
  [mini_map_handler_
      presentMiniMapNativePreviewForURL:net::NSURLWithGURL(modified_url)];
  return true;
}

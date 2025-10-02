// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/model/mini_map_tab_helper.h"

#import "components/google/core/common/google_util.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/public/provider/chrome/browser/mini_map/mini_map_api.h"
#import "net/base/apple/url_conversions.h"

MiniMapTabHelper::MiniMapTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {
  web_state->AddObserver(this);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  mini_map_service_ = MiniMapServiceFactory::GetForProfile(profile);
  CHECK(!profile->IsOffTheRecord());
}

MiniMapTabHelper::~MiniMapTabHelper() {}

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
  if (!ShouldInterceptRequest(request.URL, request_info.transition_type)) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }
  std::move(callback).Run(PolicyDecision::Cancel());
}

void MiniMapTabHelper::WebStateDestroyed() {
  // Implemented in MiniMapTabHelper::WebStateDestroyed(web::WebState*).
}

#pragma mark - Private

bool MiniMapTabHelper::ShouldInterceptRequest(
    NSURL* url,
    ui::PageTransition page_transition) {
  if (!mini_map_service_->IsMiniMapEnabled()) {
    // Only intercept request when the feature is enabled.
    return false;
  }
  if (!is_on_google_srp_ || !mini_map_service_->IsDSEGoogle()) {
    // Only consider links from Google Search results page when Google is DSE.
    return false;
  }
  if (mini_map_service_->IsGoogleMapsInstalled()) {
    // Only intercept request when the feature is enabled.
    return false;
  }
  if (!mini_map_service_->IsSignedIn()) {
    // Only intercept request when the user is signed in.
    return false;
  }

  if (!ui::PageTransitionTypeIncludingQualifiersIs(page_transition,
                                                   ui::PAGE_TRANSITION_LINK)) {
    // Only consider user initiated link clicks.
    return false;
  }

  // Only consider URLS like maps.google.com/maps/...
  GURL target_url = net::GURLWithNSURL(url);
  if (!google_util::IsGoogleDomainUrl(
          target_url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return false;
  }

  if (!base::Contains(target_url.host(), "maps.google.")) {
    return false;
  }

  if (!base::StartsWith(target_url.path(), "/maps/")) {
    return false;
  }

  if (!ios::provider::MiniMapCanHandleURL(url)) {
    return false;
  }

  [mini_map_handler_ presentMiniMapForURL:url];
  return true;
}

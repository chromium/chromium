// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/follow_util.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FollowActionState GetFollowActionState(web::WebState* webState) {
  // This method should be called only if the feature flag has been enabled.
  DCHECK(IsWebChannelsEnabled());

  if (!webState) {
    return FollowActionStateHidden;
  }

  ChromeBrowserState* browserState =
      ChromeBrowserState::FromBrowserState(webState->GetBrowserState());
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);

  // Hide the follow action when users have not signed in.
  if (!authenticationService || !authenticationService->GetPrimaryIdentity(
                                    signin::ConsentLevel::kSignin)) {
    return FollowActionStateHidden;
  }

  const GURL& URL = webState->GetLastCommittedURL();
  // Show the follow action when:
  // 1. The page url is valid;
  // 2. Users are not on NTP or Chrome internal pages;
  // 3. Users are not in incognito mode.
  if (URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL) &&
      !browserState->IsOffTheRecord()) {
    return FollowActionStateEnabled;
  }
  return FollowActionStateHidden;
}

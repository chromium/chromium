// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"

#include "base/logging.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeIdentityServiceObserverBridge::ChromeIdentityServiceObserverBridge(
    id<ChromeIdentityServiceObserver> observer)
    : observer_(observer) {
  DCHECK(observer_);
  scoped_observer_.Add(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());
}

ChromeIdentityServiceObserverBridge::~ChromeIdentityServiceObserverBridge() {}

void ChromeIdentityServiceObserverBridge::OnIdentityListChanged() {
  if ([observer_ respondsToSelector:@selector(identityListChanged)])
    [observer_ identityListChanged];
}

void ChromeIdentityServiceObserverBridge::OnAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  if ([observer_
          respondsToSelector:@selector(accessTokenRefreshFailed:userInfo:)]) {
    [observer_ accessTokenRefreshFailed:identity userInfo:user_info];
  }
}

void ChromeIdentityServiceObserverBridge::OnProfileUpdate(
    ChromeIdentity* identity) {
  if ([observer_ respondsToSelector:@selector(profileUpdate:)])
    [observer_ profileUpdate:identity];
}

void ChromeIdentityServiceObserverBridge::
    OnChromeIdentityServiceWillBeDestroyed() {
  if ([observer_
          respondsToSelector:@selector(chromeIdentityServiceWillBeDestroyed)]) {
    [observer_ chromeIdentityServiceWillBeDestroyed];
  }
}

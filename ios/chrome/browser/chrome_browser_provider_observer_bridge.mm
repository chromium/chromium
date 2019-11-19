// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeBrowserProviderObserverBridge::ChromeBrowserProviderObserverBridge(
    id<ChromeBrowserProviderObserver> observer)
    : observer_(observer) {
  DCHECK(observer_);
  scoped_observer_.Add(ios::GetChromeBrowserProvider());
}

ChromeBrowserProviderObserverBridge::~ChromeBrowserProviderObserverBridge() {}

void ChromeBrowserProviderObserverBridge::OnChromeIdentityServiceDidChange(
    ios::ChromeIdentityService* new_identity_service) {
  if ([observer_ respondsToSelector:@selector(chromeIdentityServiceDidChange:)])
    [observer_ chromeIdentityServiceDidChange:new_identity_service];
}

void ChromeBrowserProviderObserverBridge::
    OnChromeBrowserProviderWillBeDestroyed() {
  if ([observer_
          respondsToSelector:@selector(chromeBrowserProviderWillBeDestroyed)]) {
    [observer_ chromeBrowserProviderWillBeDestroyed];
  }
}

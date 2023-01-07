// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeAccountManagerServiceObserverBridge::
    ChromeAccountManagerServiceObserverBridge(
        id<ChromeAccountManagerServiceObserver> observer,
        ChromeAccountManagerService* account_manager_service)
    : observer_(observer) {
  DCHECK(observer_);

  scoped_observation_.Observe(account_manager_service);
}

ChromeAccountManagerServiceObserverBridge::
    ~ChromeAccountManagerServiceObserverBridge() {}

void ChromeAccountManagerServiceObserverBridge::OnIdentityListChanged(
    bool need_user_approval) {
  if ([observer_ respondsToSelector:@selector(identityListChanged)])
    [observer_ identityListChanged];
}

void ChromeAccountManagerServiceObserverBridge::OnIdentityChanged(
    id<SystemIdentity> identity) {
  if ([observer_ respondsToSelector:@selector(identityChanged:)])
    [observer_ identityChanged:identity];
}

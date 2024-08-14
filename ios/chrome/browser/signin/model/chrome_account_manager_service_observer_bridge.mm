// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

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

void ChromeAccountManagerServiceObserverBridge::OnIdentityListChanged() {
  if ([observer_ respondsToSelector:@selector(identityListChanged)]) {
    [observer_ identityListChanged];
  }
}

void ChromeAccountManagerServiceObserverBridge::OnIdentityUpdated(
    id<SystemIdentity> identity) {
  if ([observer_ respondsToSelector:@selector(identityUpdated:)]) {
    [observer_ identityUpdated:identity];
  }
}

void ChromeAccountManagerServiceObserverBridge::
    OnChromeAccountManagerServiceShutdown(
        ChromeAccountManagerService* chrome_account_manager_service) {
  if ([observer_ respondsToSelector:@selector
                 (onChromeAccountManagerServiceShutdown:)]) {
    [observer_
        onChromeAccountManagerServiceShutdown:chrome_account_manager_service];
  }
}

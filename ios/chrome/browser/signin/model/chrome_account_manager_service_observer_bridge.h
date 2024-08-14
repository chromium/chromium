// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

// Objective-C protocol mirroring ChromeAccountManagerService::Observer.
@protocol ChromeAccountManagerServiceObserver <NSObject>
@optional
- (void)identityListChanged;
- (void)identityUpdated:(id<SystemIdentity>)identity;
- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService;
@end

// Simple observer bridge that forwards all events to its delegate observer.
class ChromeAccountManagerServiceObserverBridge
    : public ChromeAccountManagerService::Observer {
 public:
  ChromeAccountManagerServiceObserverBridge(
      id<ChromeAccountManagerServiceObserver> observer,
      ChromeAccountManagerService* account_manager_service);
  ChromeAccountManagerServiceObserverBridge(
      const ChromeAccountManagerServiceObserverBridge&) = delete;
  ChromeAccountManagerServiceObserverBridge& operator=(
      const ChromeAccountManagerServiceObserverBridge&) = delete;
  ~ChromeAccountManagerServiceObserverBridge() override;

 private:
  // ChromeAccountManagerService::Observer implementation.
  void OnIdentityListChanged() override;
  void OnIdentityUpdated(id<SystemIdentity> identity) override;
  void OnChromeAccountManagerServiceShutdown(
      ChromeAccountManagerService* chrome_account_manager_service) override;

  __weak id<ChromeAccountManagerServiceObserver> observer_ = nil;
  base::ScopedObservation<ChromeAccountManagerService,
                          ChromeAccountManagerService::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_OBSERVER_BRIDGE_H_

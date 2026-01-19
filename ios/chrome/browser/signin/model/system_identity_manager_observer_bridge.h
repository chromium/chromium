// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_manager_observer.h"

// Objective-C protocol mirroring SystemIdentityManagerObserver.
@protocol SystemIdentityManagerObserving <NSObject>

@optional
- (void)onIdentityListChanged;
- (void)onIdentityUpdated:(id<SystemIdentity>)identity;

@end

// Simple observer bridge that forwards all events to its delegate observer.
class SystemIdentityManagerObserverBridge
    : public SystemIdentityManagerObserver {
 public:
  SystemIdentityManagerObserverBridge(
      SystemIdentityManager* service,
      id<SystemIdentityManagerObserving> observer);
  ~SystemIdentityManagerObserverBridge() override;

  // SystemIdentityManagerObserver implementation.
  void OnIdentityListChanged() override;
  void OnIdentityUpdated(id<SystemIdentity> identity) override;

 private:
  __weak id<SystemIdentityManagerObserving> observer_ = nil;
  base::ScopedObservation<SystemIdentityManager, SystemIdentityManagerObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_

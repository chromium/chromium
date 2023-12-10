// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer.h"

// Objective-C protocol mirroring AuthenticationService::Observer.
@protocol AuthenticationServiceObserving <NSObject>
- (void)onServiceStatusChanged;
@end

// Simple observer bridge that forwards all events to its delegate observer.
class AuthenticationServiceObserverBridge
    : public AuthenticationServiceObserver {
 public:
  AuthenticationServiceObserverBridge(
      AuthenticationService* service,
      id<AuthenticationServiceObserving> observer);
  ~AuthenticationServiceObserverBridge() override;

  // AuthenticationServiceObserver implementation.
  void OnServiceStatusChanged() override;

 private:
  __weak id<AuthenticationServiceObserving> observer_ = nil;
  base::ScopedObservation<AuthenticationService, AuthenticationServiceObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_OBSERVER_BRIDGE_H_

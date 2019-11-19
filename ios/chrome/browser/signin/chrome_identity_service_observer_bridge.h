// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

// Objective-C protocol mirroring ChromeIdentityService::Observer.
@protocol ChromeIdentityServiceObserver<NSObject>
@optional
- (void)identityListChanged;
- (void)accessTokenRefreshFailed:(ChromeIdentity*)identity
                        userInfo:(NSDictionary*)userInfo;
- (void)profileUpdate:(ChromeIdentity*)identity;
- (void)chromeIdentityServiceWillBeDestroyed;
@end

// Simple observer bridge that forwards all events to its delegate observer.
class ChromeIdentityServiceObserverBridge
    : public ios::ChromeIdentityService::Observer {
 public:
  explicit ChromeIdentityServiceObserverBridge(
      id<ChromeIdentityServiceObserver> observer);
  ~ChromeIdentityServiceObserverBridge() override;

 private:
  // ios::ChromeIdentityService::Observer implementation.
  void OnIdentityListChanged() override;
  void OnAccessTokenRefreshFailed(ChromeIdentity* identity,
                                  NSDictionary* user_info) override;
  void OnProfileUpdate(ChromeIdentity* identity) override;
  void OnChromeIdentityServiceWillBeDestroyed() override;

  __weak id<ChromeIdentityServiceObserver> observer_ = nil;
  ScopedObserver<ios::ChromeIdentityService,
                 ios::ChromeIdentityService::Observer>
      scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeIdentityServiceObserverBridge);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_OBSERVER_BRIDGE_H_

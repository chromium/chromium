// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

// Objective-C protocol mirroring ChromeBrowserProvider::Observer.
@protocol ChromeBrowserProviderObserver<NSObject>
@optional
// Called when a new ChromeIdentityService has been installed.
- (void)chromeIdentityServiceDidChange:
    (ios::ChromeIdentityService*)newIdentityService;
// Called when the ChromeBrowserProvider will be destroyed.
- (void)chromeBrowserProviderWillBeDestroyed;
@end

// Simple observer bridge that forwards all events to its delegate observer.
class ChromeBrowserProviderObserverBridge
    : public ios::ChromeBrowserProvider::Observer {
 public:
  explicit ChromeBrowserProviderObserverBridge(
      id<ChromeBrowserProviderObserver> observer);

  ChromeBrowserProviderObserverBridge(
      const ChromeBrowserProviderObserverBridge&) = delete;
  ChromeBrowserProviderObserverBridge& operator=(
      const ChromeBrowserProviderObserverBridge&) = delete;

  ~ChromeBrowserProviderObserverBridge() override;

 private:
  // ios::ChromeBrowserProvider::Observer implementation.
  void OnChromeIdentityServiceDidChange(
      ios::ChromeIdentityService* new_identity_service) override;
  void OnChromeBrowserProviderWillBeDestroyed() override;

  __weak id<ChromeBrowserProviderObserver> observer_;
  base::ScopedObservation<ios::ChromeBrowserProvider,
                          ios::ChromeBrowserProvider::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_OBSERVER_BRIDGE_H_

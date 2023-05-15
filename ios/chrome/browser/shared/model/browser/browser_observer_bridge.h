// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_OBSERVER_BRIDGE_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"

// Protocol that corresponds to the BrowserObserver API. Allows registering
// Objective-C objects to listen to Browser events.
@protocol BrowserObserving <NSObject>
- (void)browserDestroyed:(Browser*)browser;
@end

// Observer that bridges Browser events to an Objective-C observer that
// implements the BrowserObserver protocol (the observer is *not* owned).
class BrowserObserverBridge final : public BrowserObserver {
 public:
  // Creates a bridge which observes `browser`, forwarding events to `observer`.
  // This class will handle ending observation after forwarding BrowserDestroyed
  // calls to the Objective-C observer.
  BrowserObserverBridge(Browser* browser, id<BrowserObserving> observer);
  ~BrowserObserverBridge() final;

  // Not copyable or moveable.
  BrowserObserverBridge(const BrowserObserverBridge&) = delete;
  BrowserObserverBridge& operator=(const BrowserObserverBridge&) = delete;

 private:
  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;
  __weak id<BrowserObserving> observer_ = nil;
  base::ScopedObservation<Browser, BrowserObserver> browser_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_OBSERVER_BRIDGE_H_

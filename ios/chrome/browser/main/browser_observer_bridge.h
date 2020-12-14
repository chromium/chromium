// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_OBSERVER_BRIDGE_H_

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_observer.h"

// Protocol that corresponds to the BrowserObserver API. Allows registering
// Objective-C objects to listen to Browser events.
@protocol BrowserObserving <NSObject>
- (void)browserDestroyed:(Browser*)browser;
@end

// Observer that bridges Browser events to an Objective-C observer that
// implements the BrowserObserver protocol (the observer is *not* owned).
class BrowserObserverBridge final : public BrowserObserver {
 public:
  // It is the responsibility of calling code to add/remove the instance
  // from the WebStates observer lists.
  explicit BrowserObserverBridge(id<BrowserObserving> observer);
  ~BrowserObserverBridge() final;

 private:
  void BrowserDestroyed(Browser* browser) override;
  __weak id<BrowserObserving> observer_ = nil;

  DISALLOW_COPY_AND_ASSIGN(BrowserObserverBridge);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_OBSERVER_BRIDGE_H_

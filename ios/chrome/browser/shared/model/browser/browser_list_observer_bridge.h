// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/shared/model/browser/browser_list_observer.h"

class BrowserList;
class Browser;

@protocol BrowserListObserver <NSObject>

@optional
- (void)browserList:(const BrowserList*)browserList
       browserAdded:(Browser*)browser;
- (void)browserList:(const BrowserList*)browserList
     browserRemoved:(Browser*)browser;
- (void)browserListWillShutdown:(const BrowserList*)browserList;

@end

// Observer that bridges BrowserList events to an Objective-C observer that
// implements the BrowserListObserver protocol (the observer is *not* owned).
class BrowserListObserverBridge : public BrowserListObserver {
 public:
  explicit BrowserListObserverBridge(id<BrowserListObserver> observer);

  // Not copyable or moveable
  BrowserListObserverBridge(const BrowserListObserverBridge&) = delete;
  BrowserListObserverBridge& operator=(const BrowserListObserverBridge&) =
      delete;

  // BrowserListObserver
  void OnBrowserAdded(const BrowserList* browser_list,
                      Browser* browser) override;
  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override;
  void OnBrowserListShutdown(BrowserList* browser_list) override;

 private:
  __weak id<BrowserListObserver> observer_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_OBSERVER_BRIDGE_H_

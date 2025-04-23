// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/browser_view/model/browser_view_visibility_observer.h"

/// Objective-C equivalent of the BrowserViewVisibilityObserverBridge class.
@protocol BrowserViewVisibilityObserving <NSObject>

/// Browser visibility has changed from `previous_state` to `current_state`.
- (void)browserViewDidChangeToVisibilityState:
            (BrowserViewVisibilityState)currentState
                                    fromState:(BrowserViewVisibilityState)
                                                  previousState;

@end

/// Observes browser visibility change events from Objective-C. Used to update
/// listeners of change of state in url loading.
class BrowserViewVisibilityObserverBridge
    : public BrowserViewVisibilityObserver {
 public:
  BrowserViewVisibilityObserverBridge(id<BrowserViewVisibilityObserving> owner);
  ~BrowserViewVisibilityObserverBridge() override;

  void BrowserViewVisibilityStateDidChange(
      BrowserViewVisibilityState current_state,
      BrowserViewVisibilityState previous_state) override;

 private:
  __weak id<BrowserViewVisibilityObserving> owner_;
};

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_OBSERVER_BRIDGE_H_

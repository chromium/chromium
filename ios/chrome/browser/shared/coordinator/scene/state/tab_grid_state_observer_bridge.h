// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"

class TabGridStateObserver;

// Bridge to forward TabGridStateObserving updates to C++ observer.
@interface TabGridStateObserverBridge : NSObject <TabGridStateObserving>

- (instancetype)initWithObserver:(TabGridStateObserver*)observer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_OBSERVER_BRIDGE_H_

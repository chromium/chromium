// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_RECENT_TAB_REMOVAL_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_RECENT_TAB_REMOVAL_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"

namespace web {
class WebState;
}  // namespace web

// Protocol that corresponds to StartSurfaceRecentTabRemovalObserver API. Allows
// registering Objective-C objects to listen to removal of the most recent tab.
@protocol StartSurfaceRecentTabRemovalObserving <NSObject>
@optional
// Notifies the receiver that the most recent tab was removed.
- (void)mostRecentTabWasRemoved:(web::WebState*)web_state;
@end

// Bridge to use an id<StartSurfaceRecentTabRemovalObserving> as a
// StartSurfaceRecentTabRemovalObserver.
class StartSurfaceRecentTabRemovalObserverBridge
    : public StartSurfaceRecentTabRemovalObserver {
 public:
  StartSurfaceRecentTabRemovalObserverBridge(
      id<StartSurfaceRecentTabRemovalObserving> delegate);
  ~StartSurfaceRecentTabRemovalObserverBridge() override;

  // Not copyable or moveable.
  StartSurfaceRecentTabRemovalObserverBridge(
      const StartSurfaceRecentTabRemovalObserverBridge&) = delete;
  StartSurfaceRecentTabRemovalObserverBridge& operator=(
      const StartSurfaceRecentTabRemovalObserverBridge&) = delete;

 private:
  // StartSurfaceBrowserAgentObserver.
  void MostRecentTabRemoved(web::WebState* web_state) override;

  __weak id<StartSurfaceRecentTabRemovalObserving> delegate_ = nil;
};

#endif  // IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_RECENT_TAB_REMOVAL_OBSERVER_BRIDGE_H_

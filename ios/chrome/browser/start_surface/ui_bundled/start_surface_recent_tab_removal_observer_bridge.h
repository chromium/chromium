// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_REMOVAL_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_REMOVAL_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"

namespace web {
class WebState;
}  // namespace web

// Protocol that corresponds to StartSurfaceRecentTabObserver API. Allows
// registering Objective-C objects to listen to updates to the most recent tab.
@protocol StartSurfaceRecentTabObserving <NSObject>
// Notifies the receiver that the most recent tab was removed.
- (void)mostRecentTabWasRemoved:(web::WebState*)webState;
// Notifies the receiver that the favicon for the current page of the most
// recent tab was updated with `image`.
- (void)mostRecentTab:(web::WebState*)webState
    faviconUpdatedWithImage:(UIImage*)image;
// Notifies the receiver that the title of the current page of the most recent
// tab was updated to `title`.
- (void)mostRecentTab:(web::WebState*)webState titleWasUpdated:(NSString*)title;
@end

// Bridge to use an id<StartSurfaceRecentTabObserving> as a
// StartSurfaceRecentTabObserver.
class StartSurfaceRecentTabObserverBridge
    : public StartSurfaceRecentTabObserver {
 public:
  StartSurfaceRecentTabObserverBridge(
      id<StartSurfaceRecentTabObserving> delegate);
  ~StartSurfaceRecentTabObserverBridge() override;

  // Not copyable or moveable.
  StartSurfaceRecentTabObserverBridge(
      const StartSurfaceRecentTabObserverBridge&) = delete;
  StartSurfaceRecentTabObserverBridge& operator=(
      const StartSurfaceRecentTabObserverBridge&) = delete;

 private:
  // StartSurfaceBrowserAgentObserver.
  void MostRecentTabRemoved(web::WebState* web_state) override;
  void MostRecentTabFaviconUpdated(web::WebState* web_state,
                                   UIImage* image) override;
  void MostRecentTabTitleUpdated(web::WebState* web_state,
                                 const std::u16string& title) override;

  __weak id<StartSurfaceRecentTabObserving> delegate_ = nil;
};

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_REMOVAL_OBSERVER_BRIDGE_H_

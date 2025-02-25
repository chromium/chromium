// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBAR_UI_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBAR_UI_OBSERVER_BRIDGE_H_

#include <CoreFoundation/CoreFoundation.h>

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbar_ui.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbar_ui_observer.h"

// Allows registering of `PanelContentMediator` which is an Objective-C objects
// to listen to ToolbarUI events.
@protocol ToolbarUIObserving <NSObject>
- (void)OnTopToolbarHeightChanged;
- (void)OnBottomToolbarHeightChanged;
@end

class ToolbarUIObserverBridge : public ToolbarUIObserver {
 public:
  ToolbarUIObserverBridge(id<ToolbarUIObserving> observer);
  ~ToolbarUIObserverBridge() override;

  ToolbarUIObserverBridge(const ToolbarUIObserverBridge&) = delete;
  ToolbarUIObserverBridge& operator=(const ToolbarUIObserverBridge&) = delete;

 private:
  // ToolbarUIObserver
  void OnTopToolbarHeightChanged() override;
  void OnBottomToolbarHeightChanged() override;

  __weak id<ToolbarUIObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBAR_UI_OBSERVER_BRIDGE_H_

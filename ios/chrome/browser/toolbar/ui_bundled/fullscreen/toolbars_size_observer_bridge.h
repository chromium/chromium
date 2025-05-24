// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_OBSERVER_BRIDGE_H_

#include <CoreFoundation/CoreFoundation.h>

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_observer.h"

// Allows registering of `PanelContentMediator` which is an Objective-C objects
// to listen to ToolbarUI events.
@protocol ToolbarsSizeObserving <NSObject>
- (void)OnTopToolbarHeightChanged;
- (void)OnBottomToolbarHeightChanged;
@end

class ToolbarsSizeObserverBridge : public ToolbarsSizeObserver {
 public:
  ToolbarsSizeObserverBridge(id<ToolbarsSizeObserving> observer);
  ~ToolbarsSizeObserverBridge() override;

  ToolbarsSizeObserverBridge(const ToolbarsSizeObserverBridge&) = delete;
  ToolbarsSizeObserverBridge& operator=(const ToolbarsSizeObserverBridge&) =
      delete;

 private:
  // ToolbarUIObserver
  void OnTopToolbarHeightChanged() override;
  void OnBottomToolbarHeightChanged() override;

  __weak id<ToolbarsSizeObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_OBSERVER_BRIDGE_H_

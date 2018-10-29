// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"

namespace toolbar_container {
class HeightRange;
}  // namespace toolbar_container

// The layout orientation for a toolbar container.
enum class ToolbarContainerOrientation { kTopToBottom, kBottomToTop };

// The view controller that manages a stack of toolbars.
@interface ToolbarContainerViewController
    : UIViewController<FullscreenUIElement>

// The orientation of the container.
@property(nonatomic, assign) ToolbarContainerOrientation orientation;

// Whether the container should collapse the toolbars past the edge of the safe
// area.
@property(nonatomic, assign) BOOL collapsesSafeArea;

// The toolbar view controllers being managed by this container.
@property(nonatomic, strong) NSArray<UIViewController*>* toolbars;

// The height range of the overall stack.  It is calculated using the collapsed
// and expanded heights of the views managed by |toolbars|.
@property(nonatomic, readonly)
    const toolbar_container::HeightRange& heightRange;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_CONTAINER_VIEW_CONTROLLER_H_

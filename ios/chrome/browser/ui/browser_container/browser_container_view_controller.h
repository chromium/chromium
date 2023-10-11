// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/browser_container/browser_container_consumer.h"

@class BrowserEditMenuHandler;

@protocol LinkToTextDelegate;

// UIViewController which allows displaying and removing a content view.
@interface BrowserContainerViewController
    : UIViewController <BrowserContainerConsumer>

// The UIViewController used to display overlay UI over the web content area.
@property(nonatomic, strong)
    UIViewController* webContentsOverlayContainerViewController;

// The UIViewController used to display the ScreenTime blocker above the web
// content area.
@property(nonatomic, strong) UIViewController* screenTimeViewController;

// The delegate to handle link to text button selection.
@property(nonatomic, weak) id<LinkToTextDelegate> linkToTextDelegate;

// The handler for the edit menu.
@property(nonatomic, weak) BrowserEditMenuHandler* browserEditMenuHandler;

// Adds the given `contentView` as a subview and removes the previously added
// `contentView` or `contentViewController`, if any. If `contentView` is nil
// then only old content view or view controller is removed.
- (void)setContentView:(UIView*)contentView;

// Adds the given `contentViewController` as a child view controller and removes
// the previously added `contentViewController` if any.  Setting
// `contentViewController` does not clear `contentView`.
- (void)setContentViewController:(UIViewController*)contentViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_VIEW_CONTROLLER_H_

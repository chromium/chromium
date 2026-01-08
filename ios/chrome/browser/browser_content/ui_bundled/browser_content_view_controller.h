// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_BROWSER_CONTENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_BROWSER_CONTENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/browser_content/ui_bundled/browser_content_consumer.h"

@protocol BrowserContentViewControllerDelegate;

// UIViewController which allows displaying and removing a content view.
@interface BrowserContentViewController
    : UIViewController <BrowserContentConsumer>

// The UIViewController used to display overlay UI over the web content area.
@property(nonatomic, strong)
    UIViewController* webContentsOverlayContainerViewController;

// The delegate that hendles the edit menu.
@property(nonatomic, weak) id<BrowserContentViewControllerDelegate> delegate;

// Adds the given `contentView` as a subview and removes the previously added
// `contentView` or `contentViewController`, if any. If `contentView` is nil
// then only old content view or view controller is removed.
- (void)setContentView:(UIView*)contentView;

// Adds the given `contentViewController` as a child view controller and removes
// the previously added `contentViewController` if any.  Setting
// `contentViewController` does not clear `contentView`.
- (void)setContentViewController:(UIViewController*)contentViewController;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_BROWSER_CONTENT_VIEW_CONTROLLER_H_

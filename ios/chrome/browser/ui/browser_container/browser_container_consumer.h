// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol BrowserContainerConsumer <NSObject>

// Adds the given |contentView| as a subview and removes the previously added
// |contentView| or |contentViewController|, if any. If |contentView| is nil
// then only old content view or view controller is removed.
- (void)setContentView:(UIView*)contentView;

// Adds the given |contentViewController| as a child view controller and removes
// the previously added |contentViewController| if any.  Setting
// |contentViewController| does not clear |contentView|.
- (void)setContentViewController:(UIViewController*)contentViewController;

// The UIViewController used to display overlay UI over the web content area.
- (void)setWebContentsOverlayContainerViewController:
    (UIViewController*)webContentsOverlayContainerViewController;

// The UIViewController used to display the ScreenTime UI above the web content
// area.
- (void)setScreenTimeViewController:(UIViewController*)screenTimeViewController;

// Whether the content view should be blocked.  When set to YES, the content
// area is blocked.  Overlay UI shown in OverlayModality::kWebContentArea remain
// visible when |contentBlocked| is YES.
- (void)setContentBlocked:(BOOL)contentBlocked;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_CONSUMER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_CONTAINER_UI_BUNDLED_BROWSER_CONTAINER_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_BROWSER_CONTAINER_UI_BUNDLED_BROWSER_CONTAINER_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// Delegate protocol for BrowserContainerViewController
@protocol BrowserContainerViewControllerDelegate <NSObject>

// Called when an edit menu is triggered in `controller`.
- (void)browserContainerViewController:
            (BrowserContainerViewController*)controller
         didTriggerEditMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_CONTAINER_UI_BUNDLED_BROWSER_CONTAINER_VIEW_CONTROLLER_DELEGATE_H_

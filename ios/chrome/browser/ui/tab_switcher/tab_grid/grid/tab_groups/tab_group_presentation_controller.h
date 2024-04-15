// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class TabGroupViewController;

// The presentation controller for the TabGroup. Handle the presentation style
// and the animation of the inner elements during appearance/disappearance.
@interface TabGroupPresentationController : UIPresentationController

// Whether this coordinator should be presented with smaller motions. Default is
// NO.
@property(nonatomic, assign) BOOL smallerMotions;

// Init with the `tabGroupViewController` to be able to animate the element.
- (instancetype)initWithPresentedTabGroupViewController:
                    (TabGroupViewController*)tabGroupViewController
                               presentingViewController:
                                   (UIViewController*)presentingViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_PRESENTATION_CONTROLLER_H_

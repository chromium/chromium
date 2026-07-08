// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_SHARED_TABS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_SHARED_TABS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "url/gurl.h"

@class ComposeboxMenuSharedTab;
@class ComposeboxMenuSharedTabsViewController;

// Delegate for ComposeboxMenuSharedTabsViewController actions.
@protocol ComposeboxMenuSharedTabsViewControllerDelegate <NSObject>

// Called when the user taps on a link in the disclaimer.
- (void)composeboxMenuSharedTabsViewController:
            (ComposeboxMenuSharedTabsViewController*)viewController
                                     didTapURL:(const GURL&)url;

@end

// View controller displaying the list of shared tabs in a sheet.
@interface ComposeboxMenuSharedTabsViewController : UIViewController

// Delegate for this view controller.
@property(nonatomic, weak) id<ComposeboxMenuSharedTabsViewControllerDelegate>
    delegate;

// Initializes the view controller with the given list of shared tabs.
- (instancetype)initWithSharedTabs:
    (NSArray<ComposeboxMenuSharedTab*>*)sharedTabs NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_SHARED_TABS_VIEW_CONTROLLER_H_

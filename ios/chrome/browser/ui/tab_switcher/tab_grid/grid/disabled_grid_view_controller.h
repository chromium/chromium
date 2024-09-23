// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_DISABLED_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_DISABLED_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

class GURL;

// Delegate for this view controller to handle user click actions.
@protocol DisabledGridViewControllerDelegate

// Notifies the delegate that the user tapped a link with a `URL`.
- (void)didTapLinkWithURL:(const GURL&)URL;

// Returns true if the disabled tab view is subject to parental controls.
- (bool)isViewControllerSubjectToParentalControls;

@end

// View controller representing a view without tab grids when any of the
// incognito tab grid, regular tab grid, Recent Tabs, or Tab Groups is disabled.
@interface DisabledGridViewController : UIViewController

@property(nonatomic, weak) id<DisabledGridViewControllerDelegate> delegate;

// Init with page type, which decides the displayed text.
- (instancetype)initWithPage:(TabGridPage)page NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_DISABLED_GRID_VIEW_CONTROLLER_H_

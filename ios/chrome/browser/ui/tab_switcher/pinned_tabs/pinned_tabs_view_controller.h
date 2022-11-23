// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol GridImageDataSource;

// UICollectionViewController used to display pinned tabs.
@interface PinnedTabsViewController : UICollectionViewController

// Data source for images.
@property(nonatomic, weak) id<GridImageDataSource> imageDataSource;

// Updates the view when starting or ending a drag action.
- (void)dragSessionEnabled:(BOOL)enabled;

// Makes the pinned tabs view available. The pinned view should only be
// available when the regular tabs grid is displayed.
- (void)pinnedTabsAvailable:(BOOL)available;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithCollectionViewLayout:(UICollectionViewLayout*)layout
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_

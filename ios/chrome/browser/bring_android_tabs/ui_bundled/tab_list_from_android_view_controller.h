// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_consumer.h"

@protocol TableViewFaviconDataSource;
@protocol TabListFromAndroidViewControllerDelegate;

// Table view controller for the "Bring Android Tabs" tab list.
@interface TabListFromAndroidViewController
    : LegacyChromeTableViewController <TabListFromAndroidConsumer,
                                       UIAdaptivePresentationControllerDelegate>

// Delegate protocol that handles model updates on interaction.
@property(nonatomic, weak) id<TabListFromAndroidViewControllerDelegate>
    delegate;

// Data source for favicon images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> faviconDataSource;

// Designated initializer for this view controller.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Unavailable initializer.
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_VIEW_CONTROLLER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_accessibility_delegate.h"

@protocol ReadingListDataSource;
@protocol ReadingListListViewControllerAudience;
@protocol ReadingListListViewControllerDelegate;

// View controller that displays reading list items in a table view.
@interface ReadingListTableViewController
    : ChromeTableViewController <ReadingListListItemAccessibilityDelegate,
                                 UIAdaptivePresentationControllerDelegate>

// The delegate.
@property(nonatomic, weak) id<ReadingListListViewControllerDelegate> delegate;
// The audience that is interested in whether the table has any items.
@property(nonatomic, weak) id<ReadingListListViewControllerAudience> audience;
// The table's data source.
@property(nonatomic, weak) id<ReadingListDataSource> dataSource;

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// Prepares this view controller to be dismissed.
- (void)willBeDismissed;

// Reloads all the data.
- (void)reloadData;

@end
#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TABLE_VIEW_CONTROLLER_H_

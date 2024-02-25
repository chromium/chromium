// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRAG_AND_DROP_MODEL_TABLE_VIEW_URL_DRAG_DROP_HANDLER_H_
#define IOS_CHROME_BROWSER_DRAG_AND_DROP_MODEL_TABLE_VIEW_URL_DRAG_DROP_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"

class GURL;
@class URLInfo;

// The interface for providing draggable URLs from a table view.
@protocol TableViewURLDragDataSource
// Returns a wrapper object with URL and title to drag for the item at
// `indexPath` in `tableView`. Returns nil if item at `indexPath` is not
// draggable.
- (URLInfo*)tableView:(UITableView*)tableView
    URLInfoAtIndexPath:(NSIndexPath*)indexPath;
@end

// The interface for handling URL drops in a table view.
@protocol TableViewURLDropDelegate
// Returns whether `tableView` is in a state to handle URL drops.
- (BOOL)canHandleURLDropInTableView:(UITableView*)tableView;
// Provides the receiver with the dropped `URL`, which was dropped at
// `indexPath` in `tableView`.
- (void)tableView:(UITableView*)tableView
       didDropURL:(const GURL&)URL
      atIndexPath:(NSIndexPath*)indexPath;
@end

// A delegate object that is configured to handle URL drag and drops from a
// table view.
@interface TableViewURLDragDropHandler
    : NSObject <UITableViewDragDelegate, UITableViewDropDelegate>
// Origin used to configure drag items.
@property(nonatomic, assign) WindowActivityOrigin origin;
// The data source object that provides draggable URLs from a table view.
@property(nonatomic, weak) id<TableViewURLDragDataSource> dragDataSource;
// The delegate object that manages URL drops into a table view.
@property(nonatomic, weak) id<TableViewURLDropDelegate> dropDelegate;
@end

#endif  // IOS_CHROME_BROWSER_DRAG_AND_DROP_MODEL_TABLE_VIEW_URL_DRAG_DROP_HANDLER_H_

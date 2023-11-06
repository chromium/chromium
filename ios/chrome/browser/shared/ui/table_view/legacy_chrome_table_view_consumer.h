// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_CONSUMER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_CONSUMER_H_

#import <Foundation/Foundation.h>

// LegacyChromeTableViewConsumer declares a basic set of methods that allow
// table view mediators to update their UI. Individual features can extend this
// protocol to add feature-specific methods.
@protocol LegacyChromeTableViewConsumer <NSObject>

// Reconfigures the cells corresponding to the given `items` by calling
// `configureCell:` on each cell.
- (void)reconfigureCellsForItems:(NSArray*)items;

// Reloads the cells corresponding to the given `items` by calling
// reloadRowsAtIndexPaths with `rowAnimation` on the tableView for each of the
// `items` indexPath, this will also trigger a `configureCell:` call on each
// cell.
// Use this method over `reconfigureCellsForItems` if the cell should be redrawn
// after calling `configureCell:`.
- (void)reloadCellsForItems:(NSArray*)items
           withRowAnimation:(UITableViewRowAnimation)rowAnimation;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_CONSUMER_H_

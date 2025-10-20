// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_ITEM_TABLE_VIEW_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_ITEM_TABLE_VIEW_H_

#import "ios/chrome/browser/safari_data_import/public/safari_data_item_consumer.h"
#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"

@protocol SafariDataImportImportStageTransitionHandler;

/// View controller for the Safari data import screen.
@interface SafariDataItemTableView
    : SelfSizingTableView <SafariDataItemConsumer>

/// Import stage transition handler.
@property(nonatomic, weak) id<SafariDataImportImportStageTransitionHandler>
    importStageTransitionHandler;

/// Designated Initializer. `itemCount` specifies how many cells should be in
/// the table view.
- (instancetype)initWithItemCount:(NSInteger)itemCount
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame
                        style:UITableViewStyle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

/// Resets the table to the stage when no items are processed.
- (void)reset;

/// Notifies the table view that user has initiated importing items.
- (void)notifyImportStart;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_ITEM_TABLE_VIEW_H_

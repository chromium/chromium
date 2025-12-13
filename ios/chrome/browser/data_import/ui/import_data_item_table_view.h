// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_IMPORT_DATA_ITEM_TABLE_VIEW_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_IMPORT_DATA_ITEM_TABLE_VIEW_H_

#import "ios/chrome/browser/data_import/public/import_data_item_consumer.h"
#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"

@protocol DataImportImportStageTransitionHandler;

/// View controller for the import data screen.
@interface ImportDataItemTableView
    : SelfSizingTableView <ImportDataItemConsumer>

/// Import stage transition handler.
@property(nonatomic, weak) id<DataImportImportStageTransitionHandler>
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

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_IMPORT_DATA_ITEM_TABLE_VIEW_H_

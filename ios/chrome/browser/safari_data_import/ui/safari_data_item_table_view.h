// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_ITEM_TABLE_VIEW_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_ITEM_TABLE_VIEW_H_

#import "ios/chrome/browser/safari_data_import/ui/safari_data_item_consumer.h"
#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"

@protocol SafariDataImportImportStageConsumer;

/// View controller for the Safari data import screen.
@interface SafariDataItemTableView
    : SelfSizingTableView <SafariDataItemConsumer>

/// Consumer object handling import stage transitions.
@property(nonatomic, weak) id<SafariDataImportImportStageConsumer>
    importStageConsumer;

/// Designated Initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame
                        style:UITableViewStyle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_ITEM_TABLE_VIEW_H_

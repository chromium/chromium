/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_IMPORT_DATA_ITEM_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_IMPORT_DATA_ITEM_H_

#import <Foundation/Foundation.h>

/// Different types of items identified by a ImportDataItem. Ordered by position
/// in the table view.
enum class ImportDataItemType : NSUInteger {
  kPasswords = 0,
  kPayment = 1,
  kHistory = 2,
  kBookmarks = 3,
  kPasskeys = 4,
};

/// Current import progress for each ImportDataItem.
enum class ImportDataItemImportStatus : NSUInteger {
  /// Data is ready to be imported.
  kReady,
  /// Data import in progress.
  kImporting,
  /// Data import completed.
  kImported,
  /// The data item cannot be imported because of an enterprise policy.
  kBlockedByPolicy,
};

/// Hashable container for a type of data item that can be imported.
@interface ImportDataItem : NSObject

/// The type of the import data item. Also serves as a unique hash of the
/// object.
@property(nonatomic, readonly) ImportDataItemType type;

/// One of { kReady, kImporting, kImported }.
@property(nonatomic, readonly) ImportDataItemImportStatus status;

/// Number of items.
@property(nonatomic, readonly) int count;

/// Number of invalid items; Applicable for passwords only; value will be 0 for
/// other types.
@property(nonatomic, assign) int invalidCount;

/// Initializer with data item type.
- (instancetype)initWithType:(ImportDataItemType)type
                      status:(ImportDataItemImportStatus)status
                       count:(size_t)count NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Transition the item to the next import status. Should not be called if the
/// status is `kImported`.
- (void)transitionToNextStatus;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_IMPORT_DATA_ITEM_H_

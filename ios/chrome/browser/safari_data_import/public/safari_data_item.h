/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_ITEM_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_ITEM_H_

#import <Foundation/Foundation.h>

/// Different types of items identified by a SafariDataItem. Ordered by position
/// in the table view.
enum class SafariDataItemType : NSUInteger {
  kPasswords = 0,
  kPayment = 1,
  kHistory = 2,
  kBookmarks = 3,
};

/// Current import progress for each SafariDataItem.
enum class SafariDataItemImportStatus : NSUInteger {
  /// Data is ready to be imported.
  kReady,
  /// Data import in progress.
  kImporting,
  /// Data import completed.
  kImported,
};

/// Hashable container for a type of data item that can be imported from Safari.
@interface SafariDataItem : NSObject

/// The type of the Safari data item. Also serves as a unique hash of the
/// object.
@property(nonatomic, readonly) SafariDataItemType type;

/// One of { kReady, kImporting, kImported }.
@property(nonatomic, readonly) SafariDataItemImportStatus status;

/// Number of items.
@property(nonatomic, readonly) int count;

/// Number of invalid items; Applicable for passwords only; value will be 0 for
/// other types.
@property(nonatomic, assign) int invalidCount;

/// Initializer with data item type.
- (instancetype)initWithType:(SafariDataItemType)type
                      status:(SafariDataItemImportStatus)status
                       count:(size_t)count NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Transition the item to the next import status. Should not be called if the
/// status is `kImported`.
- (void)transitionToNextStatus;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_ITEM_H_

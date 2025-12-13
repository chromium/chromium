// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_FAVICON_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_FAVICON_DATA_SOURCE_H_

#import "base/ios/block_types.h"

@class PasswordImportItem;

/// A data source for a password import item's favicon.
@protocol PasswordImportItemFaviconDataSource

/// Loads `item.faviconAttributes` with `UIHandler` invoked asynchronously on
/// the main thread.
- (BOOL)passwordImportItem:(PasswordImportItem*)item
    loadFaviconAttributesWithUIHandler:(ProceduralBlock)UIHandler;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_FAVICON_DATA_SOURCE_H_

/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_IMPORT_DATA_ITEM_CONSUMER_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_IMPORT_DATA_ITEM_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ImportDataItem;

/// Consumer to allow the import model to send import status information to
/// its UI.
@protocol ImportDataItemConsumer

/// Populate import data items. Note that this method should only be invoked
/// from the same thread.
- (void)populateItem:(ImportDataItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_IMPORT_DATA_ITEM_CONSUMER_H_

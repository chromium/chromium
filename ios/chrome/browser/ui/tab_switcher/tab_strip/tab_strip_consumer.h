// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CONSUMER_H_

#import <Foundation/Foundation.h>

@class GridItem;

// TabStripConsumer sets the current appearance of the TabStrip.
@protocol TabStripConsumer

// Tells the consumer to replace its current set of items with |items| and
// update the selected item ID to be |selectedItemID|. It's an error to pass
// an |items| array containing items without unique IDs.
- (void)populateItems:(NSArray<GridItem*>*)items
       selectedItemID:(NSString*)selectedItemID;

// Tells the consumer to replace the item with ID |itemID| with |item|.
// It's an error if |item|'s ID duplicates any other item's ID besides |itemID|.
// The consumer should ignore this call if |itemID| has not yet been inserted.
- (void)replaceItemID:(NSString*)itemID withItem:(GridItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CONSUMER_H_

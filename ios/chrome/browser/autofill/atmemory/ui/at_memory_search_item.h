// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_ITEM_H_

#import <Foundation/Foundation.h>

// Custom item representing the search/loading cell.
// TODO(crbug.com/532090671): Delete this class once backend APIs are hooked up.
@interface AtMemorySearchItem : NSObject

@property(nonatomic, copy) NSString* text;
@property(nonatomic, copy) NSString* detailText;
@property(nonatomic, copy) NSString* itemType;
@property(nonatomic, assign) BOOL loading;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_ITEM_H_

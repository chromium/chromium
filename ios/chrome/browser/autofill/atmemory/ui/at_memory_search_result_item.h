// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_RESULT_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_RESULT_ITEM_H_

#import <Foundation/Foundation.h>

// Custom item representing a search result item.
// TODO(crbug.com/532090671): Delete this class once backend APIs are hooked up.
@interface AtMemorySearchResultItem : NSObject

@property(nonatomic, copy) NSString* fillingText;
@property(nonatomic, copy) NSString* subtitle;
@property(nonatomic, copy) NSString* iconSymbolName;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_RESULT_ITEM_H_

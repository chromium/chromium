// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_CONSUMER_H_

#import <Foundation/Foundation.h>

@class AtMemoryGranularFillItem;

// Consumer for AtMemory.
@protocol AtMemoryConsumer <NSObject>

// Sets the granular fill items.
- (void)setGranularFillItems:(NSArray<AtMemoryGranularFillItem*>*)items;

// Sets the current search query.
- (void)setSearchQuery:(NSString*)query;

// Sets whether the search is loading.
- (void)setSearchLoading:(BOOL)loading;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_CONSUMER_H_

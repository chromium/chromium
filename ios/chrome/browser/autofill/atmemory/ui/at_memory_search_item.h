// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_ITEM_H_

#import <UIKit/UIKit.h>

namespace autofill {
struct Suggestion;
}  // namespace autofill

// Represents an item in the AtMemory table view diffable data source.
// The equality between two objects is based on `title` and `subtitle`.
@interface AtMemorySearchItem : NSObject

// Title of the item.
@property(nonatomic, copy, readonly) NSString* title;

// Subtitle or description of the item.
@property(nonatomic, copy, readonly) NSString* subtitle;

// Icon representing the entity type of the item.
@property(nonatomic, strong, readonly) UIImage* icon;

// Index of the search result item.
@property(nonatomic, assign, readonly) NSInteger index;

// Initializes the item from an autofill `suggestion` and its `index`.
- (instancetype)initWithSuggestion:(const autofill::Suggestion&)suggestion
                             index:(NSInteger)index NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_ITEM_H_

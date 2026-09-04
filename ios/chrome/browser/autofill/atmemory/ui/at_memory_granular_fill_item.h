// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_ITEM_H_

#import <Foundation/Foundation.h>

// Item representing an attribute field with label and value for AtMemory
// granular fill.
@interface AtMemoryGranularFillItem : NSObject

// The name of the attribute.
@property(nonatomic, copy, readonly) NSString* attributeName;

// The value of the attribute.
@property(nonatomic, copy, readonly) NSString* attributeValue;

// The index of the child suggestion in the parent suggestion's children.
@property(nonatomic, assign, readonly) NSInteger index;

// Initializes an AtMemoryGranularFillItem with `attributeName`,
// `attributeValue`, and `index`.
- (instancetype)initWithAttributeName:(NSString*)attributeName
                       attributeValue:(NSString*)attributeValue
                                index:(NSInteger)index
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_ITEM_H_

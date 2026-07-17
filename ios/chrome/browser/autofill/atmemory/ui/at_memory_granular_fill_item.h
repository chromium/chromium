// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Item representing an attribute field with label and chip values for AtMemory
// granular fill.
@interface AtMemoryGranularFillItem : TableViewItem

// The name of the attribute.
@property(nonatomic, copy) NSString* attributeName;

// The value(s) of the attribute.
@property(nonatomic, copy) NSArray<NSString*>* attributeValue;

// The target for chip tap actions.
@property(nonatomic, weak) id target;

// The action to execute when a chip is tapped.
@property(nonatomic, assign) SEL action;

@end

// Cell that renders an AtMemory granular fill item with a field label and chip
// buttons.
@interface AtMemoryGranularFillCell : LegacyTableViewCell

// Configures the cell with attribute name and chip values.
- (void)setAttributeName:(NSString*)attributeName
          attributeValue:(NSArray<NSString*>*)chipTexts
                  target:(id)target
                  action:(SEL)action;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_ITEM_H_

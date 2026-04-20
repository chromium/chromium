// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_DATE_ITEM_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_DATE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_field_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"

@class AutofillAIEntityEditDateItem;

@protocol AutofillAIEntityEditDateItemDelegate <TableViewTextEditItemDelegate>

// Notifies the delegate that the date picker value changed.
- (void)didChangeDate:(NSDate*)date forItem:(AutofillAIEntityEditDateItem*)item;

@end

// Table view item for a date field.
@interface AutofillAIEntityEditDateItem
    : TableViewTextEditItem <AutofillAIEntityFieldItem>

@property(nonatomic, strong) NSDate* dateValue;

// Whether the item is in editing mode.
@property(nonatomic, assign) BOOL editingEnabled;

// Delegate to handle date changes.
@property(nonatomic, weak) id<AutofillAIEntityEditDateItemDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_DATE_ITEM_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_AUTOFILL_EDIT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_AUTOFILL_EDIT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"

// Item to represent and configure an AutofillEditItem.
@interface AutofillEditItem : TableViewTextEditItem

// The field type this item is describing.
@property(nonatomic, assign) AutofillUIType autofillUIType;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_AUTOFILL_EDIT_ITEM_H_

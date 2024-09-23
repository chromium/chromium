// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_AUTOFILL_PROFILE_EDIT_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_AUTOFILL_PROFILE_EDIT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"

// Item to represent and configure an AutofillEditItem.
@interface AutofillProfileEditItem : TableViewTextEditItem

// The field type this item is describing.
@property(nonatomic, copy) NSString* autofillFieldType;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_AUTOFILL_PROFILE_EDIT_ITEM_H_

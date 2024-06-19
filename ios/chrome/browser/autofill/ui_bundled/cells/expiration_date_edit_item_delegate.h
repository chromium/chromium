// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_EXPIRATION_DATE_EDIT_ITEM_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_EXPIRATION_DATE_EDIT_ITEM_DELEGATE_H_

#import "ios/chrome/browser/autofill/ui_bundled/cells/expiration_date_edit_item.h"

// Delegate of an ExpirationDateEditItem.
// Gets notified when an expiration date was picked.
@protocol ExpirationDateEditItemDelegate <NSObject>

// Invoked when an expiration date is picked in the cell corresponding to
// `item`.
- (void)expirationDateEditItemDidChange:(ExpirationDateEditItem*)item;

@end
#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_EXPIRATION_DATE_EDIT_ITEM_DELEGATE_H_

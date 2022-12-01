// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_EXPIRATION_DATE_EDIT_ITEM_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_EXPIRATION_DATE_EDIT_ITEM_PRIVATE_H_

#import "ios/chrome/browser/ui/autofill/cells/expiration_date_edit_item.h"

@interface ExpirationDateEditItem (Private)

// Making both properties writable for testing.
@property(nonatomic, readwrite, copy) NSString* month;
@property(nonatomic, readwrite, copy) NSString* year;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_EXPIRATION_DATE_EDIT_ITEM_PRIVATE_H_

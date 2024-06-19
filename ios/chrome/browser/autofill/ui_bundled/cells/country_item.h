// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_COUNTRY_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_COUNTRY_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"

// Item for autofill country field.
@interface CountryItem : TableViewTextItem

// The Country code.
@property(nonatomic, assign) NSString* countryCode;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_COUNTRY_ITEM_H_

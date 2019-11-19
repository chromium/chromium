// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_DATA_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_DATA_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"

// Item for autofill profile (address) or credit card.
@interface AutofillDataItem : TableViewMultiDetailTextItem

// Only deletable items will enter edit mode.
@property(nonatomic, assign, getter=isDeletable) BOOL deletable;

// The GUID used by the PersonalDataManager to identify data elements (e.g.
// profiles and credit cards).
@property(nonatomic, assign) std::string GUID;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_DATA_ITEM_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_CARD_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_CARD_ITEM_H_

#include <string>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"

// Item for autofill credit card.
@interface AutofillCardItem : TableViewMultiDetailTextItem

// Only deletable items will enter edit mode.
@property(nonatomic, assign, getter=isDeletable) BOOL deletable;

// The GUID used by the PersonalDataManager to identify cards.
@property(nonatomic, assign) std::string GUID;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_CARD_ITEM_H_

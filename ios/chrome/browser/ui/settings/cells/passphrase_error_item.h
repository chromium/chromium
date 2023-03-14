// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_PASSPHRASE_ERROR_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_PASSPHRASE_ERROR_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Item to display an error when the passphrase is incorrect.
@interface PassphraseErrorItem : TableViewItem

// The error text. It appears in red.
@property(nonatomic, copy) NSString* text;

@end

// Cell class associated to PassphraseErrorItem.
@interface PassphraseErrorCell : TableViewCell

// Label for the error text.
@property(nonatomic, readonly, strong) UILabel* textLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_PASSPHRASE_ERROR_ITEM_H_

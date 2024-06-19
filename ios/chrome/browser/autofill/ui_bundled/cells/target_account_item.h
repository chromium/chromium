// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_TARGET_ACCOUNT_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_TARGET_ACCOUNT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// An item showing the avatar and email of the account where the card will be
// saved.
@interface TargetAccountItem : TableViewItem

// Avatar displayed by the item's cell.
@property(nonatomic, strong) UIImage* avatar;
// Email displayed by the item's cell.
@property(nonatomic, strong) NSString* email;

@end

// Cell class for TargetAccountItem.
@interface TargetAccountCell : TableViewCell

// A left-aligned round badge showing the account avatar.
@property(nonatomic, readonly, strong) UIImageView* avatarBadge;
// A label for the account email, shown to the right of `avatarBadge` and to
// the left of the Google pay icon.
@property(nonatomic, readonly, strong) UILabel* emailLabel;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_TARGET_ACCOUNT_ITEM_H_

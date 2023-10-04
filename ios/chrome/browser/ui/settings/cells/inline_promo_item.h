// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_INLINE_PROMO_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_INLINE_PROMO_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// InlinePromoItem is a model class that uses InlinePromoCell.
@interface InlinePromoItem : TableViewItem

// Image displayed with a new feature badge.
@property(nonatomic, strong) UIImage* promoImage;

// Descriptive text of the promo.
@property(nonatomic, copy) NSString* promoText;

// Title of the more info button.
@property(nonatomic, strong) NSString* moreInfoButtonTitle;

// Whether or not the close button should be visible. `YES` by default.
@property(nonatomic, assign) BOOL shouldShowCloseButton;

// Whether or not the cell should be enabled. `YES` by default.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// Whether or not the cell should be configured with its wide layout.
@property(nonatomic, assign) BOOL shouldHaveWideLayout;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_INLINE_PROMO_ITEM_H_

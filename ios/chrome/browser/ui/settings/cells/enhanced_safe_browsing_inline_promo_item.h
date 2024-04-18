// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_ENHANCED_SAFE_BROWSING_INLINE_PROMO_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_ENHANCED_SAFE_BROWSING_INLINE_PROMO_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@protocol EnhancedSafeBrowsingInlinePromoDelegate;
@protocol SettingsCommands;

// A table view item used to represent the Enhanced Safe Browsing Inline Promo.
@interface EnhancedSafeBrowsingInlinePromoItem : TableViewItem

// Delegate object used to send events from the inline promo to the Settings
// page.
@property(nonatomic, weak) id<EnhancedSafeBrowsingInlinePromoDelegate> delegate;

@end

// EnhancedSafeBrowsingInlinePromoCell implements an TableViewCell subclass that
// contains all the inline promo's UI elements.
@interface EnhancedSafeBrowsingInlinePromoCell : TableViewCell

// Delegate object used to send events from the inline promo to the Settings
// page.
@property(nonatomic, weak) id<EnhancedSafeBrowsingInlinePromoDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_ENHANCED_SAFE_BROWSING_INLINE_PROMO_ITEM_H_

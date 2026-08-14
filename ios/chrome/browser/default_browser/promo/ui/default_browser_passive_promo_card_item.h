// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_UI_DEFAULT_BROWSER_PASSIVE_PROMO_CARD_ITEM_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_UI_DEFAULT_BROWSER_PASSIVE_PROMO_CARD_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// DefaultBrowserPassivePromoCardItem is a TableViewItem for the passive
// default browser promo card.
@interface DefaultBrowserPassivePromoCardItem : TableViewItem

// The target for actions on the cell's buttons.
@property(nonatomic, weak) id target;
// The action for the close button.
@property(nonatomic, assign) SEL closeAction;
// The action for the primary action button.
@property(nonatomic, assign) SEL primaryAction;

@end

// DefaultBrowserPassivePromoCardCell is the UITableViewCell for the
// passive default browser promo card.
@interface DefaultBrowserPassivePromoCardCell : LegacyTableViewCell

// The target for actions on the cell's buttons.
@property(nonatomic, weak) id target;
// The action for the close button.
@property(nonatomic, assign) SEL closeAction;
// The action for the primary action button.
@property(nonatomic, assign) SEL primaryAction;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_UI_DEFAULT_BROWSER_PASSIVE_PROMO_CARD_ITEM_H_

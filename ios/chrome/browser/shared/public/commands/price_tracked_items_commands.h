// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PRICE_TRACKED_ITEMS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PRICE_TRACKED_ITEMS_COMMANDS_H_

// Commands related to Price Tracking
@protocol PriceTrackedItemsCommands

// Hides the price tracking UI.
- (void)hidePriceTrackedItems;

// Shows the price tracking UI, including the current page
// the user is navigated to in the active Tab.
- (void)showPriceTrackedItemsWithCurrentPage;

// Shows the price tracking UI, showing the price tracked items
// only (no current page).
- (void)showPriceTrackedItems;

// Shows the price tracking IPH.
- (void)presentPriceTrackedItemsWhileBrowsingIPH;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PRICE_TRACKED_ITEMS_COMMANDS_H_

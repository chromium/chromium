// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_ACTIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_ACTIONS_DELEGATE_H_

// Delegate for actions to be reported back to the NTP.
@protocol NewTabPageActionsDelegate

// The recent tab tile has been tapped.
- (void)recentTabTileOpenedAtIndex:(NSUInteger)index;

// A distant tab resumption tile has been tapped.
- (void)distantTabResumptionOpenedAtIndex:(NSUInteger)index;

// The recent tab tile has been displayed.
- (void)recentTabTileDisplayedAtIndex:(NSUInteger)index;

// A distant tab resumption tile has been displayed.
- (void)distantTabResumptionDisplayedAtIndex:(NSUInteger)index;

// A feed article has been tapped.
- (void)feedArticleOpened;

// A most visited tile has been tapped.
- (void)mostVisitedTileOpened;

// A shortcut tile has been tapped.
- (void)shortcutTileOpened;

// A Set Up List item has been tapped.
- (void)setUpListItemOpened;

// The Safety Check module was tapped.
- (void)safetyCheckOpened;

// The Parcel Tracking module was tapped.
- (void)parcelTrackingOpened;

// The Price Tracking Promo module was tapped.
- (void)priceTrackingPromoOpened;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_ACTIONS_DELEGATE_H_

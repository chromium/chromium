// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTENT_DELEGATE_H_

// Delegate for actions relating to the NTP content.
@protocol NewTabPageContentDelegate

// YES if the content requires the header to stick while scrolling.
- (BOOL)isContentHeaderSticky;

// YES if the "Return to recent tab" tile is currently visible.
- (BOOL)isRecentTabTileVisible;

// Handles what happens when the signin promo changes visibility in the NTP.
- (void)signinPromoHasChangedVisibility:(BOOL)visible;

// Signals to the receiver that omnibox edit state should be cancelled.
- (void)cancelOmniboxEdit;

// Signals to the receiver that the Fakebox is blurring.
- (void)onFakeboxBlur;

// Signal to the Omnibox to enter the focused state.
- (void)focusOmnibox;

// Refreshes NTP content, such as content suggestions and feed.
- (void)refreshNTPContent;

// Updates the NTP for the selected feed.
- (void)updateForSelectedFeed:(FeedType)selectedFeed;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTENT_DELEGATE_H_

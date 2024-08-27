// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_DELEGATE_H_

// Delegate to communicate back to the NewTabPageCoordinator
@protocol NewTabPageDelegate

// Called when the feed layout needs updating. e.g. An inner view like
// ContentSuggestions height might have changed and the Feed needs to update its
// layout to reflect this.
- (void)updateFeedLayout;

// Called when the NTP's content offset needs to be set to return to the top of
// the page.
- (void)setContentOffsetToTop;

// Returns whether Google is the user's default search engine.
- (BOOL)isGoogleDefaultSearchEngine;

// Returns whether the current NTP is a start surface.
- (BOOL)isStartSurface;

// Called when the feed top section is manually dismissed.
- (void)handleFeedTopSectionClosed;

// Returns whether sign-in is enabled for the user.
- (BOOL)isSignInAllowed;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_DELEGATE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_METRICS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_METRICS_DELEGATE_H_

// Delegate for actions to be reported back to the NTP metrics recorder.
@protocol NewTabPageMetricsDelegate

// The recent tab tile has been tapped.
- (void)recentTabTileOpened;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_METRICS_DELEGATE_H_

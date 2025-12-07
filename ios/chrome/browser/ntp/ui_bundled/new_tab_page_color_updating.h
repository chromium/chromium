// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COLOR_UPDATING_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COLOR_UPDATING_H_

// A protocol for UI components that update their background colors
// based on the current NTP color palette.
@protocol NewTabPageColorUpdating

// Sets the background using the current color palette, or defaults if none is
// set.
- (void)applyBackgroundColors;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COLOR_UPDATING_H_

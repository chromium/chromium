// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_OMNIBOX_POSITIONING_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_OMNIBOX_POSITIONING_H_

// Protocol for information relating to the NTP's fake omnibox.
@protocol NewTabPageOmniboxPositioning

// Returns the height of the fake omnibox to stick to the top of the NTP.
- (CGFloat)stickyOmniboxHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_OMNIBOX_POSITIONING_H_

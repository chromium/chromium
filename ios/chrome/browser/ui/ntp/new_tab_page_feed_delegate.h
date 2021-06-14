// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEED_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEED_DELEGATE_H_

// Delegate for providing feed information to the content suggestions.
@protocol NewTabPageFeedDelegate

// YES if we're using the refactored NTP and the Discover Feed is visible.
- (BOOL)isNTPRefactoredAndFeedVisible;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEED_DELEGATE_H_

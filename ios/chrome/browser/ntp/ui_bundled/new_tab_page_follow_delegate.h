// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FOLLOW_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FOLLOW_DELEGATE_H_

// Delegate for getting information relating to Following.
@protocol NewTabPageFollowDelegate

// Returns the number of publishers the user follows.
- (NSUInteger)followedPublisherCount;

// Returns whether the user has content in their Following feed.
- (BOOL)doesFollowingFeedHaveContent;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FOLLOW_DELEGATE_H_

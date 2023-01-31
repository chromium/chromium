// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_DELEGATE_H_

// Protocol for events related to the feed.
@protocol FeedDelegate

// Informs the delegate that the ContentSuggestionsViewController has been
// updated.
- (void)contentSuggestionsWasUpdated;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_DELEGATE_H_

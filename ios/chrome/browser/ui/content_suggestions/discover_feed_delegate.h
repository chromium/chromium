// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_DELEGATE_H_

// Protocol for events related to the Discover Feed.
@protocol DiscoverFeedDelegate

// Informs the DiscoverFeedDelegate that the FeedViewController needs to be
// re-created.
- (void)recreateDiscoverFeedViewController;

// Returns current safe area insets for the window owning this discover feed.
- (UIEdgeInsets)safeAreaInsetsForDiscoverFeed;

// Informs the DiscoverFeedDelegate that the ContentSuggestionsViewController
// has been updated.
- (void)contentSuggestionsWasUpdated;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_DELEGATE_H_

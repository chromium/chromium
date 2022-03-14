// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_

// Audience for the ContentSuggestions, getting informations from it.
@protocol ContentSuggestionsViewControllerAudience

// Notifies the audience that the promo has been shown.
- (void)promoShown;

// Notifies the audience of the UIKit viewDidDisappear: callback.
- (void)viewDidDisappear;

// Notifies the audience that the Return to Recent Tab tile has been added.
- (void)returnToRecentTabWasAdded;

// Returns current safe area insets for the window owning this discover feed.
// TODO:(crbug.com/1285378) Remove this after Content Suggestions header is
// moved out the Content Suggestions CollectionView.
- (UIEdgeInsets)safeAreaInsetsForDiscoverFeed;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_

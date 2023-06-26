// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_

@class SetUpListItemViewData;

// Audience for the ContentSuggestions, getting information from it.
@protocol ContentSuggestionsViewControllerAudience

// Notifies the audience of the UIKit viewWillDisappear: callback.
- (void)viewWillDisappear;

// Notifies the audience that the Return to Recent Tab tile has been added.
- (void)returnToRecentTabWasAdded;

// Notifies the audience that a module was removed.
- (void)moduleWasRemoved;

// Returns current safe area insets for the window owning this discover feed.
// TODO:(crbug.com/1285378) Remove this after Content Suggestions header is
// moved out the Content Suggestions CollectionView.
- (UIEdgeInsets)safeAreaInsetsForDiscoverFeed;

// Notifies the audience to present the Set Up List Show More Menu.
- (void)showSetUpListShowMoreMenu;

// Notifies the audience that the module of `type` should be never shown
// anymore.
- (void)neverShowModuleType:(ContentSuggestionsModuleType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_

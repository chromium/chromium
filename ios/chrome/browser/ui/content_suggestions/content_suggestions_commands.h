// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_

@class CollectionViewItem;
@class QuerySuggestionConfig;

// Commands protocol allowing the ContentSuggestions ViewControllers to interact
// with the coordinator layer, and from there to the rest of the application.
@protocol ContentSuggestionsCommands

// Opens the Most Visited associated with this `item` at the `mostVisitedItem`.
- (void)openMostVisitedItem:(NSObject*)item atIndex:(NSInteger)mostVisitedIndex;

// Handles the actions tapping the "Return to Recent Tab" item that returns the
// user to the last opened tab.
- (void)openMostRecentTab;

// Opens the displayed tab resumption item.
- (void)openTabResumptionItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_

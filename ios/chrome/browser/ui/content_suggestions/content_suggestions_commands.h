// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_

// Commands related to Content Suggestions.
@protocol ContentSuggestionsCommands

// Show the "See More" Set Up List view, which shows all items in the list.
// If `expanded` is YES, the view will use the "large" detent by default,
// otherwise the "medium" detent will be used.
- (void)showSetUpListSeeMoreMenuExpanded:(BOOL)expanded;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_

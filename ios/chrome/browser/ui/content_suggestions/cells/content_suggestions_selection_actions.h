// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_SELECTION_ACTIONS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_SELECTION_ACTIONS_H_

// Protocol notifying the receiver of user tap actions to the Content
// Suggestions content.
@protocol ContentSuggestionsSelectionActions

// Indicates to the receiver that a Content Suggestion element was tapped.
- (void)contentSuggestionsElementTapped:(UIGestureRecognizer*)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_SELECTION_ACTIONS_H_

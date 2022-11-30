// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MENU_PROVIDER_H_

@class ContentSuggestionsMostVisitedItem;

// Protocol for instances that will provide menus to ContentSuggestions
// components.
@protocol ContentSuggestionsMenuProvider

// Creates a context menu configuration instance for the given `item`, which is
// represented on the UI by `view`.
- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (ContentSuggestionsMostVisitedItem*)item
                                                      fromView:(UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MENU_PROVIDER_H_

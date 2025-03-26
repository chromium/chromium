// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CONTENT_SUGGESTIONS_MENU_ELEMENTS_PROVIDER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CONTENT_SUGGESTIONS_MENU_ELEMENTS_PROVIDER_H_

@class ContentSuggestionsMostVisitedItem;

// Protocol for instances that will provide menus to ContentSuggestions
// components.
@protocol ContentSuggestionsMenuElementsProvider

// Returns the default menu elements that should be shown in the context menu
// for the given `item`, which is represented on the UI by `view`.
- (NSArray<UIMenuElement*>*)defaultContextMenuElementsForItem:
                                (ContentSuggestionsMostVisitedItem*)item
                                                     fromView:(UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CONTENT_SUGGESTIONS_MENU_ELEMENTS_PROVIDER_H_

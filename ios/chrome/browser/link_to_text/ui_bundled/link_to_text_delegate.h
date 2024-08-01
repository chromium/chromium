// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_UI_BUNDLED_LINK_TO_TEXT_DELEGATE_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_UI_BUNDLED_LINK_TO_TEXT_DELEGATE_H_

@protocol UIMenuBuilder;

// Protocol for handling link to text and presenting related UI.
@protocol LinkToTextDelegate

// Returns whether the link to text feature should be offered for the current
// user selection.
- (BOOL)shouldOfferLinkToText;

// Handles the link to text menu item selection.
- (void)handleLinkToTextSelection;

// Will be called by `BrowserContainerViewController buildMenuWithBuilder:`
// to customize its edit menu.
- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_UI_BUNDLED_LINK_TO_TEXT_DELEGATE_H_

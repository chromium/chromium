// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_SYNCHRONIZING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_SYNCHRONIZING_H_

#import <UIKit/UIKit.h>

// Synchronizing protocol allowing the ContentSuggestionsViewController to
// synchronize with the header, containing the fake omnibox and the logo.
@protocol ContentSuggestionsHeaderSynchronizing

// |YES| if its view is visible.  When set to |NO| various UI updates are
// ignored.
@property(nonatomic, assign, getter=isShowing) BOOL showing;

// Handles the scroll of the collection and unfocus the omnibox if needed.
// Updates the fake omnibox to adapt to the current scrolling.
- (void)updateFakeOmniboxOnCollectionScroll;

// Updates the fake omnibox to adapt to the current orientation.
- (void)updateFakeOmniboxOnNewWidth:(CGFloat)width;

// Unfocuses the omnibox.
- (void)unfocusOmnibox;

// Update any dynamic constraints.
- (void)updateConstraints;

// Returns the Y value to use for the scroll view's contentOffset when scrolling
// the omnibox to the top of the screen.
- (CGFloat)pinnedOffsetY;

// Returns the height of the header.
- (CGFloat)headerHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_SYNCHRONIZING_H_

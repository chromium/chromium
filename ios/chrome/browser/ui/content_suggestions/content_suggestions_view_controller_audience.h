// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_

// Audience for the ContentSuggestions, getting informations from it.
@protocol ContentSuggestionsViewControllerAudience

// Notifies the audience that the promo has been shown.
- (void)promoShown;

// Notifies the audience that the Discover feed header menu has been shown, and
// provides a reference to the button.
- (void)discoverHeaderMenuButtonShown:(UIView*)menuButton;

// Notifies the audience that the Discover Feed has been shown.
// TODO(crbug.com/1126940): This is still a best effort approach and might be
// called multiple times.
- (void)discoverFeedShown;

// Notifies the audience of the UIKit viewDidDisappear: callback.
- (void)viewDidDisappear;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_

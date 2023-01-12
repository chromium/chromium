// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// Delegate for the ContentSuggestionsHeaderViewController.
@protocol ContentSuggestionsHeaderViewControllerDelegate

// Whether the scrollview is scrolled to the omnibox.
@property(nonatomic, assign, readonly) BOOL scrolledToMinimumHeight;

// Indicates to the receiver to update its state to focus the omnibox.
- (void)focusFakebox;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_

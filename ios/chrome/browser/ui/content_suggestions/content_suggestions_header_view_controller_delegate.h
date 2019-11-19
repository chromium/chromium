// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol UserAccountImageUpdateDelegate;

// Delegate for the ContentSuggestionsHeaderViewController.
@protocol ContentSuggestionsHeaderViewControllerDelegate

// Returns whether a context menu is visible.
- (BOOL)isContextMenuVisible;

// Returns whether the collection is scrolled to its top.
- (BOOL)isScrolledToTop;

// Register |imageUpdater| object as delegate to refresh UI when user account
// avatar is changed.
- (void)registerImageUpdater:(id<UserAccountImageUpdateDelegate>)imageUpdater;

// Returns whether calls that may trigger a URL load are allowed, such as a
// voice search or focusing the omnibox via the fakebox.
// See: crbug.com/925304 for more context.  Remove this when ios/web supports
// queueing multiple loads during this state.
- (BOOL)ignoreLoadRequests;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol UserAccountImageUpdateDelegate;

// Delegate for the ContentSuggestionsHeaderViewController.
@protocol ContentSuggestionsHeaderViewControllerDelegate

// Returns whether the collection is scrolled to the omnibox.
- (BOOL)isScrolledToMinimumHeight;

// Register `imageUpdater` object as delegate to refresh UI when user account
// avatar is changed.
- (void)registerImageUpdater:(id<UserAccountImageUpdateDelegate>)imageUpdater;

// Returns whether the user is signed in for signin::ConsentLevel::kSignin.
- (BOOL)isSignedIn;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNACKBAR_UI_BUNDLED_UI_SNACKBAR_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_SNACKBAR_UI_BUNDLED_UI_SNACKBAR_VIEW_DELEGATE_H_

@class SnackbarView;

// Delegate for SnackbarView.
@protocol SnackbarViewDelegate <NSObject>

// Called when the action button is tapped.
- (void)snackbarViewDidTapActionButton:(SnackbarView*)snackbarView;

// Called when the snackbar view requests to be dismissed.
- (void)snackbarViewDidRequestDismissal:(SnackbarView*)snackbarView
                               animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_SNACKBAR_UI_BUNDLED_UI_SNACKBAR_VIEW_DELEGATE_H_

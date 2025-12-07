// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNACKBAR_UI_BUNDLED_UI_SNACKBAR_VIEW_H_
#define IOS_CHROME_BROWSER_SNACKBAR_UI_BUNDLED_UI_SNACKBAR_VIEW_H_

#import <UIKit/UIKit.h>

@class SnackbarMessage;
@protocol SnackbarViewDelegate;

// A view that displays a snackbar message.
@interface SnackbarView : UIView

// The message to display.
@property(nonatomic, strong, readonly) SnackbarMessage* message;

// The delegate for this view.
@property(nonatomic, weak) id<SnackbarViewDelegate> delegate;

// The bottom offset for the snackbar.
@property(nonatomic, assign) CGFloat bottomOffset;

// Designated initializer.
- (instancetype)initWithMessage:(SnackbarMessage*)message
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Presents the snackbar.
- (void)presentAnimated:(BOOL)animated completion:(void (^)(void))completion;

// Dismisses the snackbar.
- (void)dismissAnimated:(BOOL)animated completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_SNACKBAR_UI_BUNDLED_UI_SNACKBAR_VIEW_H_

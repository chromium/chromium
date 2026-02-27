// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_SNACKBAR_PRESENTER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_SNACKBAR_PRESENTER_H_

#import <UIKit/UIKit.h>

class Browser;

// Presents snackbars related to the Composebox feature.
@interface ComposeboxSnackbarPresenter : NSObject

// Shows a snackbar with the attachment limit message.
- (void)showSnackbarForAttachmentLimit:(NSUInteger)attachmentLimit;

// Shows a snackbar with the attachment limit message for an image generation
// prompt with a bottom offset.
- (void)showAttachmentLimitForImageGenerationSnackbarWithBottomOffset:
    (CGFloat)bottomOffset;

// Shows a snackbar with the tab limit message.
- (void)showSnackbarForTabAttachmentLimit:(NSUInteger)attachmentLimit;

// Shows a snackbar with the attachment limit message with a bottom offset.
- (void)showSnackbarForAttachmentLimit:(NSUInteger)attachmentLimit
                          bottomOffset:(CGFloat)bottomOffset;

// Shows a snackbar with a generic error message with a bottom offset.
- (void)showUnableToAddAttachmentSnackbarWithBottomOffset:(CGFloat)bottomOffset;

// Shows a snackbar with a generic error message if a tab can't be reloaded.
- (void)showCannotReloadTabError;

// Dismisses all presented snackbars.
- (void)dismissAllSnackbars;

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_SNACKBAR_PRESENTER_H_

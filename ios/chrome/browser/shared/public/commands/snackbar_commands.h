// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SNACKBAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SNACKBAR_COMMANDS_H_

#import <UIKit/UIKit.h>

@class SnackbarMessage;

// Commands related to Snackbar.
@protocol SnackbarCommands

// Shows a snackbar using a SnackbarMessage. On navigation controllers,
// use the bottom toolbar height as bottom offset. Otherwise, use the browser's
// bottom toolbar height as bottom offset.
- (void)showSnackbarMessage:(SnackbarMessage*)message;

// Shows a snackbar using a SnackbarMessage. Use the browser's bottom
// toolbar height as bottom offset. This is used when the presented view will be
// dismissed and web content will become visible.
- (void)showSnackbarMessageOverBrowserToolbar:(SnackbarMessage*)message;

// Shows a snackbar using a SnackbarMessage, with haptic feedback.
- (void)showSnackbarMessage:(SnackbarMessage*)message
             withHapticType:(UINotificationFeedbackType)type;

// Shows a snackbar using a SnackbarMessage, with a specific bottom
// offset.
- (void)showSnackbarMessage:(SnackbarMessage*)message
               bottomOffset:(CGFloat)offset;

// Shows a snackbar using a SnackbarMessage after dismissing the
// keyboard if present.
- (void)showSnackbarMessageAfterDismissingKeyboard:(SnackbarMessage*)message;

// Shows a snackbar displaying a message with `messageText` and a button with
// `buttonText` which triggers `messageAction` on tap. `completionAction` will
// be called when the snackbar finishes presenting, BOOL is YES if the dismissal
// was caused by a user action and NO if not. It will use the Bottom toolbar
// height as bottom offset. Use this method if displaying a Snackbar while the
// Web content is visible. If there's no bottom toolbar offset will be 0.
- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction;

// Shows a snackbar displaying a message with `messageText` and a button with
// `buttonText` and `buttonAccessibilityHint` which triggers `messageAction` on
// tap. `completionAction` will be called when the snackbar finishes presenting,
// BOOL is YES if the dismissal was caused by a user action and NO if not. It
// will use the Bottom toolbar height as bottom offset. Use this method if
// displaying a Snackbar while the Web content is visible. If there's no bottom
// toolbar offset will be 0.
- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
        buttonAccessibilityHint:(NSString*)buttonAccesibilityHint
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction;

// Dismisses all presented snackbars.
- (void)dismissAllSnackbars;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SNACKBAR_COMMANDS_H_

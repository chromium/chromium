// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SNACKBAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SNACKBAR_COMMANDS_H_

#import <UIKit/UIKit.h>

@class MDCSnackbarMessage;

// Commands related to Snackbar.
@protocol SnackbarCommands

// Shows a snackbar with `message`. On navigation controllers, use the bottom
// toolbar height as bottom offset. Otherwise, use the browser's bottom toolbar
// height as bottom offset.
- (void)showSnackbarMessage:(MDCSnackbarMessage*)message;

// Shows a snackbar with `message`. Use the browser's bottom toolbar height as
// bottom offset. This is used when the presented view will be dismissed and web
// content will become visible.
- (void)showSnackbarMessageOverBrowserToolbar:(MDCSnackbarMessage*)message;

// Shows a snackbar with `message` while having a haptic feedback with `type`.
- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
             withHapticType:(UINotificationFeedbackType)type;

// Shows a snackbar with `message` using `bottomOffset` as bottom offset.
- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
               bottomOffset:(CGFloat)offset;

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

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SNACKBAR_COMMANDS_H_

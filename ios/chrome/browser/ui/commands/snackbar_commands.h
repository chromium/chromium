// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SNACKBAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SNACKBAR_COMMANDS_H_

#import <UIKit/UIKit.h>

@class MDCSnackbarMessage;

// Commands related to Snackbar.
@protocol SnackbarCommands

// Shows a snackbar with |message|. It will use the Bottom toolbar height as
// bottom offset. Use this method if displaying a Snackbar while the Web content
// is visible. If there's no bottom toolbar offset will be 0.
- (void)showSnackbarMessage:(MDCSnackbarMessage*)message;

// Shows a snackbar with |message| using |bottomOffset| as bottom offset.
- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
               bottomOffset:(CGFloat)offset;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SNACKBAR_COMMANDS_H_

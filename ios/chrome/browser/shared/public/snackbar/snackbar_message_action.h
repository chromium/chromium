// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_SNACKBAR_SNACKBAR_MESSAGE_ACTION_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_SNACKBAR_SNACKBAR_MESSAGE_ACTION_H_

#import <Foundation/Foundation.h>

// An action to be performed by a snackbar.
@interface SnackbarMessageAction : NSObject

// The title of the action button.
@property(nonatomic, copy) NSString* title;

// The accessibility hint of the action button.
@property(nonatomic, copy) NSString* accessibilityHint;

// The handler to be called when the action button is tapped.
@property(nonatomic, copy) void (^handler)(void);

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_SNACKBAR_SNACKBAR_MESSAGE_ACTION_H_

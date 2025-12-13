// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_SNACKBAR_SNACKBAR_MESSAGE_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_SNACKBAR_SNACKBAR_MESSAGE_H_

#import <UIKit/UIKit.h>

@class SnackbarMessageAction;

// A message to be displayed in a snackbar.
@interface SnackbarMessage : NSObject

// The title of the message.
@property(nonatomic, copy) NSString* title;

// The optional subtitle of the message.
@property(nonatomic, copy) NSString* subtitle;

// The optional secondary subtitle of the message.
@property(nonatomic, copy) NSString* secondarySubtitle;

// An optional image to be displayed on the leading side of the snackbar.
@property(nonatomic, strong) UIImage* leadingAccessoryImage;

// An optional image to be displayed on the trailing side of the snackbar.
// A snackbar cannot have both a `trailingAccessoryImage` and an `action`.
@property(nonatomic, strong) UIImage* trailingAccessoryImage;

// Whether the leading accessory view should be rounded. Defaults to NO.
@property(nonatomic, assign) BOOL roundLeadingAccessoryView;

// Whether the trailing accessory view should be rounded. Defaults to NO.
@property(nonatomic, assign) BOOL roundTrailingAccessoryView;

// The optional action to be performed when the button is tapped.
// A snackbar cannot have both an `action` and a `trailingAccessoryImage`.
@property(nonatomic, strong) SnackbarMessageAction* action;

// The duration the message should be displayed. Defaults to 4 seconds.
// A value of 0 will dismiss the snackbar immediately after it is shown.
@property(nonatomic, assign) NSTimeInterval duration;

// The completion handler to be called when the message is dismissed.
@property(nonatomic, copy) void (^completionHandler)(BOOL);

// Designated initializer.
- (instancetype)initWithTitle:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_SNACKBAR_SNACKBAR_MESSAGE_H_

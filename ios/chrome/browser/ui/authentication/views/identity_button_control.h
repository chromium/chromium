// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_VIEWS_IDENTITY_BUTTON_CONTROL_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_VIEWS_IDENTITY_BUTTON_CONTROL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/views/views_constants.h"

// Enum to choose the arrow on the right of IdentityButtonControl.
typedef NS_ENUM(NSInteger, IdentityButtonControlArrowDirection) {
  // Adds a down arrow on the right of the view.
  IdentityButtonControlArrowDown,
  // Adds a right arrow on the right of the view.
  IdentityButtonControlArrowRight,
};

// Displays the name, email and avatar of a chrome identity, as a control.
// An down arrow is also displayed on the right of the control, to invite the
// user to tap and select another chrome identity. To get the tap event, see:
// -[UIControl addTarget:action:forControlEvents:].
@interface IdentityButtonControl : UIControl

// Arrow direction, default value is IdentityButtonControlArrowDown.
@property(nonatomic, assign) IdentityButtonControlArrowDirection arrowDirection;

// Style for the IdentityView.
@property(nonatomic, assign) IdentityViewStyle identityViewStyle;

// Initialises IdentityButtonControl.
- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;

// See -[IdentityButtonControl initWithFrame:].
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Set the identity avatar shown.
- (void)setIdentityAvatar:(UIImage*)identityAvatar;

// Set the name and email shown. `name` can be nil.
- (void)setIdentityName:(NSString*)name email:(NSString*)email;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_VIEWS_IDENTITY_BUTTON_CONTROL_H_

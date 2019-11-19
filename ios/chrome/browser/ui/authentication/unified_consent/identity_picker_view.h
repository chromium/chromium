// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_PICKER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_PICKER_VIEW_H_

#import <UIKit/UIKit.h>

// Displays the name, email and avatar of a chrome identity, as a control.
// An down arrow is also displayed on the right of the control, to invite the
// user to tap and select another chrome identity. To get the tap event, see:
// -[UIControl addTarget:action:forControlEvents:].
@interface IdentityPickerView : UIControl

// Initialises IdentityPickerView.
- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;

// See -[IdentityPickerView initWithFrame:].
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Set the identity avatar shown.
- (void)setIdentityAvatar:(UIImage*)identityAvatar;

// Set the name and email shown. |name| can be nil.
- (void)setIdentityName:(NSString*)name email:(NSString*)email;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_PICKER_VIEW_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_VIEW_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_VIEW_H_

#import <UIKit/UIKit.h>

// The view that is used to overlay over non-authorized incognito content.
@interface IncognitoReauthView : UIView

// Button that allows biometric authentication.
// Will auto-adjust its string based on the available authentication methods on
// the user's device.
@property(nonatomic, strong, readonly) UIButton* authenticateButton;
// A secondary button on the screen that can be used for different purposes.
// Currently used to go to the normal tab grid or to close Incognito tabs.
@property(nonatomic, strong, readonly) UIButton* secondaryButton;
// The image view with the incognito logo.
@property(nonatomic, strong, readonly) UIView* logoView;

// Method to set the label text and accessibility label of the authentication
// button (primary button).
- (void)setAuthenticateButtonText:(NSString*)text
               accessibilityLabel:(NSString*)accessibilityLabel;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_VIEW_H_

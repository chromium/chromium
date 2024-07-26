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
// The button to go to the tab switcher.
@property(nonatomic, strong, readonly) UIButton* tabSwitcherButton;
// The image view with the incognito logo.
@property(nonatomic, strong, readonly) UIView* logoView;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_VIEW_H_

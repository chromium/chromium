// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SIGNIN_SETTINGS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SIGNIN_SETTINGS_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// The app interface for sign-in settings tests.
@interface SigninSettingsAppInterface : NSObject

// Sets the kIosSettingsSigninPromoDisplayedCount value, related to the
// number of time the sign-in promo has been displayed.
+ (void)setSettingsSigninPromoDisplayedCount:(int)displayedCount;

// Returns the kIosSettingsSigninPromoDisplayedCount value, related to
// the number of time the sign-in promo has been displayed.
+ (int)settingsSigninPromoDisplayedCount;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SIGNIN_SETTINGS_APP_INTERFACE_H_

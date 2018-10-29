// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_UPDATE_PASSWORD_INFOBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_UPDATE_PASSWORD_INFOBAR_CONTROLLER_H_

#import "ios/chrome/browser/passwords/ios_password_infobar_controller.h"

class IOSChromeUpdatePasswordInfoBarDelegate;

// Controller for the Update Password info bar. Presents an info bar that asks
// the user whether they want to update their password.
@interface UpdatePasswordInfoBarController : IOSPasswordInfoBarController

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)
initWithBaseViewController:(UIViewController*)baseViewController
           infoBarDelegate:(IOSChromeUpdatePasswordInfoBarDelegate*)delegate;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_UPDATE_PASSWORD_INFOBAR_CONTROLLER_H_

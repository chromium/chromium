// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_BROWSER_OPENER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_BROWSER_OPENER_H_

#import <UIKit/UIKit.h>

// Interface for opening a URL in a browser.
@protocol ChromeIdentityBrowserOpener<NSObject>

// Opens |url| in an appropriate web browser. The request is coming from |view|
// and |viewController| may be used to present any UI necessary.
- (void)openURL:(NSURL*)url
              view:(UIView*)view
    viewController:(UIViewController*)viewController;
@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_BROWSER_OPENER_H_

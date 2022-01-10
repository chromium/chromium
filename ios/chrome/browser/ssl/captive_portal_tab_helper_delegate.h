// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

class CaptivePortalTabHelper;
class GURL;

// Delegate for CaptivePortalTabHelper.
@protocol CaptivePortalTabHelperDelegate <NSObject>

// Notifies the delegate the user selected the connect button on the captive
// portal blocking page and that the page with the specified |landingURL| should
// be displayed to the user.
- (void)captivePortalTabHelper:(CaptivePortalTabHelper*)tabHelper
         connectWithLandingURL:(const GURL&)landingURL;

@end

#endif  // IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_TAB_HELPER_DELEGATE_H_

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>


@protocol NewTabPageControllerDelegate;
@protocol PrivacyCookiesCommands;
class UrlLoadingBrowserAgent;

@interface IncognitoViewController : UIViewController

// Init with the given loader object. |loader| may be nil, but isn't
// retained so it must outlive this controller.
- (instancetype)initWithUrlLoader:(UrlLoadingBrowserAgent*)URLLoader;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_CONTROLLER_H_

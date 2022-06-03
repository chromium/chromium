// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNED_IN_ACCOUNTS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNED_IN_ACCOUNTS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>


@protocol ApplicationSettingsCommands;
class ChromeBrowserState;

// View controller that presents the signed in accounts when they have changed
// while the application was in background.
@interface SignedInAccountsViewController : UIViewController

// Returns whether the collection view should be presented for |browserState|,
// which happens when the accounts have changed while in background.
+ (BOOL)shouldBePresentedForBrowserState:(ChromeBrowserState*)browserState;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                          dispatcher:(id<ApplicationSettingsCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNED_IN_ACCOUNTS_VIEW_CONTROLLER_H_

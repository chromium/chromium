// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ReauthenticationProtocol;

// View controller that requests Local Authentication upon presentation and
// forwards the result to its delegate.
@interface ReauthenticationViewController : UIViewController

// Initializes the view controller with a `reauthenticationModule` for
// triggering Local Authentication.
- (instancetype)initWithReauthenticationModule:
    (id<ReauthenticationProtocol>)reauthenticationModule
    NS_DESIGNATED_INITIALIZER;

// Unavailable initializers.
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_VIEW_CONTROLLER_H_

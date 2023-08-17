// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ReauthenticationProtocol;

@protocol ReauthenticationViewControllerDelegate <NSObject>

// Displays an alert requesting the user to set up a passcode before getting
// access to the Password Manager.
- (void)showSetUpPasscodeDialog;

// Handle the result of Local Authentication.
- (void)reauthenticationDidFinishWithSuccess:(BOOL)success;

@end

// View controller that requests Local Authentication upon presentation and
// forwards the result to its delegate.
@interface ReauthenticationViewController : UIViewController

// Delegate of the view controller. Most likely a ReauthenticationCoordinator.
@property(nonatomic, weak) id<ReauthenticationViewControllerDelegate> delegate;

// Initializes the view controller with a `reauthenticationModule` for
// triggering Local Authentication.
// - reauthUponPresentation: Whether local authentication should be requested
// when the view controller is presented. Expected to be YES when the view
// controller is being presented to require authentication before giving access
// to a surface that has just been opened. Expected to be NO when blocking an
// already opened surface on device lock for later requiring authentication on
// device unlock.
- (instancetype)initWithReauthenticationModule:
                    (id<ReauthenticationProtocol>)reauthenticationModule
                        reauthUponPresentation:(BOOL)reauthUponPresentation
    NS_DESIGNATED_INITIALIZER;

// Unavailable initializers.
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Triggers Local Authentication. If no authentication method is available,
// displays an alert requesting the user to set a passcode.
- (void)requestAuthentication;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_VIEW_CONTROLLER_H_

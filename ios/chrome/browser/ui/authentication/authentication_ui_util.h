// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_UI_UTIL_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#include "base/strings/string16.h"

@class AlertCoordinator;
@class ActionSheetCoordinator;
class Browser;

// Sign-out result, related to SignoutActionSheetCoordinator().
typedef NS_ENUM(NSUInteger, SignoutActionSheetCoordinatorResult) {
  // The user canceled the sign-out confirmation dialog.
  SignoutActionSheetCoordinatorResultCanceled,
  // The user chose to sign-out and clear their data from the device.
  SignoutActionSheetCoordinatorResultClearFromDevice,
  // The user chose to sign-out and keep their data on the device.
  SignoutActionSheetCoordinatorResultKeepOnDevice,
};

// Sign-out completion block.
using SignoutActionSheetCoordinatorCompletion =
    void (^)(SignoutActionSheetCoordinatorResult result);

// Returns the hosted domain for the primary account.
base::string16 HostedDomainForPrimaryAccount(Browser* browser);

// Returns ActionSheetCoordinator to ask the sign-out confirmation from the
// user. The alert sheet dialog choices are based if the primary account is
// managed account or not, and if the user turned on sync or not.
// The caller is in charge to start the coordinator.
// The caller is in charge to sign-out the user and wipe the data from the
// device according to |signout_completion| value.
// |view_controller| view controller to present the action sheet.
// |view| to position the action sheet for iPad.
// |signout_completion| invoked when the user confirms or cancels sign-out.
ActionSheetCoordinator* SignoutActionSheetCoordinator(
    UIViewController* view_controller,
    Browser* browser,
    UIView* view,
    SignoutActionSheetCoordinatorCompletion signout_completion);

// Returns the sign in alert coordinator for |error|. |dismissAction| is called
// when the dialog is dismissed (the user taps on the Ok button) or cancelled
// (the alert coordinator is cancelled programatically).
AlertCoordinator* ErrorCoordinator(NSError* error,
                                   ProceduralBlock dismissAction,
                                   UIViewController* viewController,
                                   Browser* browser);

// Returns a message to display, as an error, to the user. This message
// contains:
//  * localized description (if any)
//  * domain name
//  * error code
//  * underlying errors recursively (only the domain name and the error code)
NSString* DialogMessageFromError(NSError* error);

// Returns the sign in alert coordinator for |error|, with no associated
// action. An action should be added before starting it.
AlertCoordinator* ErrorCoordinatorNoItem(NSError* error,
                                         UIViewController* viewController,
                                         Browser* browser);

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_UI_UTIL_H_

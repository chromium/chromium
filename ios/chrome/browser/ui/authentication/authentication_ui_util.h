// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_UI_UTIL_H_

#import <UIKit/UIKit.h>

#include <string>

#include "base/ios/block_types.h"

@class ActionSheetCoordinator;
@class AlertCoordinator;
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
std::u16string HostedDomainForPrimaryAccount(Browser* browser);

// Returns the sign in alert coordinator for `error`. `dismissAction` is called
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

// Returns the sign in alert coordinator for `error`, with no associated
// action. An action should be added before starting it.
AlertCoordinator* ErrorCoordinatorNoItem(NSError* error,
                                         UIViewController* viewController,
                                         Browser* browser);

// Returns a string for the view controller presentation status. This string
// can only be used for class description for debug purposes.
// `view_controller` can be nil.
NSString* ViewControllerPresentationStatusDescription(
    UIViewController* view_controller);

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_UI_UTIL_H_

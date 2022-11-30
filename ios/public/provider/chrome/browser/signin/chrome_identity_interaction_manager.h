// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_INTERACTION_MANAGER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_INTERACTION_MANAGER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@protocol SystemIdentity;

// Error domain for Chrome identity errors.
extern NSString* kChromeIdentityErrorDomain;

typedef enum {
  CHROME_IDENTITY_OPERATION_ONGOING = -200,
} ChromeIdentityErrorCode;

// Callback for the signin operation methods.
// * `identity` is the identity that was added/reauthenticated.
// * `error` is nil unless there was an error during the operation.
typedef void (^SigninCompletionCallback)(id<SystemIdentity> identity,
                                         NSError* error);

// ChromeIdentityInteractionManager abstracts the interaction to add identities
// on iOS.
@interface ChromeIdentityInteractionManager : NSObject

// If `userEmail` is not set:
// Starts the add account operation for a user. Presents user with the screen to
// enter credentials.
// If `userEmail` is set:
// Starts the reauthentication operation for a user. Presents user with the
// screen to enter credentials with the email pre-entered.
// Note: Calling this method will fail and the completion will be called with a
// CHROME_IDENTITY_OPERATION_ONGOING error if there is already another add
// account or reauthenticate operation ongoing.
// * `viewController` will display the add account screens.
// * `userEmail` will be pre-entered on the presented screen.
// * `completion` will be called once the operation has finished.
- (void)addAccountWithPresentingViewController:(UIViewController*)viewController
                                     userEmail:(NSString*)userEmail
                                    completion:
                                        (SigninCompletionCallback)completion;

// Cancels and dismisses any currently active operation.
// * `animated` represents whether the UI should be dismissed with an animation.
// * `completion` will be called once the operation has finished.
- (void)cancelAddAccountAnimated:(BOOL)animated
                      completion:(ProceduralBlock)completion;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_INTERACTION_MANAGER_H_

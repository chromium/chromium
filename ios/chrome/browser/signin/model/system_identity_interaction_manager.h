// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_INTERACTION_MANAGER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_INTERACTION_MANAGER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@protocol SystemIdentity;

// Callback for the signin operation methods.
using SigninCompletionBlock = void (^)(id<SystemIdentity>, NSError*);

// Protocol abstracting the interaction for adding identities on iOS.
@protocol SystemIdentityInteractionManager

// Starts the activity to add an account (if `userEmail` is nil or empty)
// or to re-authenticate an account (if `userEmail` is not empty). For the
// re-authentication, the screen to enter the credential will have the email
// field pre-entered.
//
// The activity will fail is there already an authentication activity in
// progress.
//
// The activity will be displayed in `viewController`, if `userEmail` is set
// it will be used for re-authentication and will be pre-entered in the screen
// presented. The `completion` will be invoked on the calling sequence when
// the activity completes.
// `completion` must not be `nullptr`.
- (void)startAuthActivityWithViewController:(UIViewController*)viewController
                                  userEmail:(NSString*)userEmail
                                 completion:(SigninCompletionBlock)completion;

// Cancels and dismisses any currently active operation. `animated` controls
// whether the dimissal is animated or not. The `completion` will be invoked
// on the calling sequence when the operation completes.
- (void)cancelAuthActivityAnimated:(BOOL)animated
                        completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_INTERACTION_MANAGER_H_

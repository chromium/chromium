// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MULTI_PROFILE_PASSKEY_CREATION_VIEW_CONTROLLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MULTI_PROFILE_PASSKEY_CREATION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@class MultiProfilePasskeyCreationViewController;
@class PasskeyRequestDetails;

// Delegate for the MultiProfilePasskeyCreationViewController.
@protocol MultiProfilePasskeyCreationViewControllerDelegate

// Dismisses the `passkeyWelcomeScreenViewController`.
- (void)multiProfilePasskeyCreationViewControllerShouldBeDismissed:
    (MultiProfilePasskeyCreationViewController*)
        multiProfilePasskeyCreationViewController;

// Attempts to create a passkey if validation succeeds. Exits with an error code
// otherwise.
- (void)validateUserAndCreatePasskeyWithDetails:
            (PasskeyRequestDetails*)passkeyRequestDetails
                                           gaia:(NSString*)gaia;

@end

// Displays a view asking the user to confirm that they agree to create a
// passkey in the provided account.
@interface MultiProfilePasskeyCreationViewController : PromoStyleViewController

// Designated initializer.
- (instancetype)
            initWithDetails:(PasskeyRequestDetails*)passkeyRequestDetails
                       gaia:(NSString*)gaia
                  userEmail:(NSString*)userEmail
                    favicon:(NSString*)favicon
    navigationItemTitleView:(UIView*)navigationItemTitleView
                   delegate:
                       (id<MultiProfilePasskeyCreationViewControllerDelegate>)
                           delegate NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MULTI_PROFILE_PASSKEY_CREATION_VIEW_CONTROLLER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSKEY_WELCOME_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSKEY_WELCOME_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// Possible purposes for showing the passkey welcome screen.
enum class PasskeyWelcomeScreenPurpose {
  kEnroll,
  kFixDegradedRecoverability,
  kReauthenticate,
};

// Screen shown to the user when they need to enroll or re-authenticate for
// passkeys.
@interface PasskeyWelcomeScreenViewController : PromoStyleViewController

// Designated initializer. `purpose` indicates the purpose for which the passkey
// welcome screen needs to be shown, which impacts the screen's content.
// `primaryButtonAction` is the block to execute when the primary button
// displayed in the view is tapped.
- (instancetype)initForPurpose:(PasskeyWelcomeScreenPurpose)purpose
           primaryButtonAction:(ProceduralBlock)primaryButtonAction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSKEY_WELCOME_SCREEN_VIEW_CONTROLLER_H_

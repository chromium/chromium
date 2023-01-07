// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_LEGACY_SIGNIN_LEGACY_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_LEGACY_SIGNIN_LEGACY_SIGNIN_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/first_run/legacy_signin/legacy_signin_screen_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// Delegate of sign-in screen view controller.
@protocol
    LegacySigninScreenViewControllerDelegate <PromoStyleViewControllerDelegate>

// Called when the user taps to see the account picker.
- (void)showAccountPickerFromPoint:(CGPoint)point;

// Logs scrollability metric on view appears.
- (void)logScrollButtonVisible:(BOOL)scrollButtonVisible
            withIdentityPicker:(BOOL)identityPickerVisible
                     andFooter:(BOOL)footerVisible;

@end

// View controller of sign-in screen.
@interface LegacySigninScreenViewController
    : PromoStyleViewController <LegacySigninScreenConsumer>

@property(nonatomic, weak) id<LegacySigninScreenViewControllerDelegate>
    delegate;

@property(nonatomic, assign)
    EnterpriseSignInRestrictions enterpriseSignInRestrictions;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_LEGACY_SIGNIN_LEGACY_SIGNIN_SCREEN_VIEW_CONTROLLER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"

#import "base/check_op.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PostRestoreSignInProvider

#pragma mark - PromoProtocol

// Conditionally returns the promo identifier (promos_manager::Promo) based on
// which variation of the Post Restore Sign-in Promo is currently active.
- (promos_manager::Promo)identifier {
  post_restore_signin::features::PostRestoreSignInType promoType =
      post_restore_signin::features::CurrentPostRestoreSignInType();

  // PostRestoreSignInProvider should not exist unless the feature
  // `kIOSNewPostRestoreExperience` is enabled. Therefore, `promoType` should
  // never be `kDisabled` here.
  DCHECK_NE(promoType,
            post_restore_signin::features::PostRestoreSignInType::kDisabled);

  if (promoType ==
      post_restore_signin::features::PostRestoreSignInType::kFullscreen) {
    return promos_manager::Promo::PostRestoreSignInFullscreen;
  } else if (promoType ==
             post_restore_signin::features::PostRestoreSignInType::kAlert) {
    return promos_manager::Promo::PostRestoreSignInAlert;
  }

  // PostRestoreSignInProvider should not exist unless the feature
  // `kIOSNewPostRestoreExperience` is enabled. Therefore, this code path should
  // never be reached.
  NOTREACHED();

  // Returns the fullscreen, FRE-like promo as the default.
  return promos_manager::Promo::PostRestoreSignInFullscreen;
}

#pragma mark - StandardPromoAlertHandler

- (void)standardPromoAlertDefaultAction {
  // TODO(crbug.com/1363283): Implement `standardPromoAlertDefaultAction`.
}

- (void)standardPromoAlertCancelAction {
  // TODO(crbug.com/1363283): Implement `standardPromoAlertCancelAction`.
}

#pragma mark - StandardPromoAlertProvider

- (NSString*)title {
  return l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PROMO_TITLE);
}

- (NSString*)message {
  // TODO(crbug.com/1363906): Remove mock user data, `userEmail`, below, and
  // instead pass the user email off ChromeIdentity.
  NSString* userEmail = @"elisa@example.test";

  return l10n_util::GetNSStringF(
      IDS_IOS_POST_RESTORE_SIGN_IN_ALERT_PROMO_MESSAGE,
      base::SysNSStringToUTF16(userEmail));
}

#pragma mark - StandardPromoViewProvider

- (PromoStyleViewController*)viewController {
  // TODO(crbug.com/1363906): Fetch user details (name, email, photo) and pass
  // it to PostRestoreSignInViewController's initializer.

  // TODO(crbug.com/1363906): Remove mock user data, `userShortName`, below, and
  // instead pass `userGivenName` off ChromeIdentity.
  NSString* userGivenName = @"Elisa";

  return [[PostRestoreSignInViewController alloc]
      initWithUserGivenName:userGivenName];
}

#pragma mark - StandardPromoActionHandler

// The "Primary Action" was touched.
- (void)standardPromoPrimaryAction {
  // TODO(crbug.com/1363283): Implement `standardPromoPrimaryAction`.
}

// The "Dismiss" button was touched. This same dismiss handler will be used for
// two promo variations:
//
// (Variation #1) A fullscren, FRE-like promo, where the dismiss button says
// "Don't Sign In".
//
// (Variation #2) A native iOS alert promo, where the dismiss button says
// "Cancel".
//
// In both variations, the same dismiss functionality is desired.
- (void)standardPromoDismissAction {
  // TODO(crbug.com/1363283): Implement `standardPromoDismissAction`.
}

@end

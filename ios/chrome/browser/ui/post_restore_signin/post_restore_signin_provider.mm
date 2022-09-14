// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"

#import "base/check_op.h"
#import "base/notreached.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

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

#pragma mark - StandardPromoViewProvider

- (ConfirmationAlertViewController*)viewController {
  // TODO(crbug.com/1363283): Construct and return a
  // ConfirmationAlertViewController.
  return nil;
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

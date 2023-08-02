// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"

#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/post_restore_signin/metrics.h"
#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PostRestoreSignInProvider ()

// Returns the email address of the last account that was signed in pre-restore.
@property(readonly) NSString* userEmail;

// Returns the given name of the last account that was signed in pre-restore.
@property(readonly) NSString* userGivenName;

// Local state is used to retrieve and/or clear the pre-restore identity.
@property(nonatomic, assign) PrefService* localState;

@end

@implementation PostRestoreSignInProvider {
  PromoStyleViewController* _viewController;
  absl::optional<AccountInfo> _accountInfo;
}

#pragma mark - Initializers

- (instancetype)init {
  if (self = [super init]) {
    _localState = GetApplicationContext()->GetLocalState();
    _accountInfo = GetPreRestoreIdentity(_localState);
  }
  return self;
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig([self identifier],
                     &feature_engagement::kIPHiOSPromoPostRestoreFeature);
}

- (void)promoWasDisplayed {
  base::UmaHistogramBoolean(kIOSPostRestoreSigninDisplayedHistogram, true);
}

// Conditionally returns the promo identifier (promos_manager::Promo) based on
// which variation of the Post Restore Sign-in Promo is currently active.
- (promos_manager::Promo)identifier {
  return promos_manager::Promo::PostRestoreSignInAlert;
}

#pragma mark - StandardPromoAlertHandler

- (void)standardPromoAlertDefaultAction {
  [self showSignin];
}

- (void)standardPromoAlertCancelAction {
  base::UmaHistogramEnumeration(kIOSPostRestoreSigninChoiceHistogram,
                                IOSPostRestoreSigninChoice::Dismiss);
  ClearPreRestoreIdentity(_localState);
}

#pragma mark - StandardPromoAlertProvider

- (NSString*)title {
  return l10n_util::GetNSString(IDS_IOS_POST_RESTORE_SIGN_IN_ALERT_PROMO_TITLE);
}

- (NSString*)message {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return l10n_util::GetNSStringF(
        IDS_IOS_POST_RESTORE_SIGN_IN_ALERT_PROMO_MESSAGE_IPAD,
        base::SysNSStringToUTF16(self.userEmail));
  } else {
    return l10n_util::GetNSStringF(
        IDS_IOS_POST_RESTORE_SIGN_IN_ALERT_PROMO_MESSAGE_IPHONE,
        base::SysNSStringToUTF16(self.userEmail));
  }
}

- (NSString*)defaultActionButtonText {
  if (self.userGivenName.length > 0) {
    return l10n_util::GetNSStringF(
        IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PRIMARY_ACTION,
        base::SysNSStringToUTF16(self.userGivenName));
  } else {
    return l10n_util::GetNSString(
        IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PRIMARY_ACTION_SHORT);
  }
}

- (NSString*)cancelActionButtonText {
  return l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_SIGN_IN_ALERT_PROMO_CANCEL_ACTION);
}

#pragma mark - Internal

// Returns the user's pre-restore given name.
- (NSString*)userGivenName {
  if (!_accountInfo.has_value())
    return nil;

  return base::SysUTF8ToNSString(_accountInfo->given_name);
}

// Returns the user's pre-restore email.
- (NSString*)userEmail {
  if (!_accountInfo.has_value())
    return nil;

  return base::SysUTF8ToNSString(_accountInfo->email);
}

// Shows the signin / sync UI flow.
- (void)showSignin {
  DCHECK(self.handler);

  base::UmaHistogramEnumeration(kIOSPostRestoreSigninChoiceHistogram,
                                IOSPostRestoreSigninChoice::Continue);
  ClearPreRestoreIdentity(_localState);

  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kSigninAndSyncReauth
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:nil];
  [self.handler showSignin:command];
}

@end

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"

#import "base/check_op.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/post_restore_signin/metrics.h"
#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PostRestoreSignInProvider ()

// Returns the email address of the last account that was signed in pre-restore.
@property(readonly) NSString* userEmail;

// Returns the given name of the last account that was signed in pre-restore.
@property(readonly) NSString* userGivenName;

// Profile pref used to retrieve and/or clear the pre-restore identity.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation PostRestoreSignInProvider {
  raw_ptr<syncer::SyncUserSettings> _syncUserSettings;
  std::optional<AccountInfo> _accountInfo;
  bool _historySyncEnabled;
  raw_ptr<Browser> _browser;
}

#pragma mark - Initializers

- (instancetype)initForBrowser:(Browser*)browser {
  if ((self = [super init])) {
    _browser = browser;
    _syncUserSettings =
        SyncServiceFactory::GetForProfile(_browser->GetProfile())
            ->GetUserSettings();
    _prefService = browser->GetProfile()->GetPrefs();
    _accountInfo = GetPreRestoreIdentity(_prefService);
    _historySyncEnabled = GetPreRestoreHistorySyncEnabled(_prefService);
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
  base::UmaHistogramEnumeration(kIOSPostRestoreSigninChoiceHistogram,
                                IOSPostRestoreSigninChoice::Continue);
  ClearPreRestoreIdentity(_prefService);

  if ([self isSignedIn]) {
    // The user has signed in after the promo was presented, so sign-in has
    // already completed.
    return;
  }
  [self showSignin];
}

- (void)standardPromoAlertCancelAction {
  base::UmaHistogramEnumeration(kIOSPostRestoreSigninChoiceHistogram,
                                IOSPostRestoreSigninChoice::Dismiss);
  ClearPreRestoreIdentity(_prefService);
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

  __weak __typeof(self) weakSelf = self;
  ShowSigninCommandCompletionCallback callback =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* completionInfo) {
        if (result == SigninCoordinatorResultSuccess) {
          [weakSelf signinDone];
        }
      };
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kResignin
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:callback];
  [self.handler showSignin:command];
}

- (void)signinDone {
  _syncUserSettings->SetSelectedType(syncer::UserSelectableType::kHistory,
                                     _historySyncEnabled);
  _syncUserSettings->SetSelectedType(syncer::UserSelectableType::kTabs,
                                     _historySyncEnabled);
}

// Returns true if the user is signed-in.
- (bool)isSignedIn {
  CoreAccountInfo primaryAccount =
      IdentityManagerFactory::GetForProfile(_browser->GetProfile())
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return !primaryAccount.IsEmpty();
}

@end

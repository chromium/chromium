// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_mediator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_consumer.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The label the bottom sheet should display, or nil if there should be none.
// The label should never promise "sign in to achieve X" if an enterprise
// policy is preventing X, thus the 2 parameters:
//   `sync_transport_disabled_by_policy`: Whether the sync-transport layer got
//   completely nuked by the SyncDisabled policy.
//   `sync_types_disabled_by_policy`: Any syncer::UserSelectableTypes disabled
//   via the SyncTypesListDisabled policy.
// Note: `sync_transport_disabled_by_policy` true is a different product state
// from `sync_types_disabled_by_policy` containing all controllable types,
// because some features are not gated behind a user-controllable toggle, e.g.
// send-tab-to-self. That's why both parameters are required.
NSString* GetPromoLabelString(
    signin_metrics::AccessPoint access_point,
    bool sync_transport_disabled_by_policy,
    syncer::UserSelectableTypeSet sync_types_disabled_by_policy) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
      // Sign-in shouldn't be offered if the feature doesn't work.
      CHECK(!sync_transport_disabled_by_policy);
      return l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_LABEL);
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
      // Configuring feed interests is independent of sync.
      return l10n_util::GetNSString(IDS_IOS_FEED_CARD_SIGN_IN_ONLY_PROMO_LABEL);
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
      // This could check `sync_types_disabled_by_policy` only for the types
      // mentioned in the regular string, but don't bother.
      return sync_transport_disabled_by_policy ||
                     !sync_types_disabled_by_policy.empty()
                 ? l10n_util::GetNSString(
                       IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_LABEL)
                 : l10n_util::GetNSString(
                       IDS_IOS_SIGNIN_SHEET_LABEL_FOR_WEB_SIGNIN);
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
      // This could check `sync_types_disabled_by_policy` only for the types
      // mentioned in the regular string, but don't bother.
      return sync_transport_disabled_by_policy ||
                     !sync_types_disabled_by_policy.empty()
                 ? nil
                 : l10n_util::GetNSString(
                       IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL);
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
      // "Sync" is mentioned in the setup list (the card, not this sheet). So it
      // was easier to hide it than come up with new strings. In the future, we
      // could tweak the card strings and return nil here.
      CHECK(!sync_transport_disabled_by_policy &&
            sync_types_disabled_by_policy.empty());
      return l10n_util::GetNSString(IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL);
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
      // Feed personalization is independent of sync.
      return l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_FEED_PROMO);
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      // Sign-in shouldn't be offered if the feature doesn't work.
      CHECK(!sync_transport_disabled_by_policy &&
            !sync_types_disabled_by_policy.Has(
                syncer::UserSelectableType::kTabs));
      return l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_RECENT_TABS);
    case signin_metrics::AccessPoint::
        ACCESS_POINT_NOTIFICATIONS_OPT_IN_SCREEN_CONTENT_TOGGLE:
      return l10n_util::GetNSString(
          IDS_IOS_NOTIFICATIONS_OPT_IN_SIGN_IN_MESSAGE_CONTENT);
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      // No text.
      return nil;
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK:
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
    case signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
    case signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON:
    case signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAFETY_CHECK:
    case signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAVE_TO_DRIVE_IOS:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAVE_TO_PHOTOS_IOS:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_RESTORE_PRIMARY_ACCOUNT_ON_PROFILE_LOAD:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_ORGANIZATION:
    case signin_metrics::AccessPoint::ACCESS_POINT_TIPS_NOTIFICATION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_CHOICE_REMEMBERED:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PROFILE_MENU_SIGNOUT_CONFIRMATION_PROMPT:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SETTINGS_SIGNOUT_CONFIRMATION_PROMPT:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_IDENTITY_DISC:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_OIDC_REDIRECTION_INTERCEPTION:
    case signin_metrics::AccessPoint::ACCESS_POINT_WEBAUTHN_MODAL_DIALOG:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN_WITH_SYNC_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_PRODUCT_SPECIFICATIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU_FAILED_SWITCH:
    case signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_CCT_ACCOUNT_MISMATCH_NOTIFICATION:
      // Nothing prevents instantiating ConsistencyDefaultAccountViewController
      // with an arbitrary entry point, API-wise. In doubt, no label is a good,
      // generic default that fits all entry points.
      return nil;
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
      NOTREACHED();
  }
}

}  // namespace

@interface ConsistencyDefaultAccountMediator () <
    ChromeAccountManagerServiceObserver> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  signin_metrics::AccessPoint _accessPoint;
  raw_ptr<syncer::SyncService> _syncService;
}

@property(nonatomic, strong) UIImage* avatar;
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

@implementation ConsistencyDefaultAccountMediator

- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                  syncService:(syncer::SyncService*)syncService
                                  accessPoint:
                                      (signin_metrics::AccessPoint)accessPoint {
  if ((self = [super init])) {
    DCHECK(accountManagerService);
    CHECK(syncService);

    _accountManagerService = accountManagerService;
    _syncService = syncService;
    _accessPoint = accessPoint;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
  DCHECK(!_syncService);
}

- (void)disconnect {
  self.accountManagerService = nullptr;
  _syncService = nullptr;
  _accountManagerServiceObserver.reset();
}

#pragma mark - Properties

- (void)setConsumer:(id<ConsistencyDefaultAccountConsumer>)consumer {
  CHECK(_syncService);

  _consumer = consumer;

  syncer::UserSelectableTypeSet disabledTypes;
  syncer::SyncUserSettings* syncSettings = _syncService->GetUserSettings();
  for (syncer::UserSelectableType type :
       syncSettings->GetRegisteredSelectableTypes()) {
    if (syncSettings->IsTypeManagedByPolicy(type)) {
      disabledTypes.Put(type);
    }
  }
  NSString* labelText = GetPromoLabelString(
      _accessPoint,
      _syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY),
      disabledTypes);
  [_consumer setLabelText:labelText];

  NSString* skipButtonText =
      _accessPoint == signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN
          ? l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SKIP)
          : l10n_util::GetNSString(IDS_CANCEL);
  [_consumer setSkipButtonText:skipButtonText];

  [self selectSelectedIdentity];
}

- (void)setSelectedIdentity:(id<SystemIdentity>)identity {
  if ([_selectedIdentity isEqual:identity]) {
    return;
  }
  _selectedIdentity = identity;
  [self updateSelectedIdentityUI];
}

#pragma mark - Private

// Updates the default identity, or hide the default identity if there isn't
// one present on the device.
- (void)selectSelectedIdentity {
  if (!self.accountManagerService) {
    return;
  }

  id<SystemIdentity> identity =
      self.accountManagerService->GetDefaultIdentity();

  // Here, default identity may be nil.
  self.selectedIdentity = identity;
}

// Updates the view controller using the default identity, or hide the default
// identity button if no identity is present on device.
- (void)updateSelectedIdentityUI {
  if (!self.selectedIdentity) {
    [self.consumer hideDefaultAccount];
    return;
  }

  id<SystemIdentity> selectedIdentity = self.selectedIdentity;
  UIImage* avatar = self.accountManagerService->GetIdentityAvatarWithIdentity(
      selectedIdentity, IdentityAvatarSize::TableViewIcon);
  [self.consumer showDefaultAccountWithFullName:selectedIdentity.userFullName
                                      givenName:selectedIdentity.userGivenName
                                          email:selectedIdentity.userEmail
                                         avatar:avatar];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  [self selectSelectedIdentity];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateSelectedIdentityUI];
  }
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40284086): Remove `[self disconnect]`.
  [self disconnect];
}

@end
